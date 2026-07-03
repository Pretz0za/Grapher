#ifndef __GVIZ_SEARCH_KNEAREST_H__
#define __GVIZ_SEARCH_KNEAREST_H__

#include "ds/gvizDeque.h"
#include "ds/gvizSubgraph.h"
#include <stddef.h>

typedef struct gvizFoundVertex {
  size_t v;
  size_t dist;
} gvizFoundVertex;

/**
 * Reusable search state: an epoch-stamped visited array and a queue that are
 * reset in O(1) per query instead of memsetting an O(N) bitset or allocating
 * a deque on every call. One scratch buffer per thread when searching in
 * parallel.
 */
typedef struct gvizKNearestScratch {
  size_t *stamp;
  size_t nvertices;
  size_t epoch;
  gvizDeque queue;
} gvizKNearestScratch;

/**
 * Initializes @p scratch for graphs with up to @p nvertices vertices.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizKNearestScratchInit(gvizKNearestScratch *scratch, size_t nvertices);

/** Releases memory owned by @p scratch. */
void gvizKNearestScratchRelease(gvizKNearestScratch *scratch);

/**
 * Finds up to @p k nearest vertices within @p sg from @p source by edge count.
 * Only vertices marked in @p filter are counted toward @p k; if @p filter is
 * NULL, all vertices in @p sg are eligible.
 *
 * @return The number of neighbors written to @p out, or a negative value on
 *         error.
 */
int gvizSearchKNearest(const gvizSubgraph *sg, gvizFoundVertex *out, size_t k,
                       size_t source, gvizVertexSubset filter);

/**
 * Same as gvizSearchKNearest but reuses @p scratch for visited tracking and
 * the BFS queue.
 */
int gvizSearchKNearestScratch(const gvizSubgraph *sg, gvizFoundVertex *out,
                              size_t k, size_t source, gvizVertexSubset filter,
                              gvizKNearestScratch *scratch);

#endif
