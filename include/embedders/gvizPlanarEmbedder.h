#ifndef _GVIZ_PLANAR_H_
#define _GVIZ_PLANAR_H_

#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"

typedef struct gvizPlanarEmbedderState {
  gvizEmbeddedGraph embedding;
  gvizGraph *kuratowskiSubdivision;
} gvizPlanarEmbedderState;

/**
 * Tests planarity and, if planar, reorders adjacency lists to a CCW rotation system.
 * On failure, @p state->kuratowskiSubdivision may hold a Kuratowski subdivision.
 *
 * @return 0 if planar, non-zero otherwise.
 */
int gvizPlanarEmbedderInit(gvizPlanarEmbedderState *state, gvizSubgraph subgraph);

/** Releases @p state, including any Kuratowski subdivision graph. */
void gvizPlanarEmbedderRelease(gvizPlanarEmbedderState *g);

/** Computes a straight-line embedding for the planar graph in @p state. */
int gvizPlanarEmbedderEmbed(gvizPlanarEmbedderState *state);

typedef struct gvizFaceIteratorContext {
  size_t *borders;
  GVIZ_BIT_ARRAY visited;
  gvizArray faces;
  size_t dCount; // dart count
} gvizFaceIteratorContext;

/**
 * Initializes @p context for face enumeration on the embedded planar graph in @p state.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizFaceIteratorInit(const gvizPlanarEmbedderState *state,
                         gvizFaceIteratorContext *context);

/** Releases memory owned by @p context. */
void gvizFaceIteratorRelease(gvizFaceIteratorContext *context);

/**
 * Enumerates all faces into @p context->faces.
 * Each face is stored as a gvizArray of vertex indices.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizPlanarEmbedderFaces(const gvizPlanarEmbedderState *state,
                             gvizFaceIteratorContext *context);

/**
 * Triangulates the planar graph in @p state by adding edges across non-triangular faces.
 * Updates @p context if faces were previously enumerated.
 */
void gvizPlanarEmbedderTriangulate(const gvizPlanarEmbedderState *state,
                                    gvizFaceIteratorContext *context);

#endif
