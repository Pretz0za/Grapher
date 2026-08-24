#ifndef GVIZ_GRAPH_HPP
#define GVIZ_GRAPH_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <vector>

namespace gviz {

/** A single directed adjacency-list entry: the neighboring vertex's index
 *  and the weight of the edge to it. Also implicitly convertible to the
 *  neighbor's raw vertex id, so `for (size_t nb : g.Neighbors(u))` works
 *  uniformly with Subgraph's neighbor iteration. */
struct Edge {
  size_t idx;
  double weight;

  operator size_t() const noexcept { return idx; }
};

/**
 * A directed or undirected adjacency-list graph. Each vertex holds its
 * adjacency list as a plain std::vector<Edge>.
 *
 * mutationCount is bumped by every structural mutation (AddVertex, AddEdge,
 * RemoveEdge, InsertNeighborAt, Clear) and by nothing else -- not by
 * weight/data updates, not by ReorderNeighbors. This lets a derived view
 * answer "has the graph changed since I last looked?" with one integer
 * compare.
 *
 * Degree/Neighbor/NeighborWeight are unchecked (no bounds validation) --
 * every embedder's inner loop walks these, same contract as
 * std::vector::operator[]. Passing an out-of-range vertex index to any
 * mutator below is likewise a precondition violation, not a checked
 * failure mode, except where a method's doc comment says otherwise.
 *
 * Copy/move: Graph has real value semantics (a copy is already a deep
 * copy). A copy never carries over the source's cached layout (BuildLayout
 * must be called again on the copy if needed).
 */
class Graph {
public:
  /** Constructs a graph with @p initialCapacity vertices reserved. */
  explicit Graph(bool directed, size_t initialCapacity = 64);

  Graph(const Graph &other);
  Graph &operator=(const Graph &other);
  Graph(Graph &&other) noexcept = default;
  Graph &operator=(Graph &&other) noexcept = default;
  ~Graph() = default;

  size_t Size() const noexcept { return vertices_.size(); }
  bool IsDirected() const noexcept { return directed_; }
  uint64_t MutationCount() const noexcept { return mutationCount_; }

  /** Tells gviz::DenseIndex that Graph's vertex handles are already a
   *  dense [0, Size()) range, so no raw<->local bijection is needed. */
  static constexpr bool kDenseVertexHandles = true;

  /** Whether @p u names a currently-valid vertex. O(1). */
  bool HasVertex(size_t u) const noexcept { return u < vertices_.size(); }

  /** Iterates vertex ids [0, Size()) in ascending order. */
  auto begin() const noexcept {
    return std::ranges::iota_view<size_t, size_t>(0, vertices_.size()).begin();
  }
  auto end() const noexcept {
    return std::ranges::iota_view<size_t, size_t>(0, vertices_.size()).end();
  }

  /** Number of edges. Requires a current layout (BuildLayout/EnsureLayout);
   *  0 if no layout has ever been built. */
  size_t EdgeCount() const noexcept;

  /** True once BuildLayout/EnsureLayout has run at least once. */
  bool HasLayout() const noexcept { return layout_ != nullptr; }

  /** (Re)builds the shared edge-bitset layout unconditionally. Const:
   *  building the layout only refreshes an internal cache (layout_ is
   *  `mutable`), letting Subgraph hold a plain `const Graph&` and still
   *  trigger a rebuild during its own Rebuild(). */
  void BuildLayout() const;

  /** Rebuilds the layout only if stale (one integer compare against
   *  MutationCount()). A rebuild shifts the shared bit addressing under
   *  any full-mode Subgraph over this graph -- Subgraph::Rebuild() is what
   *  re-syncs it. */
  void EnsureLayout() const;

  /** Returns a new graph with every edge (u, v) replaced by (v, u).
   *  Undirected graphs are unaffected by reversal, so this just returns a
   *  copy for them. Vertex data is preserved; the result never carries
   *  over a layout. */
  Graph Reversed() const;

  /** Adds a vertex holding @p data (default nullptr) and returns its index.
   *  Bumps MutationCount(). */
  size_t AddVertex(void *data = nullptr);

  /** Removes every vertex. Bumps MutationCount() and drops the cached
   *  layout; a no-op when already empty. */
  void Clear();

  /** Adds edge (from, to) with the given weight; mirrors to (to, from) for
   *  undirected graphs. Bumps MutationCount(). Unchecked: @p from and @p to
   *  must be valid vertex indices. */
  void AddEdge(size_t from, size_t to, double weight);

