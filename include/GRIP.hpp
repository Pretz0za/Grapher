#ifndef GVIZ_GRIP_HPP
#define GVIZ_GRIP_HPP

#include "BitSet.hpp"
#include "EmbeddedGraph.hpp"
#include "KNearest.hpp"
#include "Subgraph.hpp"
#include "ThreadPool.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace gviz::layout {

class GRIP;

/**
 * Construction-time configuration for GRIP. Design note (deviation from the
 * old C API): gvizGRIPEmbedderConfigureKnnCapacity and
 * gvizGRIPEmbedderConfigureStats were documented as "must be called on a
 * zero-initialized state before Init; no effect afterward" -- they size
 * storage (the per-vertex KNN buffers) and decide whether stat series exist
 * at all, both one-time decisions baked in at Init. That "mutate-before-Init"
 * shape doesn't map onto throw-from-constructor RAII (there is no
 * zeroed-but-not-yet-constructed GRIP to call setters on), so both become
 * fields of this struct, passed once to GRIP's constructor, instead of
 * post-construction setters. gvizGRIPEmbedderConfigureK (placement/
 * refinement k and the policy) is different: reading the old
 * gvizGRIPEmbedderInit, it unconditionally resets placementKMax/
 * refinementKMax/kPolicy to their defaults via memset, silently discarding
 * any pre-Init ConfigureK call -- so in the original code that function only
 * ever took effect when called AFTER Init, not before. It has no
 * construction-time constraint to preserve, so it stays an ordinary
 * post-construction mutator on GRIP instead: see GRIP::ConfigureK().
 *
 * Deliberately a free struct rather than a type nested inside GRIP (where
 * GRIP::Config might read more discoverably): a nested class's default
 * member initializers can't be used in a default argument of the enclosing
 * class's OWN member function declarations (the enclosing class isn't
 * "complete" yet at that point, even though the nested class already is --
 * a real, if obscure, C++ rule that clang enforces) -- exactly the shape
 * GRIP's constructor needs (`Config config = Config{}`). GRIP still exposes
 * this as `GRIP::Config` via a type alias for a discoverable, conventional
 * name at call sites.
 */
struct GRIPConfig {
  /** Per-vertex KNN storage capacity, allocated once at construction. 0
   *  means "use the default" (256), matching gvizGRIPEmbedderInit's
   *  handling of a zero knnCapacity request. */
  size_t knnCapacity = 256;
  /** When false, no gvizStatSeries-equivalent series are registered and
   *  GRIP::RefineRound() skips StatAppend (zero chart overhead). */
  bool statsEnabled = true;
};

/**
 * Direct port of gvizGRIPEmbedder.h/.c: large-graph layout via
 * maximal-independent-set (MIS) filtration. Builds a coarse-to-fine
 * hierarchy of the graph (createMISFiltration in the old C), places the
 * coarsest layer as a regular simplex, then refines layer by layer with
 * KNN-spring relaxation -- so cost scales with graph size much better than
 * running a direct force-directed layout on very large inputs. See
 * CLAUDE.md's GRIP row for the conceptual overview; this header only
 * documents the C++-specific shape.
 *
 * Layer numbering (unchanged from the old C, see createMISFiltration's doc
 * below): layer 0 is every active vertex (no filtering at all); layer
 * LayerCount()-1 is the coarsest layer, a single (Dim()+1)-vertex simplex.
 * CurrentLayer() starts at LayerCount()-1 after Begin() and decreases toward
 * 0 as NextStage() is called.
 *
 * Drivable one-shot (Embed()) or manually (Begin()/NextStage()/RefineRound(),
 * also reachable as the inherited "grip.nextStage"/"grip.refineRound"
 * actions) for live/animated refinement, exactly like the old C API.
 */
class GRIP : public EmbeddedGraph {
public:
  /** How placement/refinement neighbor counts are chosen per layer. Direct
   *  port of gvizGRIPKPolicy. */
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

  /** Displacement and force statistics from the most recent RefineRound().
   *  Rename of gvizGRIPRoundStats. */
  struct RoundStats {
    double maxDisplacement = 0.0;
    double meanDisplacement = 0.0;
    /** Mean raw spring-force magnitude before heat scaling. */
    double meanForce = 0.0;
  };

  /** Discoverable alias for GRIPConfig (see its own doc comment for why it
   *  is a free struct rather than nested here directly). */
  using Config = GRIPConfig;

