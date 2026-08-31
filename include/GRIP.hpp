#ifndef GVIZ_GRIP_HPP
#define GVIZ_GRIP_HPP

#include "BitSet.hpp"
#include "DenseIndex.hpp"
#include "EmbeddedGraph.hpp"
#include "GraphLike.hpp"
#include "KNearest.hpp"
#include "ThreadPool.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace gviz::layout {

/** Construction-time configuration for GRIP. */
struct GRIPConfig {
  /** Per-vertex KNN storage capacity, allocated once at construction. 0
   *  means "use the default" (256). */
  size_t knnCapacity = 256;
  /** When false, no stat series are registered and GRIP::RefineRound()
   *  skips StatAppend. */
  bool statsEnabled = true;
};

/**
 * Large-graph layout via maximal-independent-set (MIS) filtration. Builds a
 * coarse-to-fine hierarchy of the graph, places the coarsest layer as a
 * regular simplex, then refines layer by layer with KNN-spring relaxation
 * -- so cost scales with graph size much better than running a direct
 * force-directed layout on very large inputs.
 *
 * Generic over any `GraphLike G` (typically gviz::Graph or gviz::Subgraph):
 * owns its own `G structure_` plus a `gviz::DenseIndex<G> index_`. Per-
 * vertex decorator state (dec_, dispCalculated_) is sized to `index_.Size()`
 * and addressed by local index. The MIS-filtration machinery itself
 * (misFiltration_/misBorder_ and related scratch state) stays addressed by
 * native/raw vertex handle.
 *
 * Layer numbering: layer 0 is every active vertex (no filtering at all);
 * layer LayerCount()-1 is the coarsest layer, a single (Dim()+1)-vertex
 * simplex. CurrentLayer() starts at LayerCount()-1 after Begin() and
 * decreases toward 0 as NextStage() is called.
 *
 * Drivable one-shot (Embed()) or manually (Begin()/NextStage()/
 * RefineRound(), also reachable as the inherited "grip.nextStage"/
 * "grip.refineRound" actions) for live/animated refinement.
 */
template <GraphLike G>
class GRIP : public EmbeddedGraph {
public:
  /** How placement/refinement neighbor counts are chosen per layer. */
  enum class KPolicy {
    /** Same placementKMax / refinementKMax at every layer. */
    Constant = 0,
    /** maxK >> currentLayer -- smaller on coarse layers (legacy fast path). */
    LayerDecay,
    /** maxK >> depthFromCoarse -- larger on coarse layers. */
    LayerGrow,
    /** Placement uses LayerDecay; refinement stays Constant. */
    PlacementDecay,
    /**
     * Refinement k scales inversely with the active-layer vertex count so
     * the per-round cost stays roughly constant: k = refinementKMax *
     * finestBorder / activeBorder, capped at the KNN capacity. Sparse
     * (coarse/mid) layers get long-range springs, which keeps articulated
     * substructures rigid -- without them a subgraph attached through a
     * single vertex can slowly fold onto the rest of the layout. Placement
     * uses LayerDecay.
     */
    Budget,
  };

  /** Displacement and force statistics from the most recent RefineRound(). */
  struct RoundStats {
    double maxDisplacement = 0.0;
    double meanDisplacement = 0.0;
    /** Mean raw spring-force magnitude before heat scaling. */
    double meanForce = 0.0;
  };

  using Config = GRIPConfig;

  /**
   * Builds GRIP state over @p structure (moved in) in @p dimension
   * dimensions. @p diameter may be 0 if unknown; it only sizes an internal
   * reserve hint for the MIS-filtration layer-border list.
   *
   * @throws DimensionError if @p dimension is not 2, 3, or 4.
   * @throws InsufficientVerticesError if @p structure has fewer than
   * @p dimension + 1 active vertices -- too few to place the coarsest
   * simplex.
   * @throws std::bad_alloc on allocation failure.
   */
  GRIP(G structure, size_t diameter, size_t dimension, Config config = Config{});

  GRIP(const GRIP &) = delete;
  GRIP(GRIP &&) noexcept = default;
  GRIP &operator=(const GRIP &) = delete;
  GRIP &operator=(GRIP &&) = delete;
  ~GRIP() override = default;

