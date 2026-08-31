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
 * A view over a subset of a Graph's vertices and (optionally) edges.
 * Vertex-induced subgraphs have implicit edges and never touch the parent's
 * layout. Full subgraphs additionally materialize an explicit edge bitset
 * addressed through the parent's layout. Holds a `const Graph&` to its
 * parent: move-constructible, not assignable, no default state.
 */
class Subgraph {
public:
  class NeighborIterator;

  /** Vertex-induced subgraph over @p g, initially empty. */
  static Subgraph CreateVertexInduced(const Graph &g);

  /** Full subgraph over @p g with empty vertex and edge bitsets.
   *  @throws NoLayoutError if @p g has no layout (BuildLayout/EnsureLayout). */
  static Subgraph CreateEmpty(const Graph &g);

  /** Full subgraph over @p g with every vertex and edge shown.
   *  @throws NoLayoutError, same condition as CreateEmpty. */
  static Subgraph CreateFull(const Graph &g);

  Subgraph(const Subgraph &other);
  Subgraph(Subgraph &&other) noexcept = default;
  Subgraph &operator=(const Subgraph &) = delete;
  Subgraph &operator=(Subgraph &&) = delete;
  ~Subgraph() = default;

  /** Whether this is a full subgraph, as opposed to vertex-induced. */
  bool IsFull() const noexcept { return edgeBits_ != nullptr; }

  /** Bits currently allocated for the vertex bitset. */
  size_t VertexCapacity() const noexcept { return vertexBits_.Size(); }

  /** Read-only passthroughs to the parent graph. */
  uint64_t ParentMutationCount() const noexcept;
  size_t ParentSize() const noexcept;
  bool ParentIsDirected() const noexcept;

  bool HasVertex(size_t u) const noexcept;
  bool HasEdge(size_t u, size_t v) const noexcept;
  size_t Degree(size_t u) const noexcept;
  size_t VertexCount() const noexcept { return vertexBits_.Popcount(); }
  size_t EdgeCount() const noexcept;

  /** Marks @p u present. Unchecked. */
  void ShowVertex(size_t u) noexcept { vertexBits_.Set(u); }

  /** Marks @p u absent; also clears its incident edges if full. Unchecked. */
  void HideVertex(size_t u) noexcept;

  /** Marks edge (u, v) present. No-op on vertex-induced subgraphs or if the
   *  edge doesn't exist in the parent. */
  void ShowEdge(size_t u, size_t v) noexcept;

  /** Marks edge (u, v) absent. Same no-op conditions as ShowEdge. */
  void HideEdge(size_t u, size_t v) noexcept;

  /** Converts a vertex-induced subgraph to full by materializing its edge
   *  bitset. No-op if already full or if the parent has no layout yet. */
  void MakeEdgeSubset();

  /** Marks every vertex and edge of the parent present. No-op unless
   *  already full with a current parent layout. */
  void MakeFull() noexcept;

  /** Re-syncs after the parent graph's structure changed. Vertex-induced:
   *  grows the vertex bitset, amortized O(1). Full: rebuilds the parent
   *  layout and remaps every edge bit, O(V+E). */
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
   *  must satisfy HasVertex(u). */
  NeighborRange Neighbors(size_t u) const noexcept { return NeighborRange(*this, u); }

  /** Single-pass iterator over a Subgraph's neighbors of one vertex;
   *  satisfies std::input_iterator. */
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
