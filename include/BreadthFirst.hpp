#ifndef GVIZ_BREADTHFIRST_HPP
#define GVIZ_BREADTHFIRST_HPP

#include "GraphLike.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <deque>
#include <limits>
#include <vector>

namespace gviz::search {

/**
 * Performs a breadth-first search from @p source over any GraphLike @p g,
 * filling @p distances (resized to cover @p g's addressable native-handle
 * range -- see gviz::GraphLikeVertexCapacity) with BFS hop count per vertex,
 * SIZE_MAX where unreachable. See BreadthFirstTree() below for callers that
 * want the tree, not just distances.
 *
 * @param maxDepth  0 for unlimited depth; otherwise stops expanding beyond
 *                  this depth (a vertex at exactly @p maxDepth is still
 *                  reached and recorded, but its own neighbors are not
 *                  explored).
 *
 * If @p source is not present in @p g (HasVertex(source) == false),
 * @p distances still comes back correctly sized with every entry SIZE_MAX.
 */
template <GraphLike G>
void BreadthFirst(const G &g, size_t source, std::vector<size_t> &distances,
                   size_t maxDepth = 0) {
  size_t n = GraphLikeVertexCapacity(g);
  distances.assign(n, std::numeric_limits<size_t>::max());

  if (!g.HasVertex(source))
    return;

  distances[source] = 0;

  std::deque<size_t> queue;
  queue.push_back(source);

  while (!queue.empty()) {
    size_t u = queue.front();
    queue.pop_front();
    size_t d = distances[u];

    if (maxDepth && d >= maxDepth)
      continue;

    for (size_t v : g.Neighbors(u)) {
      if (distances[v] != std::numeric_limits<size_t>::max())
        continue;
      distances[v] = d + 1;
      queue.push_back(v);
    }
  }
}

/**
 * Performs a breadth-first search tree within @p sg from @p source, writing
 * the tree into @p out. @p out must be a full subgraph
 * (Subgraph::CreateEmpty/CreateFull) on the same parent Graph as @p sg, so
 * ShowEdge has an edge bitset to mark tree edges into.
 *
 * @param distances If non-null, resized to cover @p sg's addressable vertex
 *                  range and filled with BFS depth per vertex (SIZE_MAX
 *                  where unreachable).
 *
 * @return true on success, false if @p source is not present in @p sg or
 *         @p out is not a full subgraph.
 */
bool BreadthFirstTree(const Subgraph &sg, Subgraph &out, size_t source, size_t maxDepth = 0,
                       std::vector<size_t> *distances = nullptr);

} // namespace gviz::search

#endif
