#include "algorithms/search/gvizKNearest.h"
#include "core/alloc.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <stdlib.h>
#include <string.h>

static bool search_input_valid(const gvizSubgraph *sg) {
  return sg && sg->g && sg->g->layout && sg->vs;
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
  while (!gvizDequeIsEmpty(queue)) {
    gvizDequePopLeft(queue, &curr);

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr.v);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (scratch->stamp[neighbor] == epoch)
        continue;
      scratch->stamp[neighbor] = epoch;

      gvizFoundVertex next = {neighbor, curr.dist + 1};

      if (!filter || gvizVertexSubsetTest(filter, neighbor)) {
        out[count++] = next;
        if (count >= k)
          return (int)k;
      }

      if (gvizDequePush(queue, &next) < 0)
        return -1;
    }
  }

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
