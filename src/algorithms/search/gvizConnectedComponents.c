#include "algorithms/search/gvizConnectedComponents.h"
#include "algorithms/search/gvizBreadthFirst.h"
#include "core/alloc.h"
#include "ds/gvizBitArray.h"
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

  size_t *distances = GVIZ_ALLOC(n * sizeof(size_t));
  if (!distances)
    return -1;

  gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg->g);
  size_t comp = 0;

  for (size_t i = 0; i < n; i++) {
    if (!gvizSubgraphHasVertex(sg, i))
      continue;
    if (labels[i] != SIZE_MAX)
      continue;

    gvizSubgraphRelease(&bfs);
    bfs = gvizSubgraphCreateEmpty(sg->g);
    if (gvizSearchBreadthFirst(sg, &bfs, i, 0, distances) < 0) {
      gvizSubgraphRelease(&bfs);
      GVIZ_DEALLOC(distances);
      return -1;
    }

    gvizBitArrayIterator it = gvizVertexSubsetIteratorCreate(bfs.vs, n);
    size_t v;
    while (gvizBitArrayIterate(&it, &v))
      labels[v] = comp;
    comp++;
  }

  gvizSubgraphRelease(&bfs);
  GVIZ_DEALLOC(distances);

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
