#ifndef __GVIZ_SEARCH_KNEAREST_H__
#define __GVIZ_SEARCH_KNEAREST_H__

#include "ds/gvizDeque.h"
#include "ds/gvizSubgraph.h"
#include <stddef.h>

/** Max visible seeds for the batched KNN path (see gvizSearchKNearestFromVisibleBatch). */
#define GVIZ_KNN_BATCH_VISIBLE_MAX 256
/** Avg degree above this: per-vertex KNN tends to scan the whole component. */
#define GVIZ_KNN_BATCH_MIN_AVG_DEGREE 32.0
/** On sparse graphs, batch only when targets dwarf the visible seed count. */
#define GVIZ_KNN_BATCH_TARGET_RATIO 8

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

typedef struct gvizKNNBatchTarget {
  size_t vertex;
  gvizFoundVertex *out;
  size_t count;
} gvizKNNBatchTarget;

/**
 * Finds up to @p k nearest @p visible vertices for each entry in @p targets by
 * running one BFS per visible seed instead of one BFS per target. Intended when
 * @c |visible| is much smaller than @c |targets| (early GRIP layers).
 *
 * Each @p targets[i].out must hold at least @p k slots. On success,
 * @p targets[i].count is the number of neighbors written (may be less than @p k
 * if fewer visible vertices are reachable).
 *
 * @return 0 on success, -1 on error, 1 if the batch path does not apply
 *         (@p visible is NULL, empty, larger than @ref GVIZ_KNN_BATCH_VISIBLE_MAX,
 *         or not smaller than @p targetCount).
 */
int gvizSearchKNearestFromVisibleBatch(const gvizSubgraph *sg,
                                       gvizVertexSubset visible, size_t k,
                                       gvizKNNBatchTarget *targets,
                                       size_t targetCount,
                                       gvizKNearestScratch *scratch);

/**
 * Heuristic for whether batched KNN is likely faster than per-vertex search.
 * On sparse graphs per-vertex BFS stops early; batch always runs |visible|
 * full sweeps and is slower unless the graph is hub-heavy or there are many
 * more targets than visible seeds.
 */
int gvizSearchKNearestPreferBatch(const gvizSubgraph *sg, size_t visibleCount,
                                  size_t targetCount);

/** Resets counters used when @c GVIZ_KNN_PROFILE is set in the environment. */
void gvizKNNProfileReset(void);

/** Reads cumulative KNN BFS visit stats from @ref gvizKNNProfileReset. */
void gvizKNNProfileSnapshot(unsigned long long *queries,
                            unsigned long long *visited,
                            unsigned long long *maxVisited);

#endif
