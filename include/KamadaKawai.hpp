#ifndef GVIZ_KAMADAKAWAI_HPP
#define GVIZ_KAMADAKAWAI_HPP

#include "DenseIndex.hpp"
#include "EmbeddedGraph.hpp"
#include "GraphLike.hpp"

#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * Kamada & Kawai's 1989 graph-theoretic-distance layout. Minimizes a single
 * global energy defined purely in terms of graph-theoretic (hop-count)
 * distances -- no continuous physics simulation and no spatial index.
 *
 * For every pair (i, j) with graph distance d_ij (unweighted BFS hop
 * count), the ideal Euclidean length is l_ij = EdgeLength() * d_ij and the
 * spring constant is k_ij = Stiffness() / d_ij^2. The energy
 *   E = sum_{i<j} (1/2) * k_ij * (|P_i - P_j| - l_ij)^2
 * is minimized not by simultaneous gradient descent but by repeatedly:
 * finding the vertex with the largest gradient magnitude, running
 * Newton-Raphson steps that move *only that vertex* (solving the small
 * Dim() x Dim() Hessian-block linear system) until its own gradient drops
 * below Epsilon(), then picking the next-largest-gradient vertex. The whole
 * embedding is converged once every vertex's gradient is below Epsilon().
 *
 * Generic over any `GraphLike G`: owns its own `G structure_` plus a
 * `gviz::DenseIndex<G> index_`. The all-pairs distance table `distances_`
 * is sized `index_.Size() ^ 2`, addressed by local index.
 *
 * Cost: Begin() is O(V*(V+E)) (one BFS per vertex) and O(V^2) memory. Each
 * Step() scans every vertex's gradient (O(V^2) total) to find the next
 * vertex to refine, then runs one or more O(V) Newton-Raphson updates on
 * it. Intended for small/medium graphs, not GRIP/ForceAtlas scale.
 *
 * Structural requirement: the graph restricted to Structure() must be
 * connected (graph-theoretic distance is undefined between components).
 * Begin() throws NotConnectedError up front.
 *
 * Generalizes to arbitrary Dim() >= 1, unlike Tutte/SpringTutte/Planar
 * which are inherently 2D straight-line embeddings.
 */
template <GraphLike G>
class KamadaKawai : public EmbeddedGraph {
public:
  static constexpr double kDefaultEdgeLength = 100.0;
  static constexpr double kDefaultEpsilon = 1e-4;
  /** Global spring stiffness (K in k_ij = K / d_ij^2). Not a constructor
   *  parameter -- unlike EdgeLength(), scaling K alone does not change the
   *  energy's fixed point, only convergence dynamics/gradient units, so it
   *  defaults to 1.0 and is exposed only as a post-construction knob via
   *  SetStiffness(), mirroring how Tutte exposes RelaxationRate(). */
  static constexpr double kDefaultStiffness = 1.0;

  /**
   * Builds KamadaKawai state over @p structure (moved in) in @p dimension
   * dimensions. Registers action "kamadaKawai.step" and stat series
   * "kamadaKawai.maxGradient".
   *
   * @p edgeLength is the desired unit edge length L (ideal length for a
   * graph-distance-1 pair is exactly L). @p epsilon is the per-vertex
   * gradient-magnitude threshold below which a vertex (and, once true of
   * every vertex simultaneously, the whole embedding) is considered
   * converged.
   *
   * @throws DimensionError if @p dimension < 1.
   * @throws std::bad_alloc on allocation failure.
   */
  KamadaKawai(G structure, size_t dimension, double edgeLength = kDefaultEdgeLength,
              double epsilon = kDefaultEpsilon);

  KamadaKawai(const KamadaKawai &) = delete;
  KamadaKawai(KamadaKawai &&) noexcept = default;
  KamadaKawai &operator=(const KamadaKawai &) = delete;
  KamadaKawai &operator=(KamadaKawai &&) = delete;
  ~KamadaKawai() override = default;

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
   * Verifies Structure() is connected, computes the all-pairs graph-distance
   * table (one gviz::search::BreadthFirst per vertex), and seeds an initial
   * layout: for Dim() == 2, vertices are placed on a circle in Structure()'s
   * iteration order; for any other dimension this falls back to
   * EmbeddedGraph::RandomizePositions with a deterministic seed.
   *
   * Safe to call more than once per lifetime.
   *
   * @throws NotConnectedError if Structure() is not connected.
   * @throws std::bad_alloc on allocation failure.
   */
  void Begin();

  /**
   * Runs one outer iteration: scans every vertex in Structure() for the
   * largest gradient-magnitude vertex, then (if that magnitude is >=
   * Epsilon()) runs Newton-Raphson updates moving only that vertex until
   * its own gradient drops below Epsilon() (capped at an internal safety
   * iteration limit in case the local Hessian is ill-conditioned).
   *
   * @return the largest gradient magnitude found by this call's scan.
   */
  double Step();

  /**
   * Runs Step() until Converged() or @p maxIters total iterations have run.
   *
   * @return the number of iterations run since Begin().
   * @throws std::logic_error if Begin() has never been called.
   */
  size_t Run(size_t maxIters);

  /** The unit edge length fixed at construction. */
  double EdgeLength() const noexcept { return edgeLength_; }

  /** The convergence threshold fixed at construction. */
  double Epsilon() const noexcept { return epsilon_; }

  /** Global spring stiffness K (default kDefaultStiffness). */
  double Stiffness() const noexcept { return stiffness_; }
  void SetStiffness(double stiffness) noexcept { stiffness_ = stiffness; }

  /** Number of Step() calls run since the last Begin(). */
  size_t Iteration() const noexcept { return iteration_; }

  /** The largest gradient magnitude found by the last Step(); 0 before the
   *  first Step() (or since the last Begin()). */
  double LastMaxGradient() const noexcept { return lastMaxGradient_; }

  /** Whether every vertex's gradient was below Epsilon() as of the last
   *  Step()'s scan. */
  bool Converged() const noexcept { return converged_; }

  /** Whether Begin() has been called at least once. */
  bool Begun() const noexcept { return begun_; }

  /**
   * The graph-theoretic (BFS hop-count) distance between raw handles @p u
   * and @p v as computed by the last Begin(), or SIZE_MAX if Begin() has
   * never been called, either handle is unknown to Structure(), or the pair
   * is otherwise unknown.
   */
  size_t GraphDistance(size_t u, size_t v) const noexcept;

private:
  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);

  double ComputeGradientAndHessian(size_t m, double *grad, double *hessian) const;

  G structure_;
  DenseIndex<G> index_;
  std::vector<size_t> distances_; // n_ * n_ row-major, LOCAL-indexed, SIZE_MAX = unreachable
  size_t n_ = 0;
  double edgeLength_;
  double epsilon_;
  double stiffness_ = kDefaultStiffness;
  size_t iteration_ = 0;
  double lastMaxGradient_ = 0.0;
  bool converged_ = false;
  bool begun_ = false;
};

} // namespace gviz::layout

#endif
