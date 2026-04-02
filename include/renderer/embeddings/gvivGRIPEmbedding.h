#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"

typedef struct gvizGRIPDecorators {
  float oldDisp;
  float oldCos;
} gvizGRIPDecorators;

typedef struct gvizGRIPState {
  gvizEmbeddedGraph graph;
  size_t *misFiltration;
  size_t *misBorder;
  gvizGRIPDecorators *dec;
} gvizGRIPState;

int gvizGRIPEmbeddingInit(gvizGRIPState *state, gvizGraph *graph,
                          size_t diameter);
int gvizGRIPEmbeddingEmbed(gvizGRIPState *state);
void gvizGRIPEmbeddingRelease(gvizGRIPState *state);

size_t createMISFiltration(gvizGRIPState *state);

#endif
