#ifndef GVIZ_GRAPHLIKE_HPP
#define GVIZ_GRAPHLIKE_HPP

#include <concepts>
#include <cstddef>
#include <ranges>

namespace gviz {

/**
 * Structural concept capturing what every traversal algorithm and layout
 * embedder needs from "a graph": vertex range/iteration, HasVertex(u),
 * Degree(u), and Neighbors(u) yielding something usable as a vertex handle.
 * Says nothing about density, mutability, or ownership. Both gviz::Graph
 * and gviz::Subgraph satisfy it.
 */
template <typename G>
concept GraphLike = requires(const G &g, size_t u) {
  { g.HasVertex(u) } -> std::convertible_to<bool>;
  { g.Degree(u) } -> std::convertible_to<size_t>;
  requires std::ranges::input_range<G>;
  requires std::convertible_to<std::ranges::range_value_t<G>, size_t>;
  requires std::ranges::input_range<decltype(g.Neighbors(u))>;
  requires std::convertible_to<
      std::ranges::range_value_t<decltype(g.Neighbors(u))>, size_t>;
};

/** The number of vertices @p g's traversal surface actually addresses --
 *  not necessarily its handle range's upper bound. Prefers an O(1) accessor
 *  already exposed by @p g (Graph::Size(), Subgraph::VertexCount()) over an
 *  O(n) fallback walk of begin()/end(). */
template <GraphLike G>
size_t GraphLikeVertexCount(const G &g) {
  if constexpr (requires {
                  { g.Size() } -> std::convertible_to<size_t>;
                }) {
    return g.Size();
  } else if constexpr (requires {
                          { g.VertexCount() } -> std::convertible_to<size_t>;
                        }) {
    return g.VertexCount();
  } else {
    size_t n = 0;
    for ([[maybe_unused]] size_t u : g)
      n++;
    return n;
  }
}

/** An upper bound on @p g's vertex handles (every handle @p g can ever
 *  yield is < this), used to size scratch storage indexed directly by
 *  native handle. Prefers Subgraph's own VertexCapacity() when available;
 *  falls back to GraphLikeVertexCount(). */
template <GraphLike G>
size_t GraphLikeVertexCapacity(const G &g) {
  if constexpr (requires {
                  { g.VertexCapacity() } -> std::convertible_to<size_t>;
                }) {
    return g.VertexCapacity();
  } else {
    return GraphLikeVertexCount(g);
  }
}

/** Whether @p g's underlying graph is directed. Picks whichever accessor
 *  @p g has (Graph::IsDirected(), Subgraph::ParentIsDirected()); false if
 *  neither exists. */
template <GraphLike G>
bool GraphLikeIsDirected(const G &g) {
  if constexpr (requires {
                  { g.IsDirected() } -> std::convertible_to<bool>;
                }) {
    return g.IsDirected();
  } else if constexpr (requires {
                          { g.ParentIsDirected() } -> std::convertible_to<bool>;
                        }) {
    return g.ParentIsDirected();
  } else {
    return false;
  }
}

} // namespace gviz

#endif
