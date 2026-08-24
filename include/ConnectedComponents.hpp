#ifndef GVIZ_CONNECTEDCOMPONENTS_HPP
#define GVIZ_CONNECTEDCOMPONENTS_HPP

#include "GraphLike.hpp"

#include <cstddef>
#include <deque>
#include <limits>
#include <vector>

namespace gviz::search {

/**
 * Result of ConnectedComponents(): @c labels holds one 0-based component id
 * per vertex handle in @p g's addressable range (gviz::GraphLikeVertexCapacity),
 * SIZE_MAX for handles not present in @p g; @c count is the number of
 * components found (0 for an empty view).
 */
struct Components {
  std::vector<size_t> labels;
  size_t count = 0;
};

/**
 * Labels the connected components of any GraphLike @p g using repeated
 * breadth-first search. Never fails; always returns a result.
 *
 * Walks @p g's adjacency exactly as given: for an undirected parent Graph,
 * edges are stored symmetrically in both endpoints' lists, so components
 * come out as true connected components; for a directed parent Graph, only
 * outgoing edges are walked, so two vertices joined only by a reverse edge
 * end up in different components.
 */
template <GraphLike G>
Components ConnectedComponents(const G &g) {
  constexpr size_t kUnlabeled = std::numeric_limits<size_t>::max();

  size_t n = GraphLikeVertexCapacity(g);
  Components result;
  result.labels.assign(n, kUnlabeled);

  std::deque<size_t> queue;
  size_t comp = 0;
  for (size_t i : g) {
    if (result.labels[i] != kUnlabeled)
      continue;

    result.labels[i] = comp;
    queue.push_back(i);

    while (!queue.empty()) {
      size_t curr = queue.front();
      queue.pop_front();

      for (size_t neighbor : g.Neighbors(curr)) {
        if (result.labels[neighbor] != kUnlabeled)
          continue;
        result.labels[neighbor] = comp;
        queue.push_back(neighbor);
      }
    }
    comp++;
  }

  result.count = comp;
  return result;
}

/**
 * Tallies component sizes from a Components::labels array. Returns a vector
 * of length @p componentCount; index i holds the number of vertices labeled
 * i.
 */
std::vector<size_t> ConnectedComponentSizes(const std::vector<size_t> &labels,
                                             size_t componentCount);

} // namespace gviz::search

#endif
