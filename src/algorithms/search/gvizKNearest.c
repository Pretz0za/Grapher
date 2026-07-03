#include "algorithms/search/gvizKNearest.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizDeque.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <string.h>

static bool search_input_valid(const gvizSubgraph *sg) {
  return sg && sg->g && sg->g->layout && sg->vs;
}

int gvizSearchKNearest(const gvizSubgraph *sg, gvizFoundVertex *out, size_t k,
                       size_t source, gvizVertexSubset filter) {
  if (k == 0)
    return 0;
  if (!out || !search_input_valid(sg))
    return -1;
  if (!gvizSubgraphHasVertex(sg, source))
    return -1;

  size_t n = sg->g->layout->nvertices;

  gvizDeque queue;
  if (gvizDequeInit(&queue, sizeof(gvizFoundVertex)) < 0)
    return -1;

  GVIZ_BIT_UNIT seen[GVIZ_ARRAY_UNITS(n)];
  memset(seen, 0, sizeof(seen));
  gvizSetBit(seen, source);

  gvizFoundVertex curr = {source, 0};
  if (gvizDequePush(&queue, &curr) < 0) {
    gvizDequeRelease(&queue);
    return -1;
  }

  size_t count = 0;
  while (!gvizDequeIsEmpty(&queue)) {
    gvizDequePopLeft(&queue, &curr);

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr.v);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (gvizTestBit(seen, neighbor))
        continue;
      gvizSetBit(seen, neighbor);

      gvizFoundVertex next = {neighbor, curr.dist + 1};

      if (!filter || gvizVertexSubsetTest(filter, neighbor)) {
        out[count++] = next;
        if (count >= k) {
          gvizDequeRelease(&queue);
          return (int)k;
        }
      }

      if (gvizDequePush(&queue, &next) < 0) {
        gvizDequeRelease(&queue);
        return -1;
      }
    }
  }

  gvizDequeRelease(&queue);
  return (int)count;
}
