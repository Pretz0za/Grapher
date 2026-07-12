#include "algorithms/search/gvizKNearest.h"
#include "core/alloc.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

typedef struct gvizKNNProfileTotals {
  _Atomic unsigned long long queries;
  _Atomic unsigned long long visited;
  _Atomic unsigned long long maxVisited;
} gvizKNNProfileTotals;

static gvizKNNProfileTotals gvizKnnProfile;

void gvizKNNProfileReset(void) {
  atomic_store(&gvizKnnProfile.queries, 0);
  atomic_store(&gvizKnnProfile.visited, 0);
  atomic_store(&gvizKnnProfile.maxVisited, 0);
}

void gvizKNNProfileSnapshot(unsigned long long *queries,
                            unsigned long long *visited,
                            unsigned long long *maxVisited) {
  if (queries)
    *queries = atomic_load(&gvizKnnProfile.queries);
  if (visited)
    *visited = atomic_load(&gvizKnnProfile.visited);
  if (maxVisited)
    *maxVisited = atomic_load(&gvizKnnProfile.maxVisited);
}

static bool search_input_valid(const gvizSubgraph *sg) {
  return sg && sg->g && sg->g->layout && sg->vs;
}

static void knn_profile_record(size_t visited) {
  if (!getenv("GVIZ_KNN_PROFILE"))
    return;
  atomic_fetch_add_explicit(&gvizKnnProfile.queries, 1, memory_order_relaxed);
  atomic_fetch_add_explicit(&gvizKnnProfile.visited, visited,
                            memory_order_relaxed);
  unsigned long long prev = atomic_load(&gvizKnnProfile.maxVisited);
  while (visited > prev &&
         !atomic_compare_exchange_weak(&gvizKnnProfile.maxVisited, &prev,
                                       visited))
    ;
}

static void knn_insert_sorted(gvizFoundVertex *out, size_t *count, size_t k,
                              size_t v, size_t dist) {
  if (dist == SIZE_MAX)
    return;

  gvizFoundVertex cand = {v, dist};
  size_t n = *count;
  if (n < k) {
    out[n] = cand;
    (*count)++;
    for (size_t i = n; i > 0 && out[i].dist < out[i - 1].dist; i--) {
      gvizFoundVertex tmp = out[i];
      out[i] = out[i - 1];
      out[i - 1] = tmp;
    }
    return;
  }

  if (dist >= out[k - 1].dist)
    return;

  out[k - 1] = cand;
  for (size_t i = k - 1; i > 0 && out[i].dist < out[i - 1].dist; i--) {
    gvizFoundVertex tmp = out[i];
    out[i] = out[i - 1];
    out[i - 1] = tmp;
  }
}

static int knn_bfs_from_seed(const gvizSubgraph *sg, size_t seed,
                             const size_t *targetMap, gvizKNNBatchTarget *targets,
                             size_t k, gvizKNearestScratch *scratch) {
  size_t n = sg->g->layout->nvertices;
  size_t epoch = ++scratch->epoch;
  if (epoch == 0) {
    memset(scratch->stamp, 0, sizeof(size_t) * n);
    epoch = scratch->epoch = 1;
  }

  gvizDeque *queue = &scratch->queue;
  queue->count = 0;
  queue->begin = NULL;

  scratch->stamp[seed] = epoch;

  gvizFoundVertex start = {seed, 0};
  if (gvizDequePush(queue, &start) < 0)
    return -1;

  size_t visited = 1;
  while (!gvizDequeIsEmpty(queue)) {
    gvizFoundVertex curr;
    gvizDequePopLeft(queue, &curr);

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr.v);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (scratch->stamp[neighbor] == epoch)
        continue;
      scratch->stamp[neighbor] = epoch;
      visited++;

      size_t dist = curr.dist + 1;
      size_t ti = targetMap[neighbor];
      if (ti != SIZE_MAX)
        knn_insert_sorted(targets[ti].out, &targets[ti].count, k, seed, dist);

      gvizFoundVertex next = {neighbor, dist};
      if (gvizDequePush(queue, &next) < 0)
        return -1;
    }
  }

  knn_profile_record(visited);
  return 0;
}

static bool knn_all_targets_full(const gvizKNNBatchTarget *targets,
                                 size_t targetCount, size_t k) {
  for (size_t i = 0; i < targetCount; i++) {
    if (targets[i].count < k)
      return false;
  }
  return true;
}

int gvizSearchKNearestPreferBatch(const gvizSubgraph *sg, size_t visibleCount,
                                  size_t targetCount) {
  if (!search_input_valid(sg) || visibleCount == 0 ||
      visibleCount >= targetCount ||
      visibleCount > GVIZ_KNN_BATCH_VISIBLE_MAX)
    return 0;

  size_t vc = gvizSubgraphVertexCount(sg);
  if (vc == 0)
    return 0;

  size_t ec = gvizSubgraphEdgeCount(sg);
  double avgDeg = (2.0 * (double)ec) / (double)vc;
  if (avgDeg > GVIZ_KNN_BATCH_MIN_AVG_DEGREE)
    return 1;

  return targetCount >= GVIZ_KNN_BATCH_TARGET_RATIO * visibleCount;
}

