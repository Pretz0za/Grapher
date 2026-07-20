#include "algorithms/search/gvizDepthFirst.h"
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

int gvizSearchDepthFirst(const gvizSubgraph *sg, gvizSubgraph *out, size_t source) {
  if (!search_input_valid(sg) || !search_output_valid(out))
    return -1;
  if (!gvizSubgraphHasVertex(sg, source))
    return -1;

  size_t n = sg->g->layout->nvertices;
  GVIZ_BIT_ARRAY seen = gvizBitArrayAlloc(n);
  if (!seen)
    return -1;
  gvizSetBit(seen, source);

  gvizSubgraphShowVertex(out, source);

  gvizDeque stack;
  if (gvizDequeInit(&stack, sizeof(size_t)) < 0) {
    gvizBitArrayFree(seen);
    return -1;
  }
  if (gvizDequePush(&stack, &source) < 0) {
    gvizBitArrayFree(seen);
    gvizDequeRelease(&stack);
    return -1;
  }

  while (!gvizDequeIsEmpty(&stack)) {
    size_t curr;
    gvizDequePopRight(&stack, &curr);

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (gvizTestBit(seen, neighbor))
        continue;
      gvizSetBit(seen, neighbor);

      gvizSubgraphShowVertex(out, neighbor);
      gvizSubgraphShowEdge(out, curr, neighbor);

      if (gvizDequePush(&stack, &neighbor) < 0) {
        gvizBitArrayFree(seen);
        gvizDequeRelease(&stack);
        return -1;
      }
    }
  }

  gvizBitArrayFree(seen);
  gvizDequeRelease(&stack);
  return 0;
}
