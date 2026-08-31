#ifndef GVIZ_TREE_HPP
#define GVIZ_TREE_HPP

#include "GraphLike.hpp"

#include <cstddef>
#include <vector>

namespace gviz::search {

/** Outcome of IsTree(). */
enum class TreeCheckResult {
  IsTree,           /**< A directed tree rooted at a single vertex. */
  NotATree,         /**< A structurally valid single-root, in-degree<=1
                          candidate that still isn't a spanning tree -- e.g.
                          a disjoint cycle unreachable from the root, or
                          fewer than Size()-1 edges. */
  InvalidStructure, /**< Not even a valid rooted-tree candidate shape: more
                          than one vertex with in-degree 0 (multiple roots),
                          or some vertex with in-degree > 1 (a shared
                          parent). */
  Undirected,       /**< @p graph is undirected; tree-shape queries are only
                          meaningful on directed graphs, since "parent of"
                          is a directional relationship. */
};

/**
 * Tests whether @p graph is a directed tree rooted at a single vertex. When
 * @p outParents is non-null, it is resized to @p graph.Size() and filled
 * with each vertex's parent index (-1 for the root), regardless of the
 * outcome. Left untouched (not resized) when @p graph is undirected.
 *
 * Requires @p graph.IsDirected(), which GraphLike itself does not
 * guarantee, so instantiating this over a type without it (e.g. Subgraph)
 * simply won't compile.
 */
template <GraphLike G>
TreeCheckResult IsTree(const G &graph, std::vector<int> *outParents = nullptr) {
  if (!graph.IsDirected())
    return TreeCheckResult::Undirected;

  size_t n = GraphLikeVertexCount(graph);
  std::vector<int> local;
  std::vector<int> &parents = outParents ? *outParents : local;
  parents.assign(n, -1);

  size_t edgeCount = 0;
  for (size_t i : graph) {
    for (size_t child : graph.Neighbors(i)) {
      if (parents[child] != -1)
        return TreeCheckResult::InvalidStructure; // in-degree > 1
      edgeCount++;
      parents[child] = static_cast<int>(i);
    }
  }

  int root = -1;
  for (size_t i = 0; i < n; i++) {
    if (parents[i] == -1) {
      if (root == -1)
        root = static_cast<int>(i);
      else
        return TreeCheckResult::InvalidStructure; // multiple roots
    }
  }

  if (edgeCount != n - 1)
    return TreeCheckResult::NotATree;

  // In-degree <= 1 plus a single root rules out shared parents but not a
  // disjoint directed cycle elsewhere, so also check that every vertex is
  // reachable from the root.
  std::vector<size_t> stack;
  stack.push_back(static_cast<size_t>(root));
  size_t reached = 0;
  while (!stack.empty()) {
    size_t v = stack.back();
    stack.pop_back();
    reached++;
    for (size_t child : graph.Neighbors(v))
      stack.push_back(child);
  }

  return reached == n ? TreeCheckResult::IsTree : TreeCheckResult::NotATree;
}

/** Returns whether @p index is a leaf (zero out-degree) in @p tree.
 *  Unchecked -- matches Graph::Degree's own unchecked/hot-path contract. */
template <GraphLike G>
bool IsLeaf(const G &tree, size_t index) noexcept {
  return tree.Degree(index) == 0;
}

/**
 * Returns the number of leaves in the subtree rooted at @p root, via
 * recursive descent over @p tree's adjacency. Walks purely on out-edges
 * with no cycle or shared-parent detection, so calling it on a non-tree
 * graph can recurse indefinitely on a cycle or double-count through a
 * shared child. Callers for whom that matters should validate with IsTree
 * first.
 */
template <GraphLike G>
size_t CountLeaves(const G &tree, size_t root) {
  if (IsLeaf(tree, root))
    return 1;

  size_t total = 0;
  for (size_t child : tree.Neighbors(root))
    total += CountLeaves(tree, child);
  return total;
}

} // namespace gviz::search

#endif
