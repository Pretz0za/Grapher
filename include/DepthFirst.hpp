#ifndef GVIZ_DEPTHFIRST_HPP
#define GVIZ_DEPTHFIRST_HPP

#include "Subgraph.hpp"

#include <cstddef>

namespace gviz::search {

/**
 * Performs a depth-first search tree within @p sg from @p source, writing
 * the tree into @p out. Same @p out contract as BreadthFirst: must be a full
 * subgraph (Subgraph::CreateEmpty/CreateFull) on the same parent Graph as
 * @p sg.
 *
 * @return true on success, false if @p source is not present in @p sg or
 *         @p out is not a full subgraph -- routine, checkable preconditions,
 *         not constructional failures, so this never throws.
 */
bool DepthFirst(const Subgraph &sg, Subgraph &out, size_t source);

} // namespace gviz::search

#endif
