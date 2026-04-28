#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
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

int gvizGRIPRefineEmbedding(gvizGRIPState *state);

/*
 * Per-round driving API for animated/interleaved GRIP refinement.
 *
 * Lifecycle for one layer:
 *   knns = gvizGRIPPrepareLayerKNNs(state, layer, placed);
 *   for (round = 0; round < N; round++)
 *       gvizGRIPRefineOneRound(state, layer, knns);
 *   gvizGRIPReleaseLayerKNNs(state, layer, knns);
 */
gvizArray *gvizGRIPPrepareLayerKNNs(gvizGRIPState *state, size_t layer,
                                    GVIZ_BIT_UNIT *placed);
void gvizGRIPRefineOneRound(gvizGRIPState *state, size_t layer,
                            gvizArray *knns);
void gvizGRIPReleaseLayerKNNs(gvizGRIPState *state, size_t layer,
                              gvizArray *knns);

/*
 * Place the @p layer-th MIS layer's vertices at the barycenter of their
 * already-placed nearest neighbors, and mark them in @p placedVertices.
 */
void placeLayerVertices(gvizGRIPState *state, size_t layer,
                        GVIZ_BIT_ARRAY placedVertices);

/* Generate (n+1) vertices of an n-simplex, scaled to @p side_length. */
void makeRegularSimplex(size_t n, double side_length, double *out);

#endif
