#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"

typedef struct gvizGRIPDecorators {
  double *disp;
  double *oldDisp;
  double oldCos;
  double heat;
} gvizGRIPDecorators;

typedef struct gvizGRIPState {
  gvizEmbeddedGraph graph;
  size_t *misFiltration;
  size_t *misBorder;
  size_t *rounds;
  gvizGRIPDecorators *dec;
  GVIZ_BIT_ARRAY dispCalculated;
} gvizGRIPState;

int gvizGRIPEmbeddingInit(gvizGRIPState *state, gvizGraph *graph,
                          size_t diameter, size_t dimension);
int gvizGRIPEmbeddingEmbed(gvizGRIPState *state);
void gvizGRIPEmbeddingRelease(gvizGRIPState *state);

size_t createMISFiltration(gvizGRIPState *state);

#endif
