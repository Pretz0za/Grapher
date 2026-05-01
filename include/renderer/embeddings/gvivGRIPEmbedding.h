#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraphView.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"

typedef struct gvizGRIPDecorators {
  double *disp;
  double *oldDisp;
  double oldCos;
  double heat;
  int initialized;
} gvizGRIPDecorators;

typedef struct gvizGRIPState {
  gvizEmbeddedGraph graph;
  size_t *misFiltration;
  size_t *misBorder;
  size_t *rounds;
  gvizGRIPDecorators *dec;
} gvizGRIPState;

/**
 * Canonical view-aware initializer. The view is moved into the embedded
 * graph (caller must not release it after a successful return). Mode is
 * `GVIZ_EMBED_FULL_GRAPH` so positions are indexed by raw vertex id; the
 * view selects which vertices participate in force calculation.
 */
int gvizGRIPEmbeddingInitView(gvizGRIPState *state, gvizGraphView view,
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
