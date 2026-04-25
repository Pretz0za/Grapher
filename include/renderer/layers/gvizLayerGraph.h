#ifndef _GVIZ_LAYER_GRAPH_H_
#define _GVIZ_LAYER_GRAPH_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizGraphVBO.h"
#include "renderer/layers/gvizLayer.h"

typedef struct gvizLayerGraph {
  gvizLayer layer;
  gvizEmbeddedGraph *graph;
  /*
   * If non-NULL, called on release to tear down and free the embedding and
   * its underlying graph. Pass NULL to borrow the pointer (no ownership).
   * Typically a static wrapper around the specific algorithm's release fn.
   */
  void (*releaseGraph)(gvizEmbeddedGraph *);
  gvizGraphVBO vbo;
  unsigned int vboMode; /* bitmask of gvizGraphVBOMode (edges/discs) */
  int gpuDirty; /* 2=topology changed, 1=positions changed, 0=clean */
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

static const gvizLayerVTable GVIZ_LAYER_GRAPH_VTABLE = {
    .update = gvizLayerGraphUpdate,
    .draw = gvizLayerGraphDraw,
    .onEvent = gvizLayerGraphHandleEvent,
    .release = gvizLayerGraphRelease,
    .hitTest = gvizLayerGraphHitTest,
};

#endif
