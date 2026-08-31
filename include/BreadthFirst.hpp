#ifndef GVIZ_BREADTHFIRST_HPP
#define GVIZ_BREADTHFIRST_HPP

#include "Subgraph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::search {

/**
 * Performs a breadth-first search tree within @p sg from @p source, writing
 * the tree into @p out. Unlike the old C gvizSearchBreadthFirst, @p out
 * doesn't need to be pre-sized by the caller -- only pre-created as a full
 * subgraph (Subgraph::CreateEmpty/CreateFull) on the same parent Graph as
 * @p sg, so ShowEdge has an edge bitset to mark into; a vertex-induced
 * @p out is rejected the same way a null edge bitset was in the old C
 * contract, since there would be nowhere to record which edges are tree
 * edges.
 *
 * @param maxDepth  0 for unlimited depth; otherwise stops expanding beyond
 *                  this depth (a vertex at exactly @p maxDepth is still
 *                  reached and recorded, but its own neighbors are not
 *                  explored).
 * @param distances If non-null, resized to cover @p sg's addressable vertex
 *                  range and filled with BFS depth per vertex (SIZE_MAX
 *                  where unreachable) -- the caller no longer needs to know
 *                  the parent graph's size up front to pass a correctly
 *                  sized buffer, unlike the old C `size_t *distances`
 *                  out-array.
 *
 * @return true on success, false if @p source is not present in @p sg or
 *         @p out is not a full subgraph. Both are routine, checkable
 *         preconditions on an otherwise well-formed call -- not
 *         constructional failures -- so this never throws.
 */
bool BreadthFirst(const Subgraph &sg, Subgraph &out, size_t source, size_t maxDepth = 0,
                   std::vector<size_t> *distances = nullptr);

} // namespace gviz::search

#endif
