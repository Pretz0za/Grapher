#ifndef GVIZ_SPRINGTUTTE_HPP
#define GVIZ_SPRINGTUTTE_HPP

#include "BitSet.hpp"
#include "EmbeddedGraph.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace gviz::layout {

/**
 * Second-order, spring-driven variant of the Tutte barycentric embedding:
 * port of gvizSpringTutteState + gvizSpringTutteEmbedder*. Same fixed point
 * as Tutte (every interior vertex at rest exactly at the barycenter of its
 * neighbors, boundary vertices pinned), reached via a damped harmonic
 * oscillator instead of Tutte's direct position blend, so underdamped
 * settings let a vertex overshoot its equilibrium and spring back before
 * settling instead of moving straight to it:
 *
 *   a_u = stiffness * (barycenter(N(u)) - P_u) - damping * v_u
 *   v_u += a_u * dt
 *   P_u += v_u * dt
 *
 * Structurally mirrors Tutte (see its class doc, which states the same
 * thing from the other side): same method names, same call order, same
 * design choices wherever the underlying behavior doesn't actually differ.
 * The two differ only in (a) this class's extra velocity state and
 * Configure(stiffness, damping) in place of Tutte's SetGaussSeidelEnabled(),
 * and (b) Run() taking an explicit @p dt here (the dynamics are
 * dt-sensitive) vs. Tutte's implicit dt=1.0 per iteration (a pure
 * barycentric blend has no natural time unit), exactly matching their old C
 * counterparts' own asymmetry there.
 *
 * Holds its own `Graph&` (see Planar.hpp's file-level note on why:
 * Subgraph deliberately never exposes its parent, but Begin() needs mutable
 * adjacency access to install a rotation system) in addition to the
 * `Subgraph` it hands to the EmbeddedGraph base -- both must refer to the
 * same graph; passing a @p subgraph not derived from @p g is a precondition
 * violation, unchecked, matching every other cross-object consistency
 * assumption in this library.
 */
class SpringTutte : public EmbeddedGraph {
public:
  static constexpr double kDefaultEpsilon = 1e-5;
  static constexpr double kDefaultStiffness = 30.0;
  static constexpr double kDefaultDamping = 6.0;

  /**
   * Builds spring-Tutte state over @p subgraph (moved in) in @p dimension
   * dimensions, holding a reference to @p g (its parent graph) for the
   * planarity/rotation work Begin() does. Registers actions
   * "springTutte.step" and "springTutte.fixOuterFace" and stat series
   * "springTutte.maxDelta" -- matches the old C gvizSpringTutteEmbedderInit.
   *
   * @p epsilon is the convergence threshold below which Step()'s maximum
   * per-vertex displacement marks the embedding converged. Unlike the old C
   * API's "pass 0 for the default" runtime sentinel, this is now an
   * ordinary default argument: 0 was never a legitimate epsilon (Step()
   * would then never report convergence), so there is no reason to keep it
   * as an in-band value instead of simply defaulting the parameter.
   *
   * @throws DimensionError if @p dimension != 2 (the boundary polygon and
   * face walking are inherently planar; the old C only checked this in
   * Begin(), this port tightens it to construction time since no other
   * dimension is ever valid for this class).
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  SpringTutte(Graph &g, Subgraph subgraph, size_t dimension,
              double epsilon = kDefaultEpsilon);

  // Polymorphic base (EmbeddedGraph) forbids copy and move-assignment for
  // the same reason documented there (Subgraph's `const Graph&` member
  // can't be reseated); SpringTutte follows the identical shape.
  SpringTutte(const SpringTutte &) = delete;
  SpringTutte(SpringTutte &&) noexcept = default;
  SpringTutte &operator=(const SpringTutte &) = delete;
  SpringTutte &operator=(SpringTutte &&) = delete;
  ~SpringTutte() override = default;

  /**
   * Verifies planarity, applies a combinatorial rotation system
   * (ApplyPlanarRotation), pins the largest combinatorial face
   * (LargestFaceBoundary) as the initial outer boundary on a regular convex
   * polygon, and seeds interior vertices.
   *
   * Safe to call more than once per lifetime -- deliberately not guarded
   * against, matching Tutte::Begin() (and the old C, which never prevented a
   * second call either): calling it again re-tests planarity, re-installs
   * the rotation system, and re-pins/reseeds from scratch, exactly like
   * calling Begin() once on a fresh object. There is no partially-begun
   * state to protect against.
   *
   * @throws PlanarNotPlanarError/NotPlanarError if the graph restricted to
   *         Structure() is not planar (see ApplyPlanarRotation).
   * @throws LayoutError on an internal Boyer-Myrvold failure.
   */
  void Begin();

