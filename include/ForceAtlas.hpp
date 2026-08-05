#ifndef GVIZ_FORCEATLAS_HPP
#define GVIZ_FORCEATLAS_HPP

#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "ForceModel.hpp"
#include "QuadTree.hpp"
#include "Subgraph.hpp"
#include "ThreadPool.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace gviz::layout {

/**
 * Barnes-Hut force-directed layout: repulsion applies between every pair of
 * vertices and is approximated by walking a QuadTree over the current
 * positions and treating distant subtrees as a single pseudo-body at their
 * center of mass; attraction is computed additionally, on top of that
 * repulsion, exactly along real graph edges (O(E) per round). Direct port of
 * the old C gvizForceEmbedderState/gvizForceEmbedder* free functions
 * (embedders/gvizForceEmbedder.h) onto real inheritance from EmbeddedGraph.
 *
 * The actual force math (attraction, repulsion, and the mass a vertex
 * contributes to quadtree aggregation) is pluggable via a ForceModel fixed
 * at construction; everything else here (speed regulation, Barnes-Hut
 * traversal, actions, stat series) is shared across models.
 * SetBarnesHutEnabled(false) switches to exact O(V^2) all-pairs repulsion
 * instead of the scalable quadtree default -- call before Begin(), same as
 * the old C contract (toggling mid-run is unsupported: see Step()'s use of
 * the quadtree).
 *
 * Dimension is hard-locked to 2 (repulsion needs a 2D QuadTree); the
 * constructor throws DimensionError otherwise.
 *
 * The one embedder with real dynamic-graph physics catch-up: Sync() layers
 * new-vertex placement and a full degree/mass recompute on top of the
 * inherited EmbeddedGraph::Sync()'s structural commit. Sync() intentionally
 * hides (shadows) the base class's Sync() by name -- EmbeddedGraph's own
 * methods are ordinary, non-virtual functions by design (see its class
 * comment), so a derived embedder that needs extra catch-up work provides
 * its own same-named entry point that calls the base one first, exactly as
 * the old gvizForceEmbedderSync layered on top of gvizEmbeddedGraphSync.
 * Calling EmbeddedGraph::Sync() explicitly on a ForceAtlas (as this class's
 * own Sync() does internally) skips that catch-up and is available to
 * anyone holding a plain `EmbeddedGraph &`/`EmbeddedGraph *` -- exactly the
 * same escape hatch the old C code had via the free function pair.
 *
 * Threading: force evaluation, swinging/traction, and speed application are
 * data-parallel across vertices via an owned ThreadPool (see the .cpp for
 * why it's owned unconditionally rather than the old C's maybe-null,
 * fall-back-to-serial pool).
 */
class ForceAtlas : public EmbeddedGraph {
public:
  static constexpr double kEdgeLengthDefault = 10.0;
  static constexpr double kThetaDefault = 1.0;
  static constexpr double kOverlapConstantDefault = 100.0;
  static constexpr double kJitterToleranceDefault = 1.0;
  static constexpr double kSpeedInitial = 1.0;
  static constexpr double kSpeedEfficiencyInitial = 1.0;
  static constexpr double kSpeedEfficiencyMin = 0.05;
  static constexpr double kSpeedEfficiencyMax = 1.0;
  static constexpr double kSpeedMaxRise = 0.5;

  /**
   * Initializes a Barnes-Hut force layout over @p subgraph (moved in) with
   * force math supplied by @p model (moved in; fixed for the object's
   * lifetime, like @p dimension). Runs the embedding's first structural
   * commit (EmbeddedGraph::Sync()) to build the synced adjacency CSRs the
   * physics reads, registers actions "forceEmbedder.step" and
   * "forceEmbedder.toggleOverlapPrevention", and registers stat series
   * "forceEmbedder.maxDisp", "forceEmbedder.speed",
   * "forceEmbedder.attractiveForce", "forceEmbedder.repulsiveForce", and
   * "forceEmbedder.gravityForce". Edge length and box extent start at
   * sensible defaults; override with Configure()/ConfigureSpeed()/
   * ConfigureBarnesHut()/SetBarnesHutEnabled()/ConfigureGravity()/
   * ConfigureRadius()/ConfigureOverlapPrevention()/
   * SetPreventOverlapEnabled() before calling Begin(). The quadtree itself
   * is not built until Begin(), since positions are all zero right after
   * construction.
   *
   * @p subgraph's graph may be directed or undirected; direction only
   * affects which of OutNeighbors/InNeighbors attraction walks (see
   * EmbeddedGraph), never repulsion/gravity/speed regulation. For a graph
   * that will grow while animated, pass a VERTEX-INDUCED subgraph (see
   * EmbeddedGraph.hpp's GROWTH & SYNC section).
   *
   * @throws DimensionError if @p dimension != 2. @throws std::bad_alloc (or
   * std::system_error, from the owned ThreadPool) on allocation/thread
   * startup failure -- no manual -1 return path.
   */
  ForceAtlas(Subgraph subgraph, size_t dimension, std::unique_ptr<ForceModel> model);

