#include "core/alloc.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int gvizGRIPEmbeddingInit(gvizGRIPState *state, gvizGraph *graph,
                          size_t diameter) {
  int res;
  size_t N = graph->vertices.count;

  res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, graph);
  if (res < 0)
    return res;

  size_t tmp = 1024;
  if (diameter) {
    tmp = (size_t)log2(diameter) + 1;
  }
  state->misBorder = GVIZ_ALLOC(sizeof(size_t) * tmp);
  if (!state->misBorder)
    return -1;
  state->misBorder[0] = N;

  state->misFiltration = GVIZ_ALLOC(sizeof(size_t) * N);
  if (!state->misFiltration)
    return -1;

  state->dec = GVIZ_ALLOC(sizeof(gvizGRIPDecorators) * N);
  if (!state->dec)
    return -1;

  return 0;
}

void gvizGRIPEmbeddingRelease(gvizGRIPState *state) {
  gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)state);
  if (state->misBorder)
    GVIZ_DEALLOC(state->misBorder);
  if (state->misFiltration)
    GVIZ_DEALLOC(state->misFiltration);
  if (state->dec)
    GVIZ_DEALLOC(state->dec);
  return;
}

size_t *elementsOfSubset(gvizGRIPState *state, size_t i) {
  if (i == 0)
    return state->graph.graph->vertices.arr;
  return state->misFiltration;
}

int iterMISFiltration(gvizGRIPState *state, size_t i, GVIZ_BIT_ARRAY curr) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t *prev = elementsOfSubset(state, i - 1);
  size_t prevCount = state->misBorder[i];
  gvizGraph bfs;

  for (size_t j = 0; j < embedding->graph->vertices.count; j++) {
    if (!gvizTestBit(curr, j))
      continue;
    gvizGraphBFSTree(((gvizEmbeddedGraph *)state)->graph, &bfs, j, pow(2, i));

    gvizGraphRelease(&bfs);
  }
}

size_t createMISFiltration(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  GVIZ_BIT_UNIT curr[GVIZ_ARRAY_UNITS(embedding->graph->vertices.count)];
  memset(curr, 1, sizeof(curr));

  size_t i = 1;
  while (iterMISFiltration(state, i, curr)) {
    i++;
  }

  return 0;
}

int gvizGRIPEmbeddingEmbed(gvizGRIPState *state);
