#include "algorithms/search/gvizBreadthFirst.h"
#include "core/alloc.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizDeque.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <stdlib.h>
#include <string.h>

static bool search_input_valid(const gvizSubgraph *sg) {
  return sg && sg->g && sg->g->layout && sg->vs;
}

static bool search_output_valid(const gvizSubgraph *out) {
  return out && out->g && out->g->layout && out->vs && out->es.bitset;
}

int gvizSearchBreadthFirst(const gvizSubgraph *sg, gvizSubgraph *out, size_t source,
                           size_t maxDepth, size_t *distances) {
  if (!search_input_valid(sg) || !search_output_valid(out))
    return -1;
  if (!gvizSubgraphHasVertex(sg, source))
    return -1;

  size_t n = sg->g->layout->nvertices;
  if (distances) {
    for (size_t i = 0; i < n; i++)
      distances[i] = SIZE_MAX;
    distances[source] = 0;
  }

  typedef struct {
    size_t v;
    size_t d;
  } NodeDepth;

  gvizDeque queue;
  if (gvizDequeInit(&queue, sizeof(NodeDepth)) < 0)
    return -1;

  GVIZ_BIT_ARRAY seen = gvizBitArrayAlloc(n);
  if (!seen) {
    gvizDequeRelease(&queue);
    return -1;
  }
  gvizSetBit(seen, source);

  gvizSubgraphShowVertex(out, source);

  NodeDepth start = {source, 0};
  if (gvizDequePush(&queue, &start) < 0) {
    gvizBitArrayFree(seen);
    gvizDequeRelease(&queue);
    return -1;
  }

  while (!gvizDequeIsEmpty(&queue)) {
    NodeDepth nd;
    gvizDequePopLeft(&queue, &nd);

    size_t curr = nd.v;
    size_t currDepth = nd.d;

    if (maxDepth && currDepth >= maxDepth)
      continue;

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (gvizTestBit(seen, neighbor))
        continue;
      gvizSetBit(seen, neighbor);

      size_t nextDepth = currDepth + 1;
      if (distances)
        distances[neighbor] = nextDepth;

      gvizSubgraphShowVertex(out, neighbor);
      gvizSubgraphShowEdge(out, curr, neighbor);

      NodeDepth next = {neighbor, nextDepth};
      if (gvizDequePush(&queue, &next) < 0) {
        gvizBitArrayFree(seen);
        gvizDequeRelease(&queue);
        return -1;
      }
    }
  }

  gvizBitArrayFree(seen);
  gvizDequeRelease(&queue);
  return 0;
}
