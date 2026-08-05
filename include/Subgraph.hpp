#ifndef GVIZ_SUBGRAPH_HPP
#define GVIZ_SUBGRAPH_HPP

#include "BitSet.hpp"
#include "Graph.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>

namespace gviz {

/**
 * A view over a subset of a Graph's vertices and (optionally) edges. Single
 * concrete class with an internal mode discriminant -- vertex-induced
 * (edgeBits_ == nullptr: edges are implicit, "(u, v) present" means both
 * endpoints are shown and the edge exists in the parent) vs. full
 * (edgeBits_ != nullptr: an explicit, independently-editable edge bitset
 * addressed through the parent's layout). No vtable, no derived classes --
 * every embedder holds exactly one Subgraph regardless of which mode it's
 * in, and neighbor iteration (the hottest path here) never pays for virtual
 * dispatch.
 *
 * Vertex-induced subgraphs never touch the parent's layout: growth is
 * amortized-O(1) doubling of the vertex bitset (Rebuild), and queries
 * filter the parent's live adjacency lists on the fly. Full subgraphs
 * require a current layout (Graph::BuildLayout/EnsureLayout) and Rebuild()
 * is O(V+E) -- it remaps every edge bit, since the flat bit addressing
 * depends on the layout's prefix sums, which shift under any structural
 * parent mutation.
 *
 * Ownership: Subgraph owns its bitset(s) and holds a plain, never-null
 * `const Graph&` to its parent -- there is no "unbound" Subgraph state,
 * unlike the old C gvizSubgraph (which could have g == NULL on a failed
 * create). The routine "graph has no layout yet" precondition failure that
 * used to produce that null state is now NoLayoutError, thrown from
 * CreateEmpty/CreateFull. One consequence of the reference member: Subgraph
 * is move-constructible but NOT assignable (a reference can't be reseated).
 * That matches how it's actually used today -- constructed once, then kept
 * current via Rebuild() in place -- but a future caller that wants a
 * genuinely optional/replaceable subgraph slot (e.g. the old
 * gvizEmbeddedGraph::highlight, which is reset to an unbound subgraph and
 * later reassigned) should wrap it in std::optional<Subgraph> and use
 * .emplace()/.reset() rather than direct assignment.
 */
class Subgraph {
public:
  class NeighborIterator;

  /** Vertex-induced: owns only a vertex bitset sized for g.Size(); edges
   *  are implicit via parent adjacency filtering. Never requires a layout.
   *  Amortized-O(1) growth on Rebuild(). */
  static Subgraph CreateVertexInduced(const Graph &g);

  /** Full, with empty vertex and edge bitsets. Requires a current layout.
   *  @throws NoLayoutError if @p g has never called BuildLayout/EnsureLayout. */
  static Subgraph CreateEmpty(const Graph &g);

  /** Full, with every vertex and edge of @p g already shown.
   *  @throws NoLayoutError, same condition as CreateEmpty. */
  static Subgraph CreateFull(const Graph &g);

  Subgraph(const Subgraph &other);
  Subgraph(Subgraph &&other) noexcept = default;
  Subgraph &operator=(const Subgraph &) = delete;
  Subgraph &operator=(Subgraph &&) = delete;
  ~Subgraph() = default;

  /** Whether this is a full subgraph (has a materialized edge bitset), as
   *  opposed to vertex-induced. */
  bool IsFull() const noexcept { return edgeBits_ != nullptr; }

  /** Bits currently allocated for the vertex bitset (>= the live vertex
   *  count it was last synced to) -- exposes the amortized-doubling growth
   *  contract for vertex-induced mode; kept exactly in sync with the parent
   *  vertex count for full mode. */
  size_t VertexCapacity() const noexcept { return vertexBits_.Size(); }

  /**
   * Narrow, read-only passthroughs onto the parent graph -- NOT a general
   * escape hatch: Subgraph still never hands out a `const Graph&`/pointer to
   * its parent (see KNearest.hpp's design note on KNearestScratch sizing,
   * which is why nothing needed these before). EmbeddedGraph's sync/commit
   * machinery needs exactly these three scalar facts (the no-op-Sync check,
   * the raw vertex count admission watermark, and whether to build an
   * in-adjacency CSR) and nothing else about the parent graph, so it gets
   * exactly these three facts and no more.
   */
  uint64_t ParentMutationCount() const noexcept;
  size_t ParentSize() const noexcept;
  bool ParentIsDirected() const noexcept;

  bool HasVertex(size_t u) const noexcept;
  bool HasEdge(size_t u, size_t v) const noexcept;
  size_t Degree(size_t u) const noexcept;
  size_t VertexCount() const noexcept { return vertexBits_.Popcount(); }
  size_t EdgeCount() const noexcept;