  /** The structure this embedder was built over. */
  G &Structure() noexcept { return structure_; }
  const G &Structure() const noexcept { return structure_; }

  // Native-handle position accessors; shadow the base class's local-index
  // versions of the same names.
  double *GetVPosition(size_t handle) noexcept {
    return EmbeddedGraph::GetVPosition(index_.ToLocal(handle));
  }
  const double *GetVPosition(size_t handle) const noexcept {
    return EmbeddedGraph::GetVPosition(index_.ToLocal(handle));
  }
  void SetVPosition(size_t handle, const double *position) noexcept {
    EmbeddedGraph::SetVPosition(index_.ToLocal(handle), position);
  }
  void AddVPosition(size_t handle, const double *position) noexcept {
    EmbeddedGraph::AddVPosition(index_.ToLocal(handle), position);
  }

  /**
   * Configures neighbor counts for placement and refinement; both are
   * clamped to the KNN capacity fixed at construction (Config::knnCapacity).
   * A value of 0 leaves that max unchanged. Defaults after construction:
   * placement/refinement max 128, KPolicy::Constant. Callable at any time.
   */
  void ConfigureK(size_t placementKMax, size_t refinementKMax, KPolicy policy);

  /** Stats from the most recent RefineRound(); zero-valued before the first
   *  round. */
  RoundStats LastRoundStats() const noexcept { return lastRoundStats_; }

  /** Number of MIS-filtration layers built by Begin() (0 before Begin()
   *  runs). Layer 0 is every active vertex; LayerCount()-1 is the coarsest
   *  (simplex) layer. */
  size_t LayerCount() const noexcept { return layerCount_; }

  /** The layer NextStage()/RefineRound() currently operate on. Starts at
   *  LayerCount()-1 after Begin() and decreases toward 0. */
  size_t CurrentLayer() const noexcept { return currLayer_; }

  /** Number of RefineRound() calls made since the current layer was
   *  entered (via Begin() or NextStage()). */
  size_t CurrentRound() const noexcept { return currRound_; }

  /** The KNN storage capacity fixed at construction (Config::knnCapacity). */
  size_t KnnCapacity() const noexcept { return knnCapacity_; }
  /** Current placement neighbor-count cap, as set by ConfigureK(). */
  size_t PlacementKMax() const noexcept { return placementKMax_; }
  /** Current refinement neighbor-count cap, as set by ConfigureK(). */
  size_t RefinementKMax() const noexcept { return refinementKMax_; }
  /** Current K policy, as set by ConfigureK(). */
  KPolicy Policy() const noexcept { return kPolicy_; }

  /**
   * Starts GRIP embedding: builds the MIS filtration (createMISFiltration),
   * places the coarsest layer as a regular simplex, and prepares it for
   * refinement (clears decorators, computes its KNN lists, syncs the draw
   * mask).
   */
  void Begin();

  /**
   * Advances to the next finer layer and places its vertices (barycenter of
   * their nearest already-visible neighbors). No-op at layer 0. Also
   * exposed as the "grip.nextStage" action.
   */
  void NextStage();

  /**
   * Runs one force-directed refinement round on the current layer: spring
   * forces from KNN lists (and, at layer 0, real graph edges too), adaptive
   * per-vertex heat, net-translation/rotation removal, then commits
   * displacements to positions and updates LastRoundStats(). Also exposed
   * as the "grip.refineRound" action.
   */
  void RefineRound();

  /**
   * Runs the full GRIP embedding pipeline: Begin(), then per layer a bounded
   * number of refinement rounds (stopping early once the max displacement
   * settles below a small fraction of the target edge length), advancing
   * until the finest layer (layer 0) has been refined. One-shot equivalent
   * of driving Begin()/RefineRound()/NextStage() manually.
   */
  void Embed();

  // DEBUG / INTROSPECTION: ------------------------------------------------
  //
  // Narrow windows into MIS-filtration state for tests/embedders/*.cpp
  // benchmark and probe tools only. Ordinary callers (front-ends driving
  // GRIP through Begin/NextStage/RefineRound/Embed) should never need any
  // of these.

  /** Exclusive-end index into the filtration order (see FiltrationVertexAt)
   *  for MIS layer @p layer. Unchecked. */
  size_t LayerBorder(size_t layer) const noexcept { return misBorder_[layer]; }

