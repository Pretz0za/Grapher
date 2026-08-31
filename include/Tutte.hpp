#ifndef GVIZ_TUTTE_HPP
#define GVIZ_TUTTE_HPP

#include "BitSet.hpp"
#include "EmbeddedGraph.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace gviz::layout {

/**
 * Real-time Tutte barycentric embedding: port of gvizTutteState +
 * gvizTutteEmbedder*. Interior vertices are iteratively moved toward the
 * barycenter of their neighbors; boundary vertices are pinned at fixed
 * positions. Each Step(dt) advances the simulation by one time-weighted
 * relaxation pass: P_u += (barycenter(N(u)) - P_u) * clamp(relaxationRate *
 * dt, 0, 1).
 *
 * Structurally mirrors SpringTutte (a damped-harmonic-oscillator sibling
 * reaching the same fixed point): same method names, same call order, same
 * design choices wherever the underlying behavior doesn't actually differ.
 * The two differ only in (a) SpringTutte's extra velocity state and
 * Configure(stiffness, damping) in place of this class's
 * SetGaussSeidelEnabled(), and (b) Run() taking an explicit @p dt in
 * SpringTutte (its dynamics are dt-sensitive) vs. this class's implicit
 * dt=1.0 per iteration (a pure barycentric blend has no natural time unit),
 * exactly matching their old C counterparts' own asymmetry there.
 *
 * Holds its own `Graph&` (see Planar.hpp's file-level note on why:
 * Subgraph deliberately never exposes its parent, but Begin() needs mutable
 * adjacency access to install a rotation system) in addition to the
 * `Subgraph` it hands to the EmbeddedGraph base -- both must refer to the
 * same graph; passing a @p subgraph not derived from @p g is a precondition
 * violation, unchecked, matching every other cross-object consistency
 * assumption in this library.
 */
class Tutte : public EmbeddedGraph {
public:
  static constexpr double kDefaultEpsilon = 1e-5;
  static constexpr double kDefaultRelaxationRate = 5.0;

  /**
   * Builds Tutte state over @p subgraph (moved in) in @p dimension
   * dimensions, holding a reference to @p g (its parent graph) for the
   * planarity/rotation work Begin() does. Registers actions "tutte.step"
   * and "tutte.fixOuterFace" and stat series "tutte.maxDelta" -- matches
   * the old C gvizTutteEmbedderInit.
   *
   * @p epsilon is the convergence threshold below which Step()'s maximum
   * per-vertex displacement marks the embedding converged. Unlike the old C
   * API's "pass 0 for the default" runtime sentinel, this is now an
   * ordinary default argument: 0 was never a legitimate epsilon (Step()
   * would then never report convergence), so there is no reason to keep it
   * as an in-band value instead of simply defaulting the parameter.
   *
   * @throws DimensionError if @p dimension != 2 (Tutte's planar
   * straight-line embedding is inherently 2D; the old C only checked this
   * in Begin(), this port tightens it to construction time since no other
   * dimension is ever valid for this class).
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  Tutte(Graph &g, Subgraph subgraph, size_t dimension,
        double epsilon = kDefaultEpsilon);

  // Polymorphic base (EmbeddedGraph) forbids copy and move-assignment for
  // the same reason documented there (Subgraph's `const Graph&` member
  // can't be reseated); Tutte follows the identical shape.
  Tutte(const Tutte &) = delete;
  Tutte(Tutte &&) noexcept = default;
  Tutte &operator=(const Tutte &) = delete;
  Tutte &operator=(Tutte &&) = delete;
  ~Tutte() override = default;

  /**
   * Verifies planarity, applies a combinatorial rotation system
   * (ApplyPlanarRotation), pins the largest combinatorial face
   * (LargestFaceBoundary) as the initial outer boundary on a regular convex
   * polygon, and seeds interior vertices.
   *
   * Safe to call more than once per lifetime -- deliberately not guarded
   * against, matching the old C (which never prevented a second call
   * either): calling it again re-tests planarity, re-installs the rotation
   * system, and re-pins/reseeds from scratch, exactly like calling
   * Begin() once on a fresh object. There is no partially-begun state to
   * protect against, so a stricter "throw if already begun" guard would
   * only remove a legitimate reset path other embedders in this milestone
   * (e.g. GRIP::Begin()) also leave open.
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
   * values, same unchecked contract the old C's raw pointer had). May be
   * called again to change the outer face.
   *
   * @return false (no-op) if @p boundary has fewer than 3 vertices or
   *         contains an index >= the parent graph's vertex count -- both
   *         routine, checkable outcomes, not exceptions. true on success.
   */
  bool SetBoundary(std::span<const size_t> boundary,
                    std::span<const double> polygonPositions);

