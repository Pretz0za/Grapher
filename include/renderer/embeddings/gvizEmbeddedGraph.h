#ifndef _GVIZ_EMBEDDED_GRAPH_H_
#define _GVIZ_EMBEDDED_GRAPH_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"

typedef struct gvizDFSState {

  size_t *bitArrayOffsets;
  GVIZ_BIT_ARRAY visitedHalfEdges;
} gvizFaceSearchState;

typedef struct gvizEmbedding {
  size_t dim;
  double *vertexPositions;
} gvizEmbedding;

typedef struct gvizEmbeddedGraph {
  gvizGraph *graph;
  gvizEmbedding embedding;

} gvizEmbeddedGraph;

int gvizEmbeddedGraphInit(gvizEmbeddedGraph *embedding, gvizGraph *graph,
                          size_t dimension);
void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding);
int gvizEmbeddedGraphNextFace(gvizEmbeddedGraph *embedding,
                              gvizFaceSearchState *gvizDFSSate);

double *gvizEmbeddedGraphGetVPosition(gvizEmbeddedGraph *embedding, size_t idx);
void gvizEmbeddedGraphSetVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position);

void gvizEmbeddedGraphAddVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position);

int gvizEmbeddedGraphSaveEmbedding(gvizEmbeddedGraph *embedding,
                                   const char *name, const char *filename);

int gvizEmbeddedGraphLoadEmbedding(gvizEmbeddedGraph *embedding,
                                   const char *filename);

#endif
