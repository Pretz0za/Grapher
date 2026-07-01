#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

#include "dsa/gvizDeque.h"
#include "embedders/gvizEmbeddedGraph.h"

typedef struct gvizGRIPDecorators {
  gvizArray knn;
  double *disp;
  double *oldDisp;
  double oldCos;
  double heat;
} gvizGRIPDecorators;

typedef struct gvizGRIPState {
  gvizEmbeddedGraph graph;
  size_t layerCount;
  size_t currLayer;
  size_t currRound;
  GVIZ_BIT_ARRAY placed;
  size_t *misFiltration;
  size_t *misBorder;
  size_t *rounds;
  gvizGRIPDecorators *dec;
  GVIZ_BIT_ARRAY dispCalculated;
  size_t *radiusBfsStamp;
  size_t radiusBfsEpoch;
  gvizDeque radiusBfsQueue;
} gvizGRIPState;

int gvizGRIPEmbedderInit(gvizGRIPState *state, gvizGraph *graph,
                          size_t diameter, size_t dimension);
int gvizGRIPEmbedderEmbed(gvizGRIPState *state);
void gvizGRIPEmbedderRelease(gvizGRIPState *state);

size_t createMISFiltration(gvizGRIPState *state);


void gvizGRIPEmbedderBegin(gvizGRIPState *state);
void beginNewStage(gvizGRIPState *state);
void runRefinementRound(gvizGRIPState *state);

#endif
