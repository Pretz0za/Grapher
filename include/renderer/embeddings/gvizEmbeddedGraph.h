#ifndef _GVIZ_EMBEDDED_GRAPH_H_
#define _GVIZ_EMBEDDED_GRAPH_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizGraphView.h"

typedef struct gvizDFSState {

  size_t *bitArrayOffsets;
  GVIZ_BIT_ARRAY visitedHalfEdges;
} gvizFaceSearchState;

typedef struct gvizEmbedding {
  size_t dim;
  double *vertexPositions;
} gvizEmbedding;

/**
 * Mode of an embedded graph:
 *   - GVIZ_EMBED_FULL_GRAPH: `vertexPositions` is sized to the full
 *     underlying graph (N vertices). Vertex `u` is indexed directly.
 *     Algorithms may still iterate the view-only subset; vertices outside
 *     the view simply keep their stale positions.
 *   - GVIZ_EMBED_VIEW_ONLY: `vertexPositions` is sized to `view.count`.
 *     A `local` lookup table maps raw graph vertex id -> local index.
 */
typedef enum {
  GVIZ_EMBED_FULL_GRAPH = 0,
  GVIZ_EMBED_VIEW_ONLY = 1
} gvizEmbeddingMode;

typedef struct gvizEmbeddedGraph {
  gvizGraphView view;     /* full ownership of mask/edgeStart memory */
  gvizEmbeddingMode mode;
  size_t *local;          /* raw vertex id -> local index (Mode B); NULL otherwise */
  gvizEmbedding embedding;
} gvizEmbeddedGraph;

/**
 * View-aware init. The view's contents are MOVED into the embedded graph;
 * the caller's `view` becomes a zeroed/owned-elsewhere shell after the
 * call (caller should not release it). To get a fresh independent view,
 * initialize a new one before calling.
 *
 * In Mode B (`GVIZ_EMBED_VIEW_ONLY`), `vertexPositions` is sized to
 * `view.count` and a `local` table is built mapping raw graph vertex id
 * to local position index for vertices in the view (others map to a
 * sentinel). In Mode A, `vertexPositions` is sized to the full graph and
 * `local` is NULL.
 */
int gvizEmbeddedGraphInitView(gvizEmbeddedGraph *embedding, gvizGraphView view,
                              gvizEmbeddingMode mode, size_t dimension);

void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding);
int gvizEmbeddedGraphNextFace(gvizEmbeddedGraph *embedding,
                              gvizFaceSearchState *gvizDFSSate);

/**
 * Map a raw graph vertex id to its position-array index, mode-aware.
 * Returns SIZE_MAX (~(size_t)0) if `u` is not represented in this
 * embedding (Mode B view-only and `u` is outside the view).
 */
size_t gvizEmbeddedGraphLocalIndex(const gvizEmbeddedGraph *embedding,
                                   size_t u);

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