  /**
   * Pins @p boundary.size() boundary vertices and copies their positions
   * from @p polygonPositions (row-major, Dim() doubles per vertex, read
   * unchecked -- the caller must supply at least boundary.size() * Dim()
   * values, same unchecked contract the old C's raw pointer had). Zeroes
   * velocity for the pinned vertices (SpringTutte-only: Tutte has no
   * velocity to zero). May be called again to change the outer face.
   *
   * @return false (no-op) if @p boundary has fewer than 3 vertices or
   *         contains an index >= the parent graph's vertex count -- both
   *         routine, checkable outcomes, not exceptions. true on success.
   */
  bool SetBoundary(std::span<const size_t> boundary,
                    std::span<const double> polygonPositions);

  /**
   * Seeds every interior (non-boundary) vertex to the centroid of the
   * currently pinned boundary polygon and zeroes all velocities. Also
   * resets Iteration(), LastMaxDelta(), and Converged().
   */
  void SeedInterior();

  /**
   * Advances the embedding by one dt-sized step of damped spring dynamics
   * (see the class doc for the exact update). Reads old positions from a
   * Jacobi double-buffer so every interior vertex's update within a step is
   * order-independent. Appends to stat series "springTutte.maxDelta".
   *
   * @return the maximum per-vertex L2 position displacement this step, or 0
   *   if @p dt <= 0 (a no-op, matching the old C).
   */
  double Step(double dt);

  /**
   * Runs Step(dt) until Converged() or Iteration() reaches @p maxIters --
   * note @p maxIters bounds the total (persistent) Iteration() count, not
   * how many additional rounds this particular call contributes (matches
   * the old C's `while (!converged && iteration < maxIters)` exactly, and
   * Tutte::Run()'s identical loop shape): a caller that already advanced
   * Iteration() partway via manual Step() calls before calling
   * Run(maxIters, dt) only gets the remaining budget, not @p maxIters fresh
   * rounds.
   *
   * @return Iteration() after running (the persistent total, not the count
   *   of rounds this call added).
   * @throws std::logic_error if no boundary has ever been pinned (via
   *         Begin(), SetBoundary(), or FixConvexPolygon()) -- the actual
   *         precondition the old C's -1 "invalid state" return encoded,
   *         matching Tutte::Run()'s and ForceAtlas::Run()'s equivalent
   *         choice.
   */
  size_t Run(size_t maxIters, double dt);

  /**
   * Places @p boundary.size() boundary vertices on a regular convex polygon
   * of @p radius in the first two dimensions, then calls SetBoundary().
   * Deliberately takes no separate count parameter (unlike the old C's
   * gvizSpringTutteFixConvexPolygon(s, boundary, count, radius)): a
   * std::span already carries its own size, so a second, independently-
   * suppliable count would only invite the two to disagree for no benefit.
   *
   * @return whatever SetBoundary() returns (false for boundary.size() < 3
   *         or an out-of-range index) -- an improvement over the old C's
   *         void return, which silently discarded that failure.
   */
  bool FixConvexPolygon(std::span<const size_t> boundary, double radius);