  /** Raw vertex handle at position @p i in the MIS-filtration order.
   *  Unchecked: @p i must be < Structure()'s vertex count. */
  size_t FiltrationVertexAt(size_t i) const noexcept { return misFiltration_[i]; }

  /** Vertex @p v's (raw handle) displacement vector from the most recent
   *  RefineRound() (Dim() doubles) -- all-zero before its layer's first
   *  round. Unchecked: @p v must be a handle Structure() actually has. */
  std::span<const double> Displacement(size_t v) const noexcept {
    return dec_[index_.ToLocal(v)].disp;
  }

  /** Disables the internal worker pool, forcing every subsequent parallel
   *  phase (placement, KNN refresh, refinement) onto the calling thread.
   *  One-way (no re-enable). Debug/benchmark use only. */
  void DebugDisableThreadPool() noexcept { pool_.reset(); }

  /** Runs MakeFirstMISPartition: builds the finest MIS layer into @p out
   *  and records its border. @p out must be a zeroed BitSet sized to
   *  GraphLikeVertexCapacity(Structure()). For tools that drive the
   *  filtration one layer at a time instead of through Begin(). */
  void DebugMakeFirstMISPartition(BitSet &out) { MakeFirstMISPartition(out); }

  /** Runs one coarsening step of the MIS filtration: coarsens @p vertices
   *  into layer @p i in place and records its border. Returns true while
   *  further coarsening is possible. */
  bool DebugIterMISFiltration(size_t i, BitSet &vertices) {
    return IterMISFiltration(i, vertices);
  }

  /**
   * Builds the MIS filtration up to (but not including) the final top-off
   * loop that pulls extra vertices into the coarsest layer when it has
   * fewer than Dim() + 1 members, so a benchmark can checkpoint the
   * pre-migrate state and compare different top-off strategies from the
   * identical starting point.
   *
   * @return the layer index the migration loop should run at -- pass it
   * (or layerIndex - 1, per each accessor's own doc) to LayerBorder,
   * DebugMigrateOneToFinalLayer, and DebugApplyMigration.
   */
  size_t DebugBuildFiltrationPreMigrate();

  /** Read-only copies of the filtration order and layer borders, for
   *  checkpointing around a DebugBuildFiltrationPreMigrate call. */
  std::vector<size_t> DebugSnapshotFiltration() const { return misFiltration_; }
  std::vector<size_t> DebugSnapshotBorders() const { return misBorder_; }

  /** Restores a snapshot taken by DebugSnapshotFiltration/
   *  DebugSnapshotBorders. Unchecked: @p filtration.size() must equal
   *  Structure()'s vertex count and @p borders must be a valid border
   *  sequence for it. */
  void DebugRestoreFiltration(std::vector<size_t> filtration, std::vector<size_t> borders) {
    misFiltration_ = std::move(filtration);
    misBorder_ = std::move(borders);
  }

  /**
   * Pulls one vertex from a shallower layer's drop set into the coarsest
   * layer, preferring the farthest reachable candidate within a capped
   * local BFS. @p layerIndex is the coarsest layer's border index
   * (LayerBorder(layerIndex) is what advances by one on success).
   *
   * @return false when every shallower pool is exhausted.
   */
  bool DebugMigrateOneToFinalLayer(size_t layerIndex) {
    return MigrateOneToFinalLayer(layerIndex + 1);
  }

  /**
   * Raw primitive both migration strategies reduce to: swaps
   * FiltrationVertexAt(candidateIndex) into the slot at
   * LayerBorder(layerIndex) and advances that border by one.
   */
  void DebugApplyMigration(size_t layerIndex, size_t candidateIndex) noexcept {
    std::swap(misFiltration_[candidateIndex], misFiltration_[misBorder_[layerIndex]]);
    misBorder_[layerIndex]++;
  }

private:
  /** Per-vertex working state, local-indexed. knn is sized to knnCapacity_
   *  once and only [0, knnCount) holds live entries. */
  struct Decorators {
    std::vector<search::FoundVertex> knn;
    size_t knnCount = 0;
    std::vector<double> disp;
    std::vector<double> oldDisp;
    double oldCos = 0.0;
    double heat = 0.0;
    /** Raw force magnitude from the latest force evaluation (pre heat
     *  scaling). */
    double lastForceMag = 0.0;
  };

