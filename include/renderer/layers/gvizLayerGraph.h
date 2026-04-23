#ifndef _GVIZ_LAYER_GRAPH_H_
#define _GVIZ_LAYER_GRAPH_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizLayer.h"

typedef struct gvizLayerGraph {
  gvizLayer layer;
  gvizEmbeddedGraph *graph;
  /*
   * When set, the layer owns `graph` and releases + frees it (and the
   * underlying gvizGraph) in gvizLayerGraphRelease. Set by gvizLayerGraphInit.
   */
  int ownsGraph;
} gvizLayerGraph;

/*
 * Initialize memory as a gvizLayerGraph. The layer takes ownership of
 * @p graph and the gvizGraph it wraps; both are released when the layer
 * is released.
 *
 * @param layer  Memory to initialize.
 * @param graph  Heap-allocated gvizEmbeddedGraph; ownership transfers.
 * */
void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        const gvizViewport viewport, size_t z);

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
