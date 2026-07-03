#ifndef __GVIZ_SEARCH_KNEAREST_H__
#define __GVIZ_SEARCH_KNEAREST_H__

#include "ds/gvizSubgraph.h"
#include <stddef.h>

typedef struct gvizFoundVertex {
  size_t v;
  size_t dist;
} gvizFoundVertex;

/**
 * Finds up to @p k nearest vertices within @p sg from @p source by edge count.
 * Only vertices marked in @p filter are counted toward @p k; if @p filter is NULL,
 * all vertices in @p sg are eligible.
 *
 * @return The number of neighbors written to @p out, or a negative value on error.
 */
int gvizSearchKNearest(const gvizSubgraph *sg, gvizFoundVertex *out, size_t k,
                       size_t source, gvizVertexSubset filter);

#endif