  /** Marks @p u present. Unchecked (matches the old C contract, which never
   *  bounds-checked ShowVertex/HideVertex either). */
  void ShowVertex(size_t u) noexcept { vertexBits_.Set(u); }

  /** Marks @p u absent; also clears its incident edges when this is a full
   *  subgraph. Unchecked. */
  void HideVertex(size_t u) noexcept;

  /** Marks edge (u, v) present. No-op on vertex-induced subgraphs (there is
   *  no explicit edge bitset to mark) and if the edge doesn't exist in the
   *  parent. */
  void ShowEdge(size_t u, size_t v) noexcept;

  /** Marks edge (u, v) absent. Same no-op conditions as ShowEdge. */
  void HideEdge(size_t u, size_t v) noexcept;

  /**
   * Materializes the edge bitset for a vertex-induced subgraph, converting
   * it to full. No-op if already full or if the parent has no layout yet
   * (a routine "not ready" condition on an existing, live object -- unlike
   * CreateEmpty/CreateFull, which throw, this deliberately doesn't: it's a
   * mutator on an object that stays perfectly valid either way, not a
   * constructor).
   */
  void MakeEdgeSubset();

  /** Marks every vertex and edge of the parent present. No-op unless this
   *  is already a full subgraph with a current parent layout. */
  void MakeFull() noexcept;

  /**
   * Re-syncs this subgraph after the parent graph's structure changed.
   * Vertex-induced: grows the vertex bitset to cover the current vertex
   * count, doubling capacity as needed; never touches the parent's layout.
   * Full: rebuilds the parent's layout unconditionally and remaps every
   * edge bit against it -- O(V+E) regardless of how much changed, since
   * previously-shown (u, v) pairs are re-found by vertex id, not by their
   * old bit position (which may have shifted).
   */
  void Rebuild();

  /** Iterates the vertices present in this subgraph, in ascending order. */
  BitSet::iterator begin() const noexcept { return vertexBits_.begin(); }
  BitSet::iterator end() const noexcept { return vertexBits_.end(); }

  class NeighborRange {
  public:
    NeighborIterator begin() const;
    NeighborIterator end() const;

  private:
    friend class Subgraph;
    NeighborRange(const Subgraph &sg, size_t u) : sg_(&sg), u_(u) {}
    const Subgraph *sg_;
    size_t u_;
  };

  /** Iterates the neighbors of @p u within this subgraph. Unchecked: @p u
   *  must be a vertex currently present (HasVertex(u)), otherwise this
   *  quietly yields nothing rather than validating. */
  NeighborRange Neighbors(size_t u) const noexcept { return NeighborRange(*this, u); }

  /** Real, single-pass C++ iterator over a Subgraph's neighbors of one
   *  vertex; satisfies std::input_iterator. Dispatches on the parent
   *  subgraph's mode exactly like the old
   *  gvizSubgraphNeighborIterator/Iterate did. */
  class NeighborIterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const size_t *;
    using reference = size_t;

    NeighborIterator() = default;

    size_t operator*() const noexcept { return current_; }

    NeighborIterator &operator++() {
      Step();
      return *this;
    }
    NeighborIterator operator++(int) {
      NeighborIterator tmp = *this;
      Step();
      return tmp;
    }

    bool operator==(const NeighborIterator &other) const noexcept {
      return done_ == other.done_;
    }
    bool operator!=(const NeighborIterator &other) const noexcept {
      return !(*this == other);
    }

  private:
    friend class Subgraph;

    enum class Mode { Full, Induced };

    bool Step();

    const Subgraph *sg_ = nullptr;
    size_t u_ = 0;
    size_t base_ = 0;
    BitSet::iterator edgeIt_{};
    BitSet::iterator edgeEnd_{};
    size_t adjIdx_ = 0;
    size_t adjDegree_ = 0;
    size_t current_ = 0;
    Mode mode_ = Mode::Induced;
    bool done_ = true;
  };

private:
  explicit Subgraph(const Graph &g)
      : g_(g), vertexBits_(g.Size()), edgeBits_(nullptr) {}

  void ClearIncidentEdges(size_t u) noexcept;
  void RebuildVertexInduced();
  void RebuildFull();
  NeighborIterator MakeNeighborIterator(size_t u) const noexcept;

  const Graph &g_;
  BitSet vertexBits_;
  /** null = vertex-induced mode; non-null = full mode (owns the edge
   *  bitset). This pointer's nullness IS the mode discriminant. */
  std::unique_ptr<BitSet> edgeBits_;
};

inline Subgraph::NeighborIterator Subgraph::NeighborRange::begin() const {
  return sg_->MakeNeighborIterator(u_);
}
inline Subgraph::NeighborIterator Subgraph::NeighborRange::end() const {
  return NeighborIterator();
}

static_assert(std::input_iterator<Subgraph::NeighborIterator>);

} // namespace gviz

#endif
