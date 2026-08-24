#ifndef GVIZ_DEPTHFIRST_HPP
#define GVIZ_DEPTHFIRST_HPP

#include "GraphLike.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::search {

/**
 * Performs a depth-first search tree within @p g (any GraphLike) from
 * @p source, writing the tree into @p out. @p out must be a full subgraph
 * (Subgraph::CreateEmpty/CreateFull) on the same parent Graph @p g's
 * vertices ultimately belong to.
 *
 * @return true on success, false if @p source is not present in @p g or
 *         @p out is not a full subgraph.
 */
template <GraphLike G>
bool DepthFirst(const G &g, Subgraph &out, size_t source) {
  if (!out.IsFull())
    return false;
  if (!g.HasVertex(source))
    return false;

  BitSet seen(GraphLikeVertexCapacity(g));
  seen.Set(source);
  out.ShowVertex(source);

  std::vector<size_t> stack;
  stack.push_back(source);

  while (!stack.empty()) {
    size_t curr = stack.back();
    stack.pop_back();

    for (size_t neighbor : g.Neighbors(curr)) {
      if (seen.Test(neighbor))
        continue;
      seen.Set(neighbor);

      out.ShowVertex(neighbor);
      out.ShowEdge(curr, neighbor);

      stack.push_back(neighbor);
    }
  }

  return true;
}

} // namespace gviz::search

#endif