  ForceAtlas(const ForceAtlas &) = delete;
  ForceAtlas(ForceAtlas &&) noexcept = default;
  ForceAtlas &operator=(const ForceAtlas &) = delete;
  ForceAtlas &operator=(ForceAtlas &&) = delete;
  ~ForceAtlas() override = default;

  // CONFIGURATION (call before Begin(), unless noted otherwise): -----------

  /** Overrides the target edge length and the half-width of the random
   *  initial placement box. Pass 0 for either to keep its current/default
   *  value. */
  void Configure(double edgeLength, double boxExtent);

  /** Overrides the jitter tolerance used by Step()'s global speed update:
   *  how much global swinging is tolerated relative to global traction
   *  before the global speed backs off. Pass 0 to keep the current/default
   *  value. */
  void ConfigureSpeed(double tolerance);

  /** Overrides the Barnes-Hut opening-angle threshold @p theta (a subtree is
   *  approximated as one pseudo-body when side/distance < theta) and the
   *  quadtree's @p nodesPerCell. Pass 0 for either to keep its
   *  current/default value; @p nodesPerCell affects the quadtree built in
   *  Begin(). */
  void ConfigureBarnesHut(double theta, size_t nodesPerCell);

  /** Enables or disables the Barnes-Hut quadtree approximation of repulsion
   *  (enabled by construction). Disabled, Step() computes repulsion by exact
   *  all-pairs evaluation instead. Call before Begin(); toggling after is
   *  unsupported (see the class comment). */
  void SetBarnesHutEnabled(bool enabled) noexcept { barnesHutEnabled_ = enabled; }

  /** Whether Barnes-Hut approximation is currently enabled. */
  bool BarnesHutEnabled() const noexcept { return barnesHutEnabled_; }

  /** Sets the gravity constant: every vertex feels a constant-magnitude pull
   *  of @p k toward the origin. @p k = 0 (the default) disables gravity.
   *  Unlike the other Configure* methods, @p k is always assigned
   *  unconditionally -- 0 is gravity's legitimate off value. Gravity is
   *  deliberately excluded from swinging/traction (see structForce_) since
   *  it never settles to zero. */
  void ConfigureGravity(double k) noexcept { gravityK_ = k; }

  /** Sets the radius formula r(v) = @p base * (1 + @p perDegree *
   *  sqrt(degree(v))) used by repulsion when overlap prevention is enabled.
   *  Always assigned unconditionally (0 for either is a legitimate "no
   *  radius" configuration). Only affects repulsion once overlap prevention
   *  is enabled; safe to call any time. */
  void ConfigureRadius(double base, double perDegree) noexcept {
    radiusBase_ = base;
    radiusPerDegree_ = perDegree;
  }

  /** Overrides the overlap constant (Gephi ForceAtlas2's "Prevent Overlap"
   *  constant, ~100 by default): once two vertices' circles touch or
   *  overlap, repulsion magnitude holds at this many times the model's
   *  ordinary term instead of diverging. Pass 0 to keep the current/default
   *  value. Only takes effect once overlap prevention is enabled. */
  void ConfigureOverlapPrevention(double constant);

  /** Enables or disables treating vertices as circles of radius
   *  VertexRadius() (rather than dimensionless points) for repulsion.
   *  Disabled (the default) behaves as if no radius had ever been
   *  configured. Safe to call at any time, including mid-simulation -- see
   *  the old C header's note on why that's the typical usage (mirrors
   *  Gephi's own UI checkbox). Also exposed as the
   *  "forceEmbedder.toggleOverlapPrevention" action for live toggling from a
   *  bound key. */
  void SetPreventOverlapEnabled(bool enabled) noexcept { preventOverlap_ = enabled; }

  /** Whether overlap prevention is currently enabled. */
  bool PreventOverlapEnabled() const noexcept { return preventOverlap_; }

  /** Vertex @p index's (compact local index, not a raw graph vertex id)
   *  radius per the formula set by ConfigureRadius(), computed from
   *  degree_[index] on demand rather than stored. Meaningful regardless of
   *  whether overlap prevention is enabled. Unchecked: @p index must be <
   *  the current active vertex count. */
  double VertexRadius(size_t index) const noexcept;