int gvizSearchKNearestFromVisibleBatch(const gvizSubgraph *sg,
                                       gvizVertexSubset visible, size_t k,
                                       gvizKNNBatchTarget *targets,
                                       size_t targetCount,
                                       gvizKNearestScratch *scratch) {
  if (k == 0 || !visible || targetCount == 0 || !targets || !scratch ||
      !scratch->stamp || !search_input_valid(sg))
    return 1;

  size_t n = sg->g->layout->nvertices;
  if (scratch->nvertices < n)
    return -1;

  size_t visibleCount = gvizVertexSubsetCount(visible, n);
  if (visibleCount == 0 || visibleCount > GVIZ_KNN_BATCH_VISIBLE_MAX ||
      visibleCount >= targetCount)
    return 1;

  size_t *targetMap = GVIZ_ALLOC(n * sizeof(size_t));
  if (!targetMap)
    return -1;
  for (size_t i = 0; i < n; i++)
    targetMap[i] = SIZE_MAX;
  for (size_t i = 0; i < targetCount; i++)
    targetMap[targets[i].vertex] = i;

  for (size_t i = 0; i < targetCount; i++)
    targets[i].count = 0;

  gvizBitArrayIterator it = gvizVertexSubsetIteratorCreate(visible, n);
  size_t vtx;
  while (gvizBitArrayIterate(&it, &vtx)) {
    if (knn_bfs_from_seed(sg, vtx, targetMap, targets, k, scratch) < 0) {
      GVIZ_DEALLOC(targetMap);
      return -1;
    }
    if (knn_all_targets_full(targets, targetCount, k))
      break;
  }

  GVIZ_DEALLOC(targetMap);
  return 0;
}

int gvizKNearestScratchInit(gvizKNearestScratch *scratch, size_t nvertices) {
  if (!scratch || nvertices == 0)
    return -1;

  scratch->stamp = GVIZ_ALLOC(sizeof(size_t) * nvertices);
  if (!scratch->stamp)
    return -1;

  scratch->nvertices = nvertices;
  scratch->epoch = 1;
  memset(scratch->stamp, 0, sizeof(size_t) * nvertices);

  if (gvizDequeInitAtCapacity(&scratch->queue, sizeof(gvizFoundVertex), 256) <
      0) {
    GVIZ_DEALLOC(scratch->stamp);
    scratch->stamp = NULL;
    return -1;
  }

  return 0;
}

void gvizKNearestScratchRelease(gvizKNearestScratch *scratch) {
  if (!scratch)
    return;
  if (scratch->stamp)
    GVIZ_DEALLOC(scratch->stamp);
  gvizDequeRelease(&scratch->queue);
  scratch->stamp = NULL;
  scratch->nvertices = 0;
}

int gvizSearchKNearestScratch(const gvizSubgraph *sg, gvizFoundVertex *out,
                              size_t k, size_t source, gvizVertexSubset filter,
                              gvizKNearestScratch *scratch) {
  if (k == 0)
    return 0;
  if (!out || !scratch || !scratch->stamp || !search_input_valid(sg))
    return -1;
  if (!gvizSubgraphHasVertex(sg, source))
    return -1;

  size_t n = sg->g->layout->nvertices;
  if (scratch->nvertices < n)
    return -1;

  size_t epoch = ++scratch->epoch;
  if (epoch == 0) {
    memset(scratch->stamp, 0, sizeof(size_t) * n);
    epoch = scratch->epoch = 1;
  }

  gvizDeque *queue = &scratch->queue;
  queue->count = 0;
  queue->begin = NULL;

  scratch->stamp[source] = epoch;

  gvizFoundVertex curr = {source, 0};
  if (gvizDequePush(queue, &curr) < 0)
    return -1;

  size_t count = 0;
  size_t visited = 1;
  while (!gvizDequeIsEmpty(queue)) {
    gvizDequePopLeft(queue, &curr);

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr.v);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (scratch->stamp[neighbor] == epoch)
        continue;
      scratch->stamp[neighbor] = epoch;
      visited++;

      gvizFoundVertex next = {neighbor, curr.dist + 1};

      if (!filter || gvizVertexSubsetTest(filter, neighbor)) {
        out[count++] = next;
        if (count >= k) {
          knn_profile_record(visited);
          return (int)k;
        }
      }

      if (gvizDequePush(queue, &next) < 0)
        return -1;
    }
  }

  knn_profile_record(visited);

  return (int)count;
}

int gvizSearchKNearest(const gvizSubgraph *sg, gvizFoundVertex *out, size_t k,
                       size_t source, gvizVertexSubset filter) {
  if (!search_input_valid(sg))
    return -1;

  size_t n = sg->g->layout->nvertices;
  gvizKNearestScratch scratch;
  if (gvizKNearestScratchInit(&scratch, n) < 0)
    return -1;

  int res = gvizSearchKNearestScratch(sg, out, k, source, filter, &scratch);
  gvizKNearestScratchRelease(&scratch);
  return res;
}