  /**
   * Removes edge (from, to) (and its mirror for undirected graphs). Returns
   * whether the edge existed and was removed -- the found/not-found outcome
   * is routine, checkable control flow, unlike @p from/@p to being out of
   * range, which is unchecked/UB. Bumps MutationCount() only when it
   * actually removes something.
   */
  bool RemoveEdge(size_t from, size_t to);

  /** Out-of-range idx returns nullptr (checked). */
  void *GetVertexData(size_t idx) const;

  /** Unchecked. */
  void SetVertexData(size_t idx, void *data) noexcept;

  /** Out-degree of vertex @p idx. Unchecked -- hot path, see class doc. */
  size_t Degree(size_t idx) const noexcept { return vertices_[idx].edges.size(); }

  /** Vertex id of the @p i-th neighbor of @p idx. Unchecked -- hot path. */
  size_t Neighbor(size_t idx, size_t i) const noexcept {
    return vertices_[idx].edges[i].idx;
  }

  /** Weight of the edge to the @p i-th neighbor of @p idx. Unchecked. */
  double NeighborWeight(size_t idx, size_t i) const noexcept {
    return vertices_[idx].edges[i].weight;
  }

  /** @p idx's adjacency list; range-for over this for free neighbor
   *  iteration. Unchecked. */
  const std::vector<Edge> &Neighbors(size_t idx) const noexcept {
    return vertices_[idx].edges;
  }

  /** Finds the position of @p to in @p from's adjacency list; writes it to
   *  @p outPos and returns true if found, false (leaving @p outPos
   *  unmodified) otherwise. @p from out of range is unchecked/UB. */
  bool NeighborPosition(size_t from, size_t to, size_t &outPos) const noexcept;

  /** Whether edge (from, to) exists. @p from/@p to out of range is
   *  unchecked/UB (same contract as NeighborPosition, which this calls). */
  bool EdgeExists(size_t from, size_t to) const noexcept;

  /** Writes the weight of edge (from, to) to @p outWeight and returns true
   *  if the edge exists, false otherwise. @p from out of range is
   *  unchecked/UB. */
  bool GetEdgeWeight(size_t from, size_t to, double &outWeight) const noexcept;

  /** Sets the weight of edge (from, to) (and its mirror, for undirected
   *  graphs); returns whether the edge existed. Does not bump
   *  MutationCount() -- a weight update is not structural. @p from/@p to
   *  out of range is unchecked/UB. */
  bool SetEdgeWeight(size_t from, size_t to, double weight) noexcept;

  /**
   * Inserts a single directed adjacency entry (to, weight) into @p from's
   * adjacency list at position @p pos, shifting later entries back. Does
   * NOT mirror to @p to's own list -- intended for rotation-system
   * bookkeeping (e.g. planar embedding), where each endpoint's insertion
   * position is computed and applied independently. Bumps MutationCount()
   * on success. Returns false if @p pos is out of [0, Degree(from)]
   * (a checkable, expected outcome, unlike @p from itself being out of
   * range, which is unchecked/UB).
   */
  bool InsertNeighborAt(size_t from, size_t to, double weight, size_t pos);

  /**
   * Rewrites @p idx's adjacency list to match the vertex-id order given by
   * @p order, preserving each entry's existing weight. Returns false
   * (leaving the adjacency list unchanged) if @p order's size doesn't match
   * the current degree or isn't a permutation of the current neighbor ids.
   * Does not bump MutationCount() -- reordering changes no topology.
   * @p idx out of range is unchecked/UB.
   */
  bool ReorderNeighbors(size_t idx, const std::vector<size_t> &order);

private:
  friend class Subgraph;

  struct Vertex {
    void *data = nullptr;
    std::vector<Edge> edges;
  };

  /** Shared edge-bitset layout: prefix sums over adjacency-list lengths.
   *  Subgraph reaches in via friendship for its edge-bit addressing;
   *  ordinary callers only see it through EdgeCount()/HasLayout()/
   *  BuildLayout()/EnsureLayout(). */
  struct Layout {
    std::vector<size_t> vertexOffsets;
    size_t edgeCount = 0;
    uint64_t builtAtMutation = 0;
  };

  std::vector<Vertex> vertices_;
  bool directed_;
  uint64_t mutationCount_ = 0;
  mutable std::unique_ptr<Layout> layout_;
};

} // namespace gviz

#endif