  /**
   * Pins the highlight subgraph's boundary cycle (found by locating any
   * edge within it, then walking its face via FaceWalk) on a regular convex
   * polygon and resets convergence state. Vertices not on the new boundary
   * -- including ones that were pinned to the previous boundary -- keep
   * their current position AND velocity, relaxing as interior vertices from
   * wherever they were left; this is the one place SpringTutte's contract
   * differs from Tutte's FixOuterFace (Tutte has no velocity to preserve).
   *
   * @return false when no highlight is set, this embedding has no planar
   *         rotation installed yet, the highlight has no edges, or the
   *         resulting boundary has fewer than 3 vertices -- all routine,
   *         checkable outcomes. true on success.
   */
  bool FixOuterFace();

  /**
   * Overrides stiffness and damping. Pass 0 for either to keep its
   * current/default value (this "0 means keep current" convention is
   * preserved from the old C, unlike epsilon's constructor argument above:
   * stiffness/damping are legitimately re-tunable mid-lifetime -- e.g. from
   * a live UI slider -- where "0, i.e. leave it alone" is a meaningful,
   * frequently-taken call shape, not a one-time constructor sentinel with a
   * single obvious default). Callable before or after Begin(); takes effect
   * on the next Step().
   */
  void Configure(double stiffness, double damping) noexcept;

  /** Current spring constant, as set by Configure() (default
   *  kDefaultStiffness). */
  double Stiffness() const noexcept { return stiffness_; }

  /** Current velocity damping coefficient, as set by Configure() (default
   *  kDefaultDamping; >= 2*sqrt(Stiffness()) is critically damped). */
  double Damping() const noexcept { return damping_; }

  /** The convergence threshold fixed at construction. */
  double Epsilon() const noexcept { return epsilon_; }

  /** Number of Step() calls run since the last SeedInterior(). */
  size_t Iteration() const noexcept { return iteration_; }

  /** The maximum per-vertex displacement applied by the last Step(); 0
   *  before the first Step() (or since the last SeedInterior()). */
  double LastMaxDelta() const noexcept { return lastMaxDelta_; }

  /** Whether the last Step()'s maximum displacement fell below Epsilon(). */
  bool Converged() const noexcept { return converged_; }

  /** Whether Begin() or FixOuterFace() has been called at least once. */
  bool Begun() const noexcept { return begun_; }

  /** The currently pinned boundary, in the order last passed to
   *  SetBoundary()/FixConvexPolygon()/FixOuterFace(). Empty before any of
   *  those has been called. */
  std::span<const size_t> Boundary() const noexcept { return boundary_; }

  /** Whether raw vertex @p u is currently pinned as a boundary vertex.
   *  Unchecked: @p u must be < the parent graph's vertex count as of
   *  construction. */
  bool IsBoundaryVertex(size_t u) const noexcept {
    return isBoundary_.Test(u);
  }

  /** Pointer to raw vertex @p u's current velocity (Dim() doubles); zero for
   *  boundary vertices. SpringTutte-only (Tutte has no velocity state) --
   *  exists mainly so tests/front-ends can observe the oscillation this
   *  class's damped dynamics is the whole point of. Same unchecked contract
   *  as IsBoundaryVertex(). */
  const double *GetVelocity(size_t u) const noexcept {
    return velocity_.data() + u * Dim();
  }

private:
  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);
  static void ActionFixOuterFace(EmbeddedGraph &embedding, void *userData,
                                  const ActionPayload &payload);

  void SnapshotInterior();
  const double *NeighborReadPos(size_t v) const noexcept;
  void ComputeBarycenter(size_t u, double *out) const;
  double SpringVertex(size_t u, double dt);

  Graph &graph_;
  std::vector<size_t> boundary_;
  BitSet isBoundary_;
  std::vector<double> scratch_; // N * Dim() Jacobi double-buffer
  std::vector<double> velocity_; // N * Dim(); zero for boundary vertices
  size_t iteration_ = 0;
  double lastMaxDelta_ = 0.0;
  bool converged_ = false;
  double epsilon_;
  double stiffness_ = kDefaultStiffness;
  double damping_ = kDefaultDamping;
  bool begun_ = false;
};

} // namespace gviz::layout

#endif
