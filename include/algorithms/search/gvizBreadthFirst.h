#ifndef GVIZ_SEARCH_BREADTHFIRST_H
#define GVIZ_SEARCH_BREADTHFIRST_H

#include "ds/gvizSubgraph.h"
#include <stddef.h>

/**
 * Performs a breadth-first search tree within @p sg from @p source.
 * Writes the tree into @p out (must be an empty subgraph on the same parent graph).
 * If @p distances is non-NULL, writes BFS depth for each reached vertex into
 * @p distances[0..nvertices-1] (SIZE_MAX where unreachable).
 *
 * @param maxDepth 0 for unlimited depth; otherwise stops expanding beyond this depth.
 *
 * @return 0 on success, -1 on invalid input or if @p source is not in @p sg.
 */
int gvizSearchBreadthFirst(const gvizSubgraph *sg, gvizSubgraph *out, size_t source,
                           size_t maxDepth, size_t *distances);

#endif
