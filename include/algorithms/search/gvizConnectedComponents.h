#ifndef GVIZ_CONNECTED_COMPONENTS_H
#define GVIZ_CONNECTED_COMPONENTS_H

#include "ds/gvizSubgraph.h"
#include <stddef.h>

/**
 * Labels the connected components of @p sg using repeated breadth-first search
 * (the standard graph-traversal connected-components algorithm; equivalent to
 * repeated depth-first search on each unseen vertex).
 *
 * Treats edges as undirected regardless of the parent graph's @c directed flag:
 * reachability follows @p sg adjacency in both directions.
 *
 * @param labels       Caller-owned array of length @c gvizGraphSize(sg->g).
 *                     On success, @p labels[v] is the 0-based component id for
 *                     each vertex @p v present in @p sg, and @c SIZE_MAX for
 *                     vertices outside @p sg.
 * @param out_count    If non-NULL, set to the number of components found.
 *
 * @return 0 on success, -1 on invalid input or allocation failure.
 */
int gvizConnectedComponents(const gvizSubgraph *sg, size_t *labels,
                            size_t *out_count);

/**
 * Tallies component sizes from a @p labels array produced by
 * gvizConnectedComponents.
 *
 * @param labels           Component label per vertex (SIZE_MAX = absent).
 * @param nvertices        Length of @p labels.
 * @param component_count  Number of components (max label + 1).
 * @param sizes            Caller-owned array of length @p component_count;
 *                         zeroed then incremented per labeled vertex.
 */
void gvizConnectedComponentSizes(const size_t *labels, size_t nvertices,
                               size_t component_count, size_t *sizes);

#endif
