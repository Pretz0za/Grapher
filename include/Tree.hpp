#ifndef GVIZ_TREE_HPP
#define GVIZ_TREE_HPP

#include "Graph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::search {

/**
 * Outcome of IsTree(). More than a plain yes/no, and deliberately kept as
 * one checked enum rather than splitting Undirected off into a thrown
 * exception: IsTree is a side-effect-free predicate query, not a
 * constructor, and this port reserves exceptions for fallible
 * *construction* (see Error.hpp's class comment) -- something a caller
 * commits to once and either has or doesn't. A query a caller might run
 * speculatively against an arbitrary graph (directed or not, tree-shaped or
 * not) shouldn't require a try/catch just to ask the question safely; a
 * caller who *does* know in advance that directedness is a precondition
 * they control can still check Graph::IsDirected() themselves before
 * calling.
 */
enum class TreeCheckResult {
  IsTree,           /**< A directed tree rooted at a single vertex. */
  NotATree,         /**< A structurally valid single-root, in-degree<=1
                          candidate that still isn't a spanning tree -- e.g.
                          a disjoint cycle unreachable from the root, or
                          fewer than Size()-1 edges. */
  InvalidStructure, /**< Not even a valid rooted-tree candidate shape: more
                          than one vertex with in-degree 0 (multiple roots),
                          or some vertex with in-degree > 1 (a shared
                          parent). Mirrors the old C contract's -1 result,
                          whose doc comment named "multiple roots" only as
                          an example -- both conditions collapse into this
                          one outcome here, exactly as they did there. */
  Undirected,       /**< @p graph is undirected; tree-shape queries are only
                          meaningful on directed graphs, since "parent of"
                          is a directional relationship. */
};

/**
 * Tests whether @p graph is a directed tree rooted at a single vertex. When
 * @p outParents is non-null, it is resized to @p graph.Size() and filled
 * with each vertex's parent index (-1 for the root) -- filled regardless of
 * the outcome (IsTree, NotATree, or InvalidStructure alike), matching the
 * old C gvizGraphIsTree, which wrote into the caller's parents/scratch
 * buffer even on a non-tree result. Left untouched (not resized) when
 * @p graph is undirected, since no parent assignment was attempted.
 */
TreeCheckResult IsTree(const Graph &graph, std::vector<int> *outParents = nullptr);

/** Returns whether @p index is a leaf (zero out-degree) in @p tree.
 *  Unchecked -- matches Graph::Degree's own unchecked/hot-path contract. */
bool IsLeaf(const Graph &tree, size_t index) noexcept;

/**
 * Returns the number of leaves in the subtree rooted at @p root, via the
 * same recursive descent over @p tree's adjacency as the old C
 * gvizTreeCountLeaves. @p tree need not have passed IsTree first: like the
 * old version, this walks purely on out-edges with no cycle or
 * shared-parent detection, so calling it on a non-tree graph can recurse
 * indefinitely on a cycle or double-count through a shared child. Callers
 * for whom that matters should validate with IsTree first.
 */
size_t CountLeaves(const Graph &tree, size_t root);

} // namespace gviz::search

#endif