  /**
   * Builds GRIP state over @p subgraph in @p dimension dimensions. @p
   * diameter may be 0 if unknown; it only sizes an internal reserve hint for
   * the MIS-filtration layer-border list (same role as the old C
   * gvizGRIPEmbedderInit's diameter parameter).
   *
   * @throws DimensionError if @p dimension is not 2, 3, or 4.
   * @throws InsufficientVerticesError if @p subgraph has fewer than
   * @p dimension + 1 active vertices -- too few to place the coarsest
   * simplex. Checked before the (potentially large) base EmbeddedGraph
   * allocation runs, so a rejected construction leaves nothing behind to
   * unwind.
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  GRIP(Subgraph subgraph, size_t diameter, size_t dimension,
       Config config = Config{});

  // Polymorphic base (EmbeddedGraph) forbids copy and move-assignment for
  // the same reason documented there (Subgraph's `const Graph&` member can't
  // be reseated); GRIP follows the identical shape.
  GRIP(const GRIP &) = delete;
  GRIP(GRIP &&) noexcept = default;
  GRIP &operator=(const GRIP &) = delete;
  GRIP &operator=(GRIP &&) = delete;
  ~GRIP() override = default;

  /**
   * Configures neighbor counts for placement and refinement; both are
   * clamped to the KNN capacity fixed at construction (Config::knnCapacity).
   * A value of 0 leaves that max unchanged. Defaults after construction:
   * placement/refinement max 128, KPolicy::Constant. Callable at any time
   * (see Config's doc comment on why this one isn't construction-only).
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
   * mask). Also exposed as the "grip.nextStage"-sibling action set up by the
   * constructor is NOT required before calling Begin() directly -- Begin()
   * itself is not an action (there is no natural payload for "start over"),
   * matching the old C gvizGRIPEmbedderBegin, which was likewise only ever
   * called directly by a driver, never through the action registry.
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
   * of driving Begin()/RefineRound()/NextStage() manually. Matches the old
   * gvizGRIPEmbedderEmbed's early-stopping behavior exactly.
   */
  void Embed();

  // DEBUG / INTROSPECTION: ------------------------------------------------
  //
  // Narrow windows into MIS-filtration state for tests/embedders/*.cpp
  // benchmark and probe tools only (gvizGRIPKBench/LayerProbe/MigrateBench/
  // FiltrationDebug/PlaceBench's C++ ports) -- the white-box access those
  // tools got for free in C via gvizGRIPInternal.h/gvizGRIPState's public
  // fields. Ordinary callers (front-ends driving GRIP through Begin/
  // NextStage/RefineRound/Embed) should never need any of these; each one
  // exists because a specific old-C tool read (or, for the Debug*Migration/
  // Debug*Filtration group, mutated) exactly that piece of state and has no
  // other way to reach it now that GRIP's internals are private (see the
  // class doc above on why there is no second "internal" header in C++).
  //
  // LayerBorder/FiltrationVertexAt/Displacement are plain read-only
  // passthroughs, safe for any caller. The Debug*-prefixed group below them
  // is a different, more invasive kind of surface: DebugBuildFiltrationPreMigrate/
  // DebugSnapshotFiltration/DebugSnapshotBorders/DebugRestoreFiltration/
  // DebugMigrateOneToFinalLayer/DebugApplyMigration exist ONLY so
  // GRIPMigrateBench can checkpoint the filtration mid-build and compare the
  // production bounded-BFS migration step against an alternate
  // (intentionally unbounded, historical baseline) algorithm implemented
  // entirely at the call site -- that comparison is the tool's whole
  // purpose, and there is no way to offer it without exposing both a
  // mutation primitive and a checkpoint/restore pair. This is broader
  // surface than any other embedder's Debug accessors in this port; flagged
  // here rather than silently folded in with the read-only group above.

  /** Exclusive-end index into the filtration order (see FiltrationVertexAt)
   *  for MIS layer @p layer -- misBorder_[layer] in the old C's naming, and
   *  the direct equivalent of the old C tools' `gripMisBorderAt` helper.
   *  Unchecked: @p layer must address an already-recorded border (< the
   *  number of Begin()/DebugMakeFirstMISPartition/DebugIterMISFiltration/
   *  DebugBuildFiltrationPreMigrate calls made so far, plus one). */
  size_t LayerBorder(size_t layer) const noexcept { return misBorder_[layer]; }

