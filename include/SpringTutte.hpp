#ifndef GVIZ_SPRINGTUTTE_HPP
#define GVIZ_SPRINGTUTTE_HPP

#include "BitSet.hpp"
#include "DenseIndex.hpp"
#include "EmbeddedGraph.hpp"
#include "GraphLike.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace gviz::layout {

/**
 * Second-order, spring-driven variant of the Tutte barycentric embedding.
 * Same fixed point as Tutte (every interior vertex at rest exactly at the
 * barycenter of its neighbors, boundary vertices pinned), reached via a
 * damped harmonic oscillator instead of Tutte's direct position blend:
 *
 *   a_u = stiffness * (barycenter(N(u)) - P_u) - damping * v_u
 *   v_u += a_u * dt
 *   P_u += v_u * dt
 *
 * Does not test planarity or auto-pick a boundary; SetBoundary()/
 * FixConvexPolygon() are the only way to establish one.
 */
template <GraphLike G>
class SpringTutte : public EmbeddedGraph {
public:
  static constexpr double kDefaultEpsilon = 1e-5;
  static constexpr double kDefaultStiffness = 30.0;
  static constexpr double kDefaultDamping = 6.0;

  /**
   * Builds spring-Tutte state over @p structure (moved in) in @p dimension
   * dimensions. Registers action "springTutte.step" and stat series
   * "springTutte.maxDelta".
   *
   * @throws DimensionError if @p dimension != 2 (the boundary polygon is
   * inherently planar).
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  SpringTutte(G structure, size_t dimension, double epsilon = kDefaultEpsilon);

  SpringTutte(const SpringTutte &) = delete;
  SpringTutte(SpringTutte &&) noexcept = default;
  SpringTutte &operator=(const SpringTutte &) = delete;
  SpringTutte &operator=(SpringTutte &&) = delete;
  ~SpringTutte() override = default;

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
   * Pins @p boundary.size() boundary vertices (raw handles) and copies
   * their positions from @p polygonPositions (row-major, Dim() doubles per
   * vertex, read unchecked). Zeroes velocity for the pinned vertices. Also
   * marks Begun() true on success. May be called again to change the outer
   * face.
   *
   * @return false (no-op) if @p boundary has fewer than 3 vertices or
   *         contains a handle Structure() doesn't have. true on success.
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
   *   if @p dt <= 0 (a no-op).
   */
  double Step(double dt);

  /**
   * Runs Step(dt) until Converged() or Iteration() reaches @p maxIters.
   *
   * @return Iteration() after running.
   * @throws std::logic_error if no boundary has ever been pinned.
   */
  size_t Run(size_t maxIters, double dt);

  /**
   * Places @p boundary.size() boundary vertices on a regular convex polygon
   * of @p radius in the first two dimensions, then calls SetBoundary().
   *
   * @return whatever SetBoundary() returns.
   */
  bool FixConvexPolygon(std::span<const size_t> boundary, double radius);

  /**
   * Overrides stiffness and damping. Pass 0 for either to keep its
   * current/default value. Callable before or after any Step(); takes
   * effect on the next Step().
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

  /** Whether a boundary has ever been successfully pinned. */
  bool Begun() const noexcept { return begun_; }

  /** The currently pinned boundary (raw handles), in the order last passed
   *  to SetBoundary()/FixConvexPolygon(). */
  std::span<const size_t> Boundary() const noexcept { return boundary_; }

  /** Whether raw handle @p u is currently pinned as a boundary vertex.
   *  Unchecked: @p u must be a handle Structure() has. */
  bool IsBoundaryVertex(size_t u) const noexcept {
    return isBoundary_.Test(index_.ToLocal(u));
  }

  /** Pointer to raw handle @p u's current velocity (Dim() doubles); zero
   *  for boundary vertices. Unchecked: @p u must be a handle Structure()
   *  has. */
  const double *GetVelocity(size_t u) const noexcept {
    return velocity_.data() + index_.ToLocal(u) * Dim();
  }

private:
  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);

  void SnapshotInterior();
  const double *NeighborReadPos(size_t vLocal) const noexcept;
  void ComputeBarycenter(size_t uLocal, double *out) const;
  double SpringVertex(size_t uLocal, double dt);

  G structure_;
  DenseIndex<G> index_;
  std::vector<size_t> boundary_; // raw handles
  BitSet isBoundary_;            // LOCAL-indexed
  std::vector<double> scratch_;  // n_ * Dim() Jacobi double-buffer, LOCAL-indexed
  std::vector<double> velocity_; // n_ * Dim(), LOCAL-indexed; zero for boundary vertices
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