  /**
   * Seeds every interior (non-boundary) vertex to the centroid of the
   * currently pinned boundary polygon. Also resets Iteration(),
   * LastMaxDelta(), and Converged().
   */
  void SeedInterior();

  /**
   * Advances the embedding by one dt-weighted relaxation pass. Each
   * interior vertex moves a fraction min(RelaxationRate() * dt, 1) toward
   * its neighbor barycenter. Jacobi mode (default) reads old positions and
   * writes from a scratch double-buffer so all updates within a step are
   * order-independent; GaussSeidelEnabled() reads live (already-updated)
   * neighbor positions instead.
   *
   * @return the maximum per-vertex L2 displacement this step.
   */
  double Step(double dt);

  /**
   * Runs Step(1.0) until Converged() or @p maxIters steps have run.
   *
   * @return the number of iterations run.
   * @throws std::logic_error if no boundary has ever been pinned (via
   *         Begin(), SetBoundary(), or FixConvexPolygon()) -- the actual
   *         precondition the old C's -1 "invalid state" return encoded
   *         (`!s->boundary`, not "Begin() was never called": the old test
   *         suite exercises SetBoundary()/FixConvexPolygon()+SeedInterior()
   *         directly, without ever calling Begin(), and Run() must keep
   *         working in that usage). This is a usage-order precondition
   *         violation, not a routinely-expected outcome, so it's an
   *         exception rather than a sentinel return, matching
   *         ForceAtlas::Run()'s equivalent choice.
   */
  size_t Run(size_t maxIters);

  /**
   * Places @p boundary.size() boundary vertices on a regular convex polygon
   * of @p radius in the first two dimensions, then calls SetBoundary().
   * Deliberately takes no separate count parameter (unlike the old C's
   * gvizTutteFixConvexPolygon(s, boundary, count, radius)): a std::span
   * already carries its own size, so a second, independently-suppliable
   * count would only invite the two to disagree for no benefit -- this
   * port drops it rather than porting a footgun literally.
   *
   * @return whatever SetBoundary() returns (false for boundary.size() < 3
   *         or an out-of-range index) -- an improvement over the old C's
   *         void return, which silently discarded that failure.
   */
  bool FixConvexPolygon(std::span<const size_t> boundary, double radius);

  /**
   * Pins the highlight subgraph's boundary cycle (found by locating any
   * edge within it, then walking its face via FaceWalk) on a regular
   * convex polygon and resets convergence state. Vertices not on the new
   * boundary keep their current position (including vertices pinned to the
   * previous boundary, which now relax as interior vertices from wherever
   * they were left).
   *
   * @return false when no highlight is set, this embedding has no planar
   *         rotation installed yet, the highlight has no edges, or the
   *         resulting boundary has fewer than 3 vertices -- all routine,
   *         checkable outcomes. true on success.
   */
  bool FixOuterFace();

  /** Enables or disables Gauss-Seidel relaxation (reading live,
   *  already-updated neighbor positions within a Step()) instead of the
   *  default Jacobi mode (reading a snapshot taken before the step, so all
   *  updates within it are order-independent). Both converge to the same
   *  fixed point; Gauss-Seidel typically needs fewer iterations. */
  void SetGaussSeidelEnabled(bool enabled) noexcept {
    useGaussSeidel_ = enabled;
  }

  /** Whether Gauss-Seidel relaxation is currently enabled. */
  bool GaussSeidelEnabled() const noexcept { return useGaussSeidel_; }

  /** Overrides the blend factor per second used by Step() (default 5.0). */
  void SetRelaxationRate(double rate) noexcept { relaxationRate_ = rate; }

  /** Current relaxation rate, as set by SetRelaxationRate(). */
  double RelaxationRate() const noexcept { return relaxationRate_; }

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

private:
  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);
  static void ActionFixOuterFace(EmbeddedGraph &embedding, void *userData,
                                  const ActionPayload &payload);

  void SnapshotInterior();
  const double *NeighborReadPos(size_t v) const noexcept;
  void ComputeBarycenter(size_t u, double *out) const;
  double RelaxVertex(size_t u, double alpha);

  Graph &graph_;
  std::vector<size_t> boundary_;
  BitSet isBoundary_;
  std::vector<double> scratch_; // N * Dim() Jacobi double-buffer
  size_t iteration_ = 0;
  double lastMaxDelta_ = 0.0;
  bool converged_ = false;
  bool useGaussSeidel_ = false;
  double epsilon_;
  double relaxationRate_ = kDefaultRelaxationRate;
  bool begun_ = false;
};

} // namespace gviz::layout

#endif
