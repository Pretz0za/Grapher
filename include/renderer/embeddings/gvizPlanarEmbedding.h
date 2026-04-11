#ifndef _GVIZ_PLANAR_H_
#define _GVIZ_PLANAR_H_

#include "dsa/gvizBitArray.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"

typedef struct gvizPlanarEmbeddingState {
  gvizEmbeddedGraph embedding;
  gvizGraph *kuratowskiSubdivision;
} gvizPlanarEmbeddingState;

int gvizPlanarEmbeddingInit(gvizPlanarEmbeddingState *state, gvizGraph *g);
void gvizPlanarEmbeddingRelease(gvizPlanarEmbeddingState *g);

int gvizPlanarEmbeddingEmbed(gvizPlanarEmbeddingState *state);

typedef struct gvizFaceIteratorContext {
  size_t *borders;
  GVIZ_BIT_ARRAY visited;
  gvizArray faces;
  size_t dCount; // dart count
} gvizFaceIteratorContext;

int gvizFaceIteratorInit(const gvizPlanarEmbeddingState *state,
                         gvizFaceIteratorContext *context);
void gvizFaceIteratorRelease(gvizFaceIteratorContext *context);

int gvizPlanarEmbeddingFaces(const gvizPlanarEmbeddingState *state,
                             gvizFaceIteratorContext *context);

void gvizPlanarEmbeddingTriangulate(const gvizPlanarEmbeddingState *state,
                                    gvizFaceIteratorContext *context);

#endif
