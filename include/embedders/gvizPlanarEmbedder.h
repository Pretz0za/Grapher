#ifndef _GVIZ_PLANAR_H_
#define _GVIZ_PLANAR_H_

#include "dsa/gvizBitArray.h"
#include "embedders/gvizEmbeddedGraph.h"

typedef struct gvizPlanarEmbedderState {
  gvizEmbeddedGraph embedding;
  gvizGraph *kuratowskiSubdivision;
} gvizPlanarEmbedderState;

// initializes a planar graph and reorders the adjacency lists to show the
// counter-clockwise rotation system (embedding).
int gvizPlanarEmbedderInit(gvizPlanarEmbedderState *state, gvizGraph *g);
void gvizPlanarEmbedderRelease(gvizPlanarEmbedderState *g);

int gvizPlanarEmbedderEmbed(gvizPlanarEmbedderState *state);

typedef struct gvizFaceIteratorContext {
  size_t *borders;
  GVIZ_BIT_ARRAY visited;
  gvizArray faces;
  size_t dCount; // dart count
} gvizFaceIteratorContext;

int gvizFaceIteratorInit(const gvizPlanarEmbedderState *state,
                         gvizFaceIteratorContext *context);
void gvizFaceIteratorRelease(gvizFaceIteratorContext *context);

int gvizPlanarEmbedderFaces(const gvizPlanarEmbedderState *state,
                             gvizFaceIteratorContext *context);

void gvizPlanarEmbedderTriangulate(const gvizPlanarEmbedderState *state,
                                    gvizFaceIteratorContext *context);

#endif
