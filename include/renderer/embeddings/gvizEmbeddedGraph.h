#ifndef _GVIZ_EMBEDDED_GRAPH_H_
#define _GVIZ_EMBEDDED_GRAPH_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "raylib.h"

typedef struct gvizDFSState {

  size_t *bitArrayOffsets;
  GVIZ_BIT_ARRAY visitedHalfEdges;
} gvizFaceSearchState;

typedef struct gvizEmbedding {
  Vector2 *vertexPositions;
} gvizEmbedding;

typedef struct gvizEmbeddedGraph {
  gvizGraph *graph;
  gvizEmbedding embedding;

} gvizEmbeddedGraph;

int gvizEmbeddedGraphInit(gvizEmbeddedGraph *embedding, gvizGraph *graph);
void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding);
int gvizEmbeddedGraphNextFace(gvizEmbeddedGraph *embedding,
                              gvizFaceSearchState *gvizDFSSate);

#endif
