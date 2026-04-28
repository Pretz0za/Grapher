#ifndef _GVIZ_LAYER_GRAPH_H_
#define _GVIZ_LAYER_GRAPH_H_

#include "core/gvizCamera.h"
#include "dsa/gvizBitArray.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizGraphVBO.h"
#include "renderer/layers/gvizLayer.h"

typedef struct gvizLayerGraph {
  gvizLayer layer;
  /* Per-layer camera. Component layers always carry their own camera; the
   * scene's input router resolves world coords through the camera of the
   * layer under the cursor. */
  gvizCamera camera;
  gvizEmbeddedGraph *graph;
  /*
   * If non-NULL, called on release to tear down and free the embedding and
   * its underlying graph. Pass NULL to borrow the pointer (no ownership).
   * Typically a static wrapper around the specific algorithm's release fn.
   */
  void (*releaseGraph)(gvizEmbeddedGraph *);
  /*
   * Optional hook fired after dynamic mutations (vertex/edge add/remove).
   * When non-NULL, called with the layer's gvizEmbeddedGraph * so the
   * embedding owner may re-seed/re-fix interior or rebuild matrices.
   */
  void (*onTopologyChanged)(gvizEmbeddedGraph *);
  gvizGraphVBO vbo;
  unsigned int vboMode; /* bitmask of gvizGraphVBOMode (edges/discs) */
  int gpuDirty; /* 2=topology changed, 1=positions changed, 0=clean */
  int highlightDirty; /* 1 if color buffer must be reuploaded */

  /* Per-vertex highlight bits. Length in bits = vertexHighlightBits. */
  GVIZ_BIT_UNIT *vertexHighlight;
  size_t vertexHighlightBits;

  /*
   * Per-directed-edge highlight bits. The bit index for the j-th neighbor of
   * vertex u is `edgeStartIdx[u] + j`. Length-(N+1) prefix-sum array, last
   * entry = total directed-edge count = edgeHighlightBits.
   */
  GVIZ_BIT_UNIT *edgeHighlight;
  size_t edgeHighlightBits;
  size_t *edgeStartIdx;
} gvizLayerGraph;

/*
 * Initialize @p layer. When @p releaseGraph is non-NULL the layer owns
 * @p graph and calls @p releaseGraph(graph) in gvizLayerGraphRelease.
 * Pass NULL to borrow the pointer (caller manages lifetime).
 */
void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        void (*releaseGraph)(gvizEmbeddedGraph *),
                        const gvizViewport viewport, size_t z);

/* Update which render passes (edges, discs) the underlying VBO uses. */
void gvizLayerGraphSetVBOMode(gvizLayerGraph *layer, unsigned int mode);

void gvizLayerGraphDraw(void *layer, const gvizCamera *camera);
void gvizLayerGraphUpdate(void *layer, float dt);
void gvizLayerGraphRelease(void *layer);
int gvizLayerGraphHandleEvent(void *layer, const gvizEvent *event);
int gvizLayerGraphHitTest(void *layer, float wx, float wy);
struct gvizCamera *gvizLayerGraphGetCamera(void *layer);

/* ---- Highlight API ------------------------------------------------------- */

/*
 * Set or clear the highlight bit for a single vertex. Out-of-bounds calls are
 * silently ignored. Marks the color buffer dirty so the next Draw re-uploads.
 */
void gvizLayerGraphSetVertexHighlight(gvizLayerGraph *layer, size_t v, int on);

/* Clears all per-vertex highlight bits. */
void gvizLayerGraphClearVertexHighlights(gvizLayerGraph *layer);

/*
 * Set or clear the highlight bit for the directed edge (u, v). For undirected
 * graphs the symmetric (v, u) bit is also touched. Resolves the bit index via
 * `edgeStartIdx[u] + indexOf(neighbors(u), v)`.
 */
void gvizLayerGraphSetEdgeHighlight(gvizLayerGraph *layer, size_t u, size_t v,
                                    int on);

/* Clears all per-directed-edge highlight bits. */
void gvizLayerGraphClearEdgeHighlights(gvizLayerGraph *layer);

/*
 * Recomputes the @c edgeStartIdx prefix sums from the current adjacency lists
 * and resizes @c edgeHighlight to match the new total directed-edge count
 * (new bits zeroed). Call after any topology mutation.
 */
void gvizLayerGraphRebuildEdgeIndex(gvizLayerGraph *layer);

/* ---- Dynamic mutations --------------------------------------------------- */

/*
 * Append a new vertex at @p startPos (length = embedding.dim) to the underlying
 * graph and grow position + highlight buffers. Marks topology dirty.
 */
int gvizLayerGraphAddVertex(gvizLayerGraph *layer, const double *startPos);

/*
 * Remove vertex @p v from the underlying graph. All incident edges are dropped
 * and remaining vertex indices in adjacency lists are decremented for indices
 * that shifted. Compacts position + highlight buffers. Marks topology dirty.
 */
int gvizLayerGraphRemoveVertex(gvizLayerGraph *layer, size_t v);

/* Add edge (u, v) to the graph and rebuild the edge index. */
int gvizLayerGraphAddEdge(gvizLayerGraph *layer, size_t u, size_t v);

/* Remove edge (u, v) from the graph and rebuild the edge index. */
int gvizLayerGraphRemoveEdge(gvizLayerGraph *layer, size_t u, size_t v);

static const gvizLayerVTable GVIZ_LAYER_GRAPH_VTABLE = {
    .update = gvizLayerGraphUpdate,
    .draw = gvizLayerGraphDraw,
    .onEvent = gvizLayerGraphHandleEvent,
    .release = gvizLayerGraphRelease,
    .hitTest = gvizLayerGraphHitTest,
    .getCamera = gvizLayerGraphGetCamera,
};

#endif