  // LIFECYCLE: ---------------------------------------------------------------

  /**
   * Places every active vertex uniformly at random inside the
   * [-boxExtent, boxExtent]^2 box, resets the global speed/speed efficiency
   * and previous-round force history, and (re)builds the quadtree over the
   * freshly randomized positions. @p seed seeds the generator; pass 0 (the
   * default) for a time-based seed. Safe to call again to restart the
   * layout.
   *
   * @throws std::bad_alloc if building the quadtree fails (propagates
   * naturally from QuadTree's constructor/Rebuild -- no manual -1 return).
   * This object stays validly constructed either way: unlike a constructor
   * failure, a failed Begin() is not "this object is now invalid", so
   * there's no reason to swallow/wrap the exception here.
   */
  void Begin(unsigned int seed = 0);

  /**
   * Commits whatever has been mutated on the underlying Graph since
   * construction or the last Sync() -- first structurally
   * (EmbeddedGraph::Sync(): admit new vertices, grow positions/draw mask,
   * rebuild synced adjacency), then for the physics (grow the per-vertex
   * arrays, place new vertices near their already-synced neighbors'
   * centroid jittered by up to edgeLength/2, or uniformly at random if a
   * vertex has no edges yet, then recompute degree/mass for EVERY vertex --
   * not just new ones, since a new edge can raise an already-tracked
   * vertex's degree). Existing vertices' positions and swinging/traction
   * history are left untouched, so the layout never jumps.
   *
   * @p seed seeds new-vertex jitter placement; pass 0 (the default) for a
   * time-based seed.
   *
   * @return true if a commit happened (structural, physics catch-up, or
   * both), false for the true no-op (nothing changed since the last
   * Sync()/construction). @throws std::bad_alloc on allocation failure; on
   * failure this object stays exactly as it was before the call (see the
   * .cpp for the "build into temporaries, publish only on success"
   * mechanism), so a caller that catches and retries later resumes cleanly.
   */
  bool Sync(unsigned int seed = 0);

  /**
   * Runs one round: accumulates the model's repulsive force (Barnes-Hut
   * quadtree walk, or exact all-pairs when Barnes-Hut is disabled) between
   * every active vertex pair, the model's attractive force along every real
   * edge, and (if ConfigureGravity() set a nonzero k) a constant-magnitude
   * pull toward the origin -- then scales the resulting per-vertex force
   * down to a displacement using ForceAtlas2's adaptive global-speed
   * regulation (Jacomy et al. 2014; see the .cpp for the exact formulas).
   *
   * @return the maximum per-vertex displacement actually applied this
   * round.
   */
  double Step();

  /**
   * Runs Step() until the max displacement drops below @p epsilon or
   * @p maxIters rounds have run.
   *
   * @return the number of rounds run.
   * @throws std::logic_error if Begin() has not been called -- a usage-order
   * precondition violation (there is no "run before begin" outcome a caller
   * would ever want to routinely check for and recover from, unlike the
   * genuinely-expected outcomes the rest of this library's exceptions
   * cover), replacing the old C API's -1 sentinel return.
   */
  size_t Run(size_t maxIters, double epsilon);

  /** Whether Begin() has been called at least once. */
  bool Begun() const noexcept { return begun_; }

  /** The maximum per-vertex displacement applied by the last Step(); 0
   *  before the first Step(). */
  double LastMaxDisplacement() const noexcept { return lastMaxDisplacement_; }

  /** Number of Step() rounds run since the last Begin(). */
  size_t Iteration() const noexcept { return iteration_; }

private:
  static double DefaultBoxExtent(size_t vertexCount, double edgeLength);

