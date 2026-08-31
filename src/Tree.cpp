#include "Tree.hpp"

namespace gviz::search {

TreeCheckResult IsTree(const Graph &graph, std::vector<int> *outParents) {
  if (!graph.IsDirected())
    return TreeCheckResult::Undirected;

  size_t n = graph.Size();
  std::vector<int> local;
  std::vector<int> &parents = outParents ? *outParents : local;
  parents.assign(n, -1);

  size_t edgeCount = 0;
  for (size_t i = 0; i < n; i++) {
    size_t degree = graph.Degree(i);
    for (size_t j = 0; j < degree; j++) {
      size_t child = graph.Neighbor(i, j);
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

  // In-degree <= 1 everywhere plus a single root only rules out shared
  // parents; a disjoint directed cycle elsewhere still satisfies both. Count
  // the vertices reachable from the root: exactly n for a tree.
  std::vector<size_t> stack;
  stack.push_back(static_cast<size_t>(root));
  size_t reached = 0;
  while (!stack.empty()) {
    size_t v = stack.back();
    stack.pop_back();
    reached++;
    size_t degree = graph.Degree(v);
    for (size_t j = 0; j < degree; j++)
      stack.push_back(graph.Neighbor(v, j));
  }

  return reached == n ? TreeCheckResult::IsTree : TreeCheckResult::NotATree;
}

bool IsLeaf(const Graph &tree, size_t index) noexcept { return tree.Degree(index) == 0; }

size_t CountLeaves(const Graph &tree, size_t root) {
  if (IsLeaf(tree, root))
    return 1;

  size_t total = 0;
  size_t degree = tree.Degree(root);
  for (size_t i = 0; i < degree; i++)
    total += CountLeaves(tree, tree.Neighbor(root, i));
  return total;
}

} // namespace gviz::search
