#ifndef GVIZ_FORCEATLAS_HPP
#define GVIZ_FORCEATLAS_HPP

#include "DenseIndex.hpp"
#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "ForceModel.hpp"
#include "GraphLike.hpp"
#include "QuadTree.hpp"
#include "ThreadPool.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace gviz::layout {

/**
 * Barnes-Hut force-directed layout: repulsion applies between every pair of
 * vertices and is approximated by walking a QuadTree over the current
 * positions and treating distant subtrees as a single pseudo-body at their
 * center of mass; attraction is computed additionally, on top of that
 * repulsion, exactly along real graph edges (O(E) per round).
 *
 * Generic over any `GraphLike G` (typically gviz::Graph or gviz::Subgraph):
 * owns its own `G structure_` plus a `gviz::DenseIndex<G> index_` built once
 * from it at construction. Every physics array is sized to the view's
 * actual vertex count and indexed by local/compact index; a native G handle
 * only appears where an edge is actually crossed.
 *
 * The force math (attraction, repulsion, quadtree mass) is pluggable via a
 * ForceModel fixed at construction; everything else here (speed
 * regulation, Barnes-Hut traversal, actions, stat series) is shared across
 * models. SetBarnesHutEnabled(false) switches to exact O(V^2) all-pairs
 * repulsion instead of the scalable quadtree default -- call before
 * Begin(); toggling mid-run is unsupported.
 *
 * Dimension is hard-locked to 2 (repulsion needs a 2D QuadTree); the
 * constructor throws DimensionError otherwise.
 *
 * Every per-vertex array is built exactly once, at construction, from
 * `structure_` as it stood then; there is no dynamic-graph growth support.
 *
 * Threading: force evaluation, swinging/traction, and speed application are
 * data-parallel across vertices via an owned ThreadPool when one could be
 * started, falling back to serial execution when it couldn't (e.g. no
 * thread support on the platform).
 */
template <GraphLike G>
class ForceAtlas : public EmbeddedGraph {
public:
  static constexpr double kEdgeLengthDefault = 1000.0;
  static constexpr double kThetaDefault = 1.0;
  static constexpr double kOverlapConstantDefault = 100.0;
  static constexpr double kJitterToleranceDefault = 1.0;
  static constexpr double kSpeedInitial = 1.0;
  static constexpr double kSpeedEfficiencyInitial = 1.0;
  static constexpr double kSpeedEfficiencyMin = 0.05;
  static constexpr double kSpeedEfficiencyMax = 1.0;
  static constexpr double kSpeedMaxRise = 0.5;

  /**
   * Initializes a Barnes-Hut force layout over @p structure (moved in) with
   * force math supplied by @p model (moved in; fixed for the object's
   * lifetime, like @p dimension). Registers actions "forceEmbedder.step"
   * and "forceEmbedder.toggleOverlapPrevention", and stat series
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
   * @p structure may be directed or undirected; direction only affects
   * whether an in-adjacency index is built alongside the out one, never
   * repulsion/gravity/speed regulation.
   *
   * @throws DimensionError if @p dimension != 2.
   * @throws std::bad_alloc (or std::system_error, from the owned
   * ThreadPool) on allocation/thread startup failure.
   */
  ForceAtlas(G structure, size_t dimension, std::unique_ptr<ForceModel> model);

  ForceAtlas(const ForceAtlas &) = delete;
  ForceAtlas(ForceAtlas &&) noexcept = default;
  ForceAtlas &operator=(const ForceAtlas &) = delete;
  ForceAtlas &operator=(ForceAtlas &&) = delete;
  ~ForceAtlas() override = default;

  /** The structure this embedder was built over. */
  G &Structure() noexcept { return structure_; }
  const G &Structure() const noexcept { return structure_; }

  // Native-handle position accessors; shadow the base class's local-index
  // versions of the same names, translating via index_.ToLocal().
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
   *  unsupported. */
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
   *  configured. Safe to call at any time, including mid-simulation. Also
   *  exposed as the "forceEmbedder.toggleOverlapPrevention" action for live
   *  toggling from a bound key. */
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
   * @throws std::bad_alloc if building the quadtree fails. The object stays
   * validly constructed either way.
   */
  void Begin(unsigned int seed = 0);

  /**
   * Runs one round: accumulates the model's repulsive force (Barnes-Hut
   * quadtree walk, or exact all-pairs when Barnes-Hut is disabled) between
   * every active vertex pair, the model's attractive force along every real
   * edge, and (if ConfigureGravity() set a nonzero k) a constant-magnitude
   * pull toward the origin -- then scales the resulting per-vertex force
   * down to a displacement using ForceAtlas2's adaptive global-speed
   * regulation (Jacomy et al. 2014).
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
   * @throws std::logic_error if Begin() has not been called.
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

  void BuildInAdjacencyIfDirected();
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

  /** Runs @p task over [begin, end) via the worker pool if one exists,
   *  falling back to one synchronous call on the calling thread otherwise. */
  void RunForRange(size_t begin, size_t end, size_t grain,
                    const std::function<void(size_t, size_t)> &task);

  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);
  static void ActionToggleOverlapPrevention(EmbeddedGraph &embedding,
                                             void *userData,
                                             const ActionPayload &payload);

  G structure_;
  DenseIndex<G> index_;
  std::unique_ptr<ForceModel> model_; // force computation strategy, fixed at construction

  // Out-adjacency (local index -> local neighbor indices) and, only for a
  // directed structure_, in-adjacency, both built exactly once at
  // construction.
  std::vector<size_t> outOffsets_, outNeighborsLocal_;
  std::vector<size_t> inOffsets_, inNeighborsLocal_;

  std::vector<size_t> degree_;   // out-degree + in-degree per vertex, local-indexed
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

  // Engaged once the quadtree has been built at least once; stays
  // disengaged whenever barnesHutEnabled_ is false.
  std::optional<QuadTree> quadtree_;

  // Null when a worker thread failed to start (e.g. no thread support on
  // the platform); every parallel phase then falls back to running its
  // whole range serially via RunForRange. unique_ptr rather than a value
  // member so ForceAtlas stays move-constructible (ThreadPool itself is
  // neither copyable nor movable).
  std::unique_ptr<ThreadPool> pool_;
};

} // namespace gviz::layout

#endif