  void GrowPerVertexArraysTo(size_t newCount);
  void GatherPositions();
  double VertexRadiusIfEnabled(size_t idx) const noexcept;
  void AccumulateBHRepulsion(const QuadTree::Node *node, size_t selfIdx,
                              double vRadius, const double *vPos,
                              double *acc) const;
  void ComputeForceRange(size_t begin, size_t end);
  void ComputeSwingTractionRange(size_t begin, size_t end);
  void UpdateGlobalSpeed();
  void ApplySpeedRange(size_t begin, size_t end);
  void RecomputeDegreeMass();
  void PlaceGrownVertex(size_t i, unsigned int &seed);

  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);
  static void ActionToggleOverlapPrevention(EmbeddedGraph &embedding,
                                             void *userData,
                                             const ActionPayload &payload);

  std::unique_ptr<ForceModel> model_; // force computation strategy, fixed at construction

  // Active subgraph vertex ids, compact index i -> raw vertex id. Every
  // other per-vertex vector below is parallel to this one (size() in
  // lockstep); vertices_.size() IS the published/active vertex count, no
  // separate counter -- unlike the old C state->vertexCount, which had to be
  // a field distinct from state->vertices' allocated length because growth
  // there happened in place. Here GrowPerVertexArraysTo builds fully-grown
  // replacement vectors and moves them in only once every one has
  // succeeded, so vertices_ is never observably larger than "published."
  std::vector<size_t> vertices_;
  std::vector<size_t> degree_;   // out-degree + in-degree per vertex, from the synced CSRs
  std::vector<double> mass_;     // model_->VertexMass(degree_[i])
  std::vector<double> disp_;                // vertexCount * 2, raw net force before speed scaling
  std::vector<double> positionsScratch_;    // vertexCount * 2, gathered from base positions each round
  std::vector<double> attForceMag_;         // vertexCount, this round's attractive force magnitude
  std::vector<double> repForceMag_;         // vertexCount, this round's repulsive force magnitude
  std::vector<double> structForce_;         // vertexCount * 2, attraction + repulsion only (no gravity)
  std::vector<double> oldStructForce_;      // vertexCount * 2, previous round's structForce_
  std::vector<double> appliedDisp_;         // vertexCount * 2, disp_ after speed scaling
  std::vector<double> swinging_;            // vertexCount, mass-weighted swinging
  std::vector<double> traction_;            // vertexCount, mass-weighted effective traction

  // Graph::MutationCount() (via Structure().ParentMutationCount()) as of the
  // last time the PHYSICS side of Sync() completed. Normally equal to the
  // base class's own commit mark; it lags only when a Sync()'s structural
  // commit succeeded but its physics growth then failed on allocation, and
  // it's what lets the next Sync() notice and retry even though
  // EmbeddedGraph::Sync() itself reports "nothing new." Reading the parent
  // graph's live mutation count directly (rather than peeking at
  // EmbeddedGraph's own private syncedMutationCount_) is deliberate: it's
  // public API (Subgraph::ParentMutationCount()) and answers exactly the
  // same question the old C code asked of its sibling struct field.
  uint64_t physicsSyncedMutationCount_ = UINT64_MAX;

  double radiusBase_ = 0.0;
  double radiusPerDegree_ = 0.0;
  double overlapConstant_ = kOverlapConstantDefault;
  bool preventOverlap_ = false;
  double jitterTolerance_ = kJitterToleranceDefault;
  double globalSpeed_ = kSpeedInitial;
  double speedEfficiency_ = kSpeedEfficiencyInitial;
  double edgeLength_ = kEdgeLengthDefault;
  double boxExtent_ = 0.0;
  double gravityK_ = 0.0;
  size_t iteration_ = 0;
  double lastMaxDisplacement_ = 0.0;
  bool begun_ = false;
  double theta_ = kThetaDefault;
  size_t nodesPerCell_ = QuadTree::kNodesPerCellDefault;
  bool barnesHutEnabled_ = true;

  // Engaged exactly when the quadtree has been built at least once (old C's
  // quadtreeReady flag) -- stays disengaged whenever barnesHutEnabled_ is
  // false, since the quadtree is never built in that mode. std::optional is
  // a natural fit: QuadTree has no default constructor (it always indexes
  // *some* point buffer), so there is no "zeroed but safe" QuadTree value to
  // fall back on the way the old C struct's zero-initialized quadtree field
  // was -- optional supplies the same "maybe not built yet" state
  // explicitly instead.
  std::optional<QuadTree> quadtree_;

  // Owned unconditionally, unlike the old C state->pool (created with
  // gvizThreadPoolCreate(0), tolerated NULL on failure, and fell back to
  // gvizThreadPoolForRange's serial path everywhere it was used). ThreadPool
  // now throws std::system_error immediately on startup failure instead of
  // returning null, so there is no partially-constructed pool state to carry
  // around or null-check at every call site -- construction either succeeds
  // outright or this object's own construction fails with it, matching the
  // rest of this class's "allocation failure propagates, no manual checks"
  // convention. Held via unique_ptr rather than by value specifically so
  // ForceAtlas stays move-constructible: ThreadPool itself is neither
  // copyable nor movable (see ThreadPool.hpp), so a `ThreadPool pool_;`
  // value member would make the implicitly-declared ForceAtlas move
  // constructor deleted, breaking parity with the base EmbeddedGraph (which
  // *is* move-constructible) for no benefit -- the pointer is never null
  // after a successful construction, so no caller ever needs to check it
  // either way.
  std::unique_ptr<ThreadPool> pool_;
};

} // namespace gviz::layout

#endif