  /** Raw vertex id at position @p i in the MIS-filtration order
   *  (misFiltration_[i] in the old C's naming). Unchecked: @p i must be
   *  < Structure().VertexCount(). */
  size_t FiltrationVertexAt(size_t i) const noexcept { return misFiltration_[i]; }

  /** Vertex @p v's displacement vector from the most recent RefineRound()
   *  (Dim() doubles) -- all-zero before its layer's first round. Unchecked:
   *  @p v must be < Structure().VertexCapacity(). */
  std::span<const double> Displacement(size_t v) const noexcept { return dec_[v].disp; }

  /** Disables the internal worker pool, forcing every subsequent parallel
   *  phase (placement, KNN refresh, refinement) onto the calling thread.
   *  One-way (no re-enable). Matches the old C benchmarking pattern of
   *  destroying gvizGRIPState::pool right after Init for a reproducible
   *  single-threaded run (see GRIPKBench). Debug/benchmark use only. */
  void DebugDisableThreadPool() noexcept { pool_.reset(); }

  /** Runs MakeFirstMISPartition: builds the finest MIS layer into @p out
   *  and records its border. @p out must be a zeroed BitSet sized to
   *  Structure().VertexCapacity(). For tools (GRIPFiltrationDebug) that
   *  drive the filtration one layer at a time instead of through Begin();
   *  see DebugBuildFiltrationPreMigrate for the "run it to completion and
   *  hand back just the layer index" equivalent Begin() itself uses. */
  void DebugMakeFirstMISPartition(BitSet &out) { MakeFirstMISPartition(out); }

  /** Runs one coarsening step of the MIS filtration: coarsens @p vertices
   *  into layer @p i in place and records its border. Returns true while
   *  further coarsening is possible (same contract as the old C
   *  iterMISFiltration). Same step-by-step-driving use case as
   *  DebugMakeFirstMISPartition. */
  bool DebugIterMISFiltration(size_t i, BitSet &vertices) {
    return IterMISFiltration(i, vertices);
  }

  /**
   * Builds the MIS filtration up to (but not including) the final top-off
   * loop that pulls extra vertices into the coarsest layer when it has
   * fewer than Dim() + 1 members (see DebugMigrateOneToFinalLayer) -- the
   * same work CreateMISFiltration() does, minus that last step, so a
   * benchmark can checkpoint the pre-migrate state and compare different
   * top-off strategies from the identical starting point. Direct port of
   * the old C GRIPMigrateBench.c's file-local buildFiltrationPreMigrate.
   *
   * @return the layer index the migration loop should run at -- pass it
   * (or layerIndex - 1, per each accessor's own doc) to LayerBorder,
   * DebugMigrateOneToFinalLayer, and DebugApplyMigration.
   */
  size_t DebugBuildFiltrationPreMigrate();

  /** Read-only copies of the filtration order and layer borders, for
   *  checkpointing around a DebugBuildFiltrationPreMigrate call so a
   *  benchmark can run two different migration strategies from the same
   *  starting state (see DebugRestoreFiltration). */
  std::vector<size_t> DebugSnapshotFiltration() const { return misFiltration_; }
  std::vector<size_t> DebugSnapshotBorders() const { return misBorder_; }

  /** Restores a snapshot taken by DebugSnapshotFiltration/
   *  DebugSnapshotBorders. Unchecked: @p filtration.size() must equal
   *  Structure().VertexCapacity() and @p borders must be a valid border
   *  sequence for it (as produced by an earlier DebugBuildFiltrationPreMigrate
   *  run over the same subgraph). */
  void DebugRestoreFiltration(std::vector<size_t> filtration, std::vector<size_t> borders) {
    misFiltration_ = std::move(filtration);
    misBorder_ = std::move(borders);
  }

  /**
   * Runs the production bounded-BFS migration step (the old C
   * migrateOneToFinalLayer) that pulls one vertex from a shallower layer's
   * drop set into the coarsest layer, preferring the farthest reachable
   * candidate within a capped local BFS. @p layerIndex is the coarsest
   * layer's border index (LayerBorder(layerIndex) is what advances by one
   * on success) -- one less than the old C function's `count` parameter.
   *
   * @return false when every shallower pool is exhausted (see
   * MigrateOneToFinalLayer's doc for why that's unreachable in practice).
   */
  bool DebugMigrateOneToFinalLayer(size_t layerIndex) {
    return MigrateOneToFinalLayer(layerIndex + 1);
  }