  static void ActionRefineRound(EmbeddedGraph &embedding, void *userData,
                                 const ActionPayload &payload);
  static void ActionNextStage(EmbeddedGraph &embedding, void *userData,
                               const ActionPayload &payload);

  void SyncDrawMask();
  const BitSet &VisibleVertices() const noexcept {
    return GetDrawMask().visibleVertices;
  }

  /** Local-indexed decorator lookup for raw handle @p v. */
  Decorators &Dec(size_t v) noexcept { return dec_[index_.ToLocal(v)]; }

  search::KNearestScratch &KnnScratchForCaller();
  size_t ComputeK(size_t maxK, bool forPlacement) const;
  size_t PlacementK() const;
  size_t RefinementK() const;

  size_t MisBorderAt(size_t i) const noexcept { return misBorder_[i]; }

  void Barycenter(std::span<const search::FoundVertex> neighbors,
                   double *out) const;

  /** Runs @p task over [begin, end) via the worker pool if one exists,
   *  falling back to one synchronous call on the calling thread otherwise. */
  void RunForRange(size_t begin, size_t end, size_t grain,
                    const std::function<void(size_t, size_t)> &task);

  void MakeFirstMISPartition(BitSet &out);
  bool IterMISFiltration(size_t i, BitSet &vertices);
  size_t CreateMISFiltration();
  size_t PickFarCandidate(size_t finalEnd, size_t candBegin, size_t candEnd,
                           size_t maxDepth);
  bool MigrateOneToFinalLayer(size_t count);
  void VerticesWithinRadius(size_t source, size_t maxDepth, BitSet &out);

  void PlaceVertexRange(size_t begin, size_t end);
  void PlaceLayerVertices();
  void UpdateLocalTemp(size_t v);
  void CalculateSpringForces(size_t v, size_t layer);
  void ClearDecorators();
  void UpdateKNNRange(size_t begin, size_t end);
  void UpdateKNNs();
  void RefinementPass1Range(size_t begin, size_t end);
  void RemoveNetTranslation();
  void RemoveNetRotationPlane(size_t a, size_t b, const double *com,
                               size_t count);
  void RemoveNetRotation();

  G structure_;
  DenseIndex<G> index_;

  size_t knnCapacity_;
  size_t placementKMax_;
  size_t refinementKMax_;
  KPolicy kPolicy_;
  bool statsEnabled_;

  size_t layerCount_ = 0;
  size_t currLayer_ = 0;
  size_t currRound_ = 0;
  RoundStats lastRoundStats_{};

  /** misFiltration[i]: raw vertex handle at MIS-filtration array position
   *  i. Sized to index_.Size(). */
  std::vector<size_t> misFiltration_;
  /** misBorder_[layer]: exclusive end index into misFiltration_ for that
   *  layer's vertex range. */
  std::vector<size_t> misBorder_;
  /** Local-indexed, sized to index_.Size(). */
  std::vector<Decorators> dec_;
  /** Whether a vertex has had a displacement computed yet this layer.
   *  Local-indexed, sized to index_.Size(). */
  BitSet dispCalculated_;

  /** Depth reached by the most recent radius-BFS (VerticesWithinRadius /
   *  PickFarCandidate), valid for raw handle v exactly when
   *  radiusBfsScratch_.Visited(v, <that BFS's epoch>) is true. Raw/
   *  capacity-addressed, same as radiusBfsScratch_ whose epoch/visited half
   *  this reuses. */
  std::vector<size_t> radiusBfsDepth_;
  search::KNearestScratch radiusBfsScratch_;

  /** Worker pool for data-parallel phases (placement, KNN refresh,
   *  refinement). Null => every parallel phase runs serially on the calling
   *  thread instead. */
  std::unique_ptr<ThreadPool> pool_;
  /** One KNN scratch buffer per pool worker plus one for the caller thread
   *  (or just one, serial fallback). Capacity-addressed, same as
   *  radiusBfsScratch_. */
  size_t knnScratchCount_ = 1;
  std::vector<search::KNearestScratch> knnScratch_;
};

} // namespace gviz::layout

#endif
