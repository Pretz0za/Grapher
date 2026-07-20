#include "algorithms/search/gvizConnectedComponents.h"
#include "core/alloc.h"
#include "ds/gvizDeque.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <stdlib.h>
#include <string.h>

int gvizConnectedComponents(const gvizSubgraph *sg, size_t *labels,
                            size_t *out_count) {
  if (!sg || !labels || !sg->g || !sg->g->layout || !sg->vs)
    return -1;

  size_t n = sg->g->layout->nvertices;
  for (size_t i = 0; i < n; i++)
    labels[i] = SIZE_MAX;

  gvizDeque queue;
  if (gvizDequeInit(&queue, sizeof(size_t)) < 0)
    return -1;

  size_t comp = 0;
  for (size_t i = 0; i < n; i++) {
    if (!gvizSubgraphHasVertex(sg, i))
      continue;
    if (labels[i] != SIZE_MAX)
      continue;

    labels[i] = comp;
    if (gvizDequePush(&queue, &i) < 0) {
      gvizDequeRelease(&queue);
      return -1;
    }

    while (!gvizDequeIsEmpty(&queue)) {
      size_t curr;
      gvizDequePopLeft(&queue, &curr);

      gvizSubgraphNeighborIterator nit =
          gvizSubgraphNeighborIteratorCreate(sg, curr);
      size_t neighbor;
      while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
        if (labels[neighbor] != SIZE_MAX)
          continue;
        labels[neighbor] = comp;
        if (gvizDequePush(&queue, &neighbor) < 0) {
          gvizDequeRelease(&queue);
          return -1;
        }
      }
    }
    comp++;
  }

  gvizDequeRelease(&queue);

  if (out_count)
    *out_count = comp;
  return 0;
}

void gvizConnectedComponentSizes(const size_t *labels, size_t nvertices,
                                 size_t component_count, size_t *sizes) {
  if (!labels || !sizes || component_count == 0)
    return;

  memset(sizes, 0, component_count * sizeof(size_t));
  for (size_t v = 0; v < nvertices; v++) {
    if (labels[v] == SIZE_MAX)
      continue;
    sizes[labels[v]]++;
  }
}