  /**
   * Raw primitive both migration strategies reduce to: swaps
   * FiltrationVertexAt(candidateIndex) into the slot at
   * LayerBorder(layerIndex) and advances that border by one. Exposed so an
   * alternative migration algorithm -- e.g. GRIPMigrateBench's historical
   * full-BFS-per-candidate baseline, implemented entirely at the call site
   * using Structure()/search::BreadthFirst plus FiltrationVertexAt/
   * LayerBorder to pick a candidate -- can apply its pick the same way
   * DebugMigrateOneToFinalLayer's production algorithm does internally,
   * without reaching into private state to do it.
   */
  void DebugApplyMigration(size_t layerIndex, size_t candidateIndex) noexcept {
    std::swap(misFiltration_[candidateIndex], misFiltration_[misBorder_[layerIndex]]);
    misBorder_[layerIndex]++;
  }

private:
  /** Per-vertex working state. Direct port of gvizGRIPDecorators: knn/disp/
   *  oldDisp become owned std::vectors (one small allocation per vertex)
   *  instead of slices of three big manually-managed contiguous blocks --
   *  simpler ownership at the cost of N allocations instead of O(1); GRIP
   *  has no hot-path perf constraint on par with Graph/Subgraph's, so this
   *  trade favors clarity. knn is sized to knnCapacity_ once (like the old
   *  fixed-capacity gvizArray) and only [0, knnCount) holds live entries. */
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

  search::KNearestScratch &KnnScratchForCaller();
  size_t ComputeK(size_t maxK, bool forPlacement) const;
  size_t PlacementK() const;
  size_t RefinementK() const;

  size_t MisBorderAt(size_t i) const noexcept { return misBorder_[i]; }

  void Barycenter(std::span<const search::FoundVertex> neighbors,
                   double *out) const;

  /** Runs @p task over [begin, end) via the worker pool if one exists,
   *  falling back to one synchronous call on the calling thread otherwise
   *  -- the same NULL-pool serial fallback gvizThreadPoolForRange gave a
   *  NULL gvizThreadPool*. */
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

  size_t knnCapacity_;
  size_t placementKMax_;
  size_t refinementKMax_;
  KPolicy kPolicy_;
  bool statsEnabled_;

  size_t layerCount_ = 0;
  size_t currLayer_ = 0;
  size_t currRound_ = 0;
  RoundStats lastRoundStats_{};

  /** misFiltration[i]: raw vertex id at MIS-filtration array position i.
   *  Sized to Structure().VertexCapacity(). Direct port of
   *  gvizGRIPState::misFiltration. */
  std::vector<size_t> misFiltration_;
  /** misBorder_[layer]: exclusive end index into misFiltration_ for that
   *  layer's vertex range. Direct port of gvizGRIPState::misBorder
   *  (was a gvizArray; grows the same way a std::vector does). */
  std::vector<size_t> misBorder_;
  std::vector<Decorators> dec_;
  /** Whether a vertex has had a displacement computed yet this layer (its
   *  heat needs a first-touch initial value instead of the adaptive
   *  update). Direct port of gvizGRIPState::dispCalculated. */
  BitSet dispCalculated_;

  /** Depth reached by the most recent radius-BFS (VerticesWithinRadius /
   *  PickFarCandidate), valid for vertex v exactly when
   *  radiusBfsScratch_.Visited(v, <that BFS's epoch>) is true. Design note:
   *  the old C packed epoch and depth into one stamp word per vertex
   *  (gripMigrateStampVisit/Visited/Depth); here the epoch/visited half is
   *  reused from search::KNearestScratch (its stamp array + queue are
   *  exactly this shape already, so GRIP doesn't hand-roll a second
   *  epoch-stamped BFS scratch next to KNearest's), and only the payload
   *  half (depth) needs its own parallel array. */
  std::vector<size_t> radiusBfsDepth_;
  search::KNearestScratch radiusBfsScratch_;

  /** Worker pool for data-parallel phases (placement, KNN refresh,
   *  refinement). Null => every parallel phase runs serially on the calling
   *  thread instead -- deliberately tolerated (see the constructor), same
   *  as the old C's NULL gvizThreadPool* fallback. */
  std::unique_ptr<ThreadPool> pool_;
  /** One KNN scratch buffer per pool worker plus one for the caller thread
   *  (or just one, serial fallback). */
  size_t knnScratchCount_ = 1;
  std::vector<search::KNearestScratch> knnScratch_;
};

} // namespace gviz::layout

#endif
