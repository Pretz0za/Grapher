#ifndef GVIZ_TUTTE_HPP
#define GVIZ_TUTTE_HPP

#include "BitSet.hpp"
#include "DenseIndex.hpp"
#include "EmbeddedGraph.hpp"
#include "GraphLike.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace gviz::layout {

/**
 * Real-time Tutte barycentric embedding. Interior vertices are iteratively
 * moved toward the barycenter of their neighbors; boundary vertices are
 * pinned at fixed positions. Each Step(dt) advances the simulation by one
 * time-weighted relaxation pass: P_u += (barycenter(N(u)) - P_u) *
 * clamp(relaxationRate * dt, 0, 1).
 *
 * Does not test planarity or auto-pick a boundary: an actually-non-planar
 * Structure() simply produces a broken/overlapping layout when relaxed.
 * SetBoundary()/FixConvexPolygon() are the only way to establish a
 * boundary; Run() throws std::logic_error if none has ever been pinned.
 */
template <GraphLike G>
class Tutte : public EmbeddedGraph {
public:
  static constexpr double kDefaultEpsilon = 1e-5;
  static constexpr double kDefaultRelaxationRate = 5.0;

  /**
   * Builds Tutte state over @p structure (moved in) in @p dimension
   * dimensions. Registers action "tutte.step" and stat series
   * "tutte.maxDelta".
   *
   * @p epsilon is the convergence threshold below which Step()'s maximum
   * per-vertex displacement marks the embedding converged.
   *
   * @throws DimensionError if @p dimension != 2 (Tutte's planar
   * straight-line embedding is inherently 2D).
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  Tutte(G structure, size_t dimension, double epsilon = kDefaultEpsilon);

  Tutte(const Tutte &) = delete;
  Tutte(Tutte &&) noexcept = default;
  Tutte &operator=(const Tutte &) = delete;
  Tutte &operator=(Tutte &&) = delete;
  ~Tutte() override = default;

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
   * Pins @p boundary.size() boundary vertices (raw handles into Structure())
   * and copies their positions from @p polygonPositions (row-major, Dim()
   * doubles per vertex, read unchecked). Also sets Begun() true on success.
   * May be called again to change the outer face.
   *
   * @return false (no-op) if @p boundary has fewer than 3 vertices or
   *         contains a handle Structure() doesn't have -- both routine,
   *         checkable outcomes, not exceptions. true on success.
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
   *         SetBoundary() or FixConvexPolygon()).
   */
  size_t Run(size_t maxIters);

  /**
   * Places @p boundary.size() boundary vertices on a regular convex polygon
   * of @p radius in the first two dimensions, then calls SetBoundary().
   *
   * @return whatever SetBoundary() returns.
   */
  bool FixConvexPolygon(std::span<const size_t> boundary, double radius);

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

  /** Whether a boundary has ever been successfully pinned (via
   *  SetBoundary()/FixConvexPolygon()). */
  bool Begun() const noexcept { return begun_; }

  /** The currently pinned boundary (raw handles), in the order last passed
   *  to SetBoundary()/FixConvexPolygon(). Empty before either has been
   *  called. */
  std::span<const size_t> Boundary() const noexcept { return boundary_; }

  /** Whether raw handle @p u is currently pinned as a boundary vertex.
   *  Unchecked: @p u must be a handle Structure() has. */
  bool IsBoundaryVertex(size_t u) const noexcept {
    return isBoundary_.Test(index_.ToLocal(u));
  }

private:
  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);

  void SnapshotInterior();
  const double *NeighborReadPos(size_t vLocal) const noexcept;
  void ComputeBarycenter(size_t uLocal, double *out) const;
  double RelaxVertex(size_t uLocal, double alpha);

  G structure_;
  DenseIndex<G> index_;
  std::vector<size_t> boundary_; // raw handles
  BitSet isBoundary_;            // LOCAL-indexed
  std::vector<double> scratch_;  // n_ * Dim() Jacobi double-buffer, LOCAL-indexed
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
