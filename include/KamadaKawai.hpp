#ifndef GVIZ_KAMADAKAWAI_HPP
#define GVIZ_KAMADAKAWAI_HPP

#include "EmbeddedGraph.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * Kamada & Kawai's 1989 graph-theoretic-distance layout ("An algorithm for
 * drawing general undirected graphs"): unlike ForceAtlas's continuous force
 * simulation or GRIP's MIS-filtration + KNN-spring hierarchy, this embedder
 * minimizes a single global energy defined purely in terms of graph-theoretic
 * (hop-count) distances -- there is no continuous "physics" and no spatial
 * index.
 *
 * For every pair (i, j) with graph distance d_ij (unweighted BFS hop count --
 * edge weights are deliberately ignored, matching this codebase's existing
 * precedent of not threading weights through algorithms that don't already
 * need them, e.g. GraphLoader parses-but-doesn't-apply .edges weights), the
 * ideal Euclidean length is l_ij = EdgeLength() * d_ij and the spring
 * constant is k_ij = Stiffness() / d_ij^2. The energy
 *   E = sum_{i<j} (1/2) * k_ij * (|P_i - P_j| - l_ij)^2
 * is minimized not by simultaneous gradient descent but by repeatedly:
 * finding the vertex with the largest gradient magnitude, running
 * Newton-Raphson steps that move *only that vertex* (solving the small
 * Dim() x Dim() Hessian-block linear system) until its own gradient drops
 * below Epsilon(), then picking the next-largest-gradient vertex. The whole
 * embedding is converged once every vertex's gradient is below Epsilon().
 *
 * Cost: Begin() is O(V*(V+E)) (one BreadthFirst per vertex) and O(V^2)
 * memory (the all-pairs hop-distance table). Each Step() scans every
 * vertex's gradient (O(V) each, O(V^2) total) to find the next vertex to
 * refine, then runs one or more O(V) Newton-Raphson updates on it. This
 * does not scale like GRIP -- it is intended for small/medium graphs, with
 * a precompute-then-iterate-to-convergence shape much closer to Tutte's
 * than to GRIP's or ForceAtlas's.
 *
 * Structural requirement: the graph restricted to Structure() must be
 * connected (graph-theoretic distance is undefined between components).
 * Unlike Tutte's planarity check (also deferred to Begin(), the same
 * precedent this follows), there is no way to "partially" satisfy
 * connectivity, so Begin() throws NotConnectedError up front rather than
 * leaving some pairwise distances undefined.
 *
 * Generalizes to arbitrary Dim() >= 1 (the energy/gradient/Hessian math is
 * dimension-agnostic), unlike Tutte/SpringTutte/Planar which are inherently
 * 2D straight-line embeddings.
 *
 * Subgraph-only construction, like ForceAtlas/GRIP: this class does NOT hold
 * a `Graph&` alongside its Subgraph. The all-pairs distance table needs a
 * plain BFS from every vertex, but the free gviz::search::BreadthFirst
 * function requires its `out` parameter to be a *full* subgraph (so it has
 * an edge bitset to record the BFS tree into) -- a feature this class never
 * uses, since it only ever reads hop counts, never the tree. Rather than
 * pay for that with a Graph& (needed only to call Graph::EnsureLayout() and
 * construct a throwaway full subgraph), Begin() hand-rolls the same small
 * neighbor-queue BFS GRIP::VerticesWithinRadius already hand-rolls for an
 * identical reason, reading only Structure().Neighbors() -- entirely
 * answerable through Subgraph's existing public API.
 */
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
   * Builds KamadaKawai state over @p subgraph (moved in) in @p dimension
   * dimensions. Registers action "kamadaKawai.step" and stat series
   * "kamadaKawai.maxGradient" (StatChartKind::LineLog -- gradient magnitude
   * spans decades while converging, same reasoning as GRIP's heat stats).
   *
   * @p edgeLength is the desired unit edge length L (ideal length for a
   * graph-distance-1 pair is exactly L). @p epsilon is the per-vertex
   * gradient-magnitude threshold below which a vertex (and, once true of
   * every vertex simultaneously, the whole embedding) is considered
   * converged.
   *
   * @throws DimensionError if @p dimension < 1 (the Newton-Raphson solve is
   * well-posed for any dimension >= 1; there is no upper bound like GRIP's
   * 2/3/4 or Tutte's fixed 2, since the energy model itself doesn't care).
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  KamadaKawai(Subgraph subgraph, size_t dimension,
              double edgeLength = kDefaultEdgeLength,
              double epsilon = kDefaultEpsilon);

  // Polymorphic base (EmbeddedGraph) forbids copy and move-assignment for
  // the same reason documented there (Subgraph's `const Graph&` member
  // can't be reseated); KamadaKawai follows the identical shape.
  KamadaKawai(const KamadaKawai &) = delete;
  KamadaKawai(KamadaKawai &&) noexcept = default;
  KamadaKawai &operator=(const KamadaKawai &) = delete;
  KamadaKawai &operator=(KamadaKawai &&) = delete;
  ~KamadaKawai() override = default;

  /**
   * Verifies Structure() is connected, computes the all-pairs graph-distance
   * table (one gviz::search::BreadthFirst per vertex), and seeds an initial
   * layout: for Dim() == 2, vertices are placed on a circle in Structure()'s
   * iteration order (the standard Kamada-Kawai starting point, chosen
   * because it spreads every vertex apart before any energy-based
   * refinement runs and converges well in practice); for any other
   * dimension there is no analogous "the standard circle" convention, so
   * this falls back to EmbeddedGraph::RandomizePositions with a
   * deterministic seed (chosen over a time-based seed so repeated runs are
   * reproducible, matching this library's general preference for
   * deterministic behavior wherever a caller doesn't ask for randomness).
   *
   * Safe to call more than once per lifetime, mirroring Tutte::Begin() --
   * re-verifies connectivity, rebuilds the distance table from scratch, and
   * re-seeds the initial layout, exactly like calling Begin() once on a
   * fresh object.
   *
   * @throws NotConnectedError if Structure() is not connected (more than
   * one component; an empty or single-vertex Structure() is trivially
   * connected and does not throw).
   * @throws std::bad_alloc on allocation failure, propagated naturally.
   */
  void Begin();

  /**
   * Runs one outer iteration: scans every vertex in Structure() for the
   * largest gradient-magnitude vertex, then (if that magnitude is >=
   * Epsilon()) runs Newton-Raphson updates moving only that vertex until
   * its own gradient drops below Epsilon() (capped at an internal safety
   * iteration limit in case the local Hessian is ill-conditioned).
   *
   * @return the largest gradient magnitude found by this call's scan (the
   * value that determined whether -- and which vertex -- to refine). Once
   * this drops below Epsilon(), Converged() becomes true and subsequent
   * Step() calls are no-ops that keep returning a sub-Epsilon value.
   */
  double Step();

  /**
   * Runs Step() until Converged() or @p maxIters total iterations have run
   * (mirrors Tutte::Run()'s exact shape, including treating @p maxIters as
   * a cap on Iteration(), not on how many additional steps this call runs).
   *
   * @return the number of iterations run since Begin().
   * @throws std::logic_error if Begin() has never been called -- the
   * distance table Step() depends on does not exist yet, matching
   * Tutte::Run()'s "no boundary pinned yet" usage-order precondition.
   */
  size_t Run(size_t maxIters);

  /** The unit edge length fixed at construction. */
  double EdgeLength() const noexcept { return edgeLength_; }

  /** The convergence threshold fixed at construction. */
  double Epsilon() const noexcept { return epsilon_; }

  /** Global spring stiffness K (default kDefaultStiffness); see the class
   *  doc for why this is a post-construction knob rather than a
   *  constructor parameter. */
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
   * The graph-theoretic (BFS hop-count) distance between raw vertices @p u
   * and @p v as computed by the last Begin(), or SIZE_MAX if Begin() has
   * never been called, either index is out of range, or the pair is
   * otherwise unknown. Exposed for introspection/testing -- the same table
   * Step()'s gradient/Hessian computation reads internally.
   */
  size_t GraphDistance(size_t u, size_t v) const noexcept;

private:
  static void ActionStep(EmbeddedGraph &embedding, void *userData,
                          const ActionPayload &payload);

  /** Hand-rolled BFS from @p source over Structure().Neighbors(), writing
   *  hop counts into @p dist (sized to n_, SIZE_MAX = unreachable). Mirrors
   *  GRIP::VerticesWithinRadius's shape/reasoning: it exists so this class
   *  never needs a Graph& just to satisfy search::BreadthFirst's full-
   *  subgraph-for-edge-tree-recording requirement (see the class doc). */
  void ComputeDistancesFrom(size_t source, std::vector<size_t> &dist) const;

  double ComputeGradientAndHessian(size_t m, double *grad,
                                    double *hessian) const;

  std::vector<size_t> distances_; // n_ * n_ row-major, SIZE_MAX = unreachable
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
