#ifndef __GVIZ_SEARCH_DEPTHFIRST_H__
#define __GVIZ_SEARCH_DEPTHFIRST_H__

#include "ds/gvizSubgraph.h"
#include <stddef.h>

/**
 * Performs a depth-first search tree within @p sg from @p source.
 * Writes the tree into @p out (must be an empty subgraph on the same parent graph).
 *
 * @return 0 on success, -1 on invalid input or if @p source is not in @p sg.
 */
int gvizSearchDepthFirst(const gvizSubgraph *sg, gvizSubgraph *out, size_t source);

#endif
