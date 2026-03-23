#include "raylib.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizLayer.h"

typedef struct gvizLayerGraph {
  gvizLayer layer;
  gvizEmbeddedGraph *graph;
} gvizLayerGraph;

/*
 * Initializes some memory as a gvizLayerGraph. @p graph is cloned (moved) into
 * the layer, meaning the graph data in the address will become invalid.
 *
 * @param layer A pointer to a the memory address to be initialized.
 * @param graph A pointer to the graph data that will be moved to the layer.
 * */
void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        const gvizViewport viewport, size_t z);

void gvizLayerGraphDraw(void *layer, const Camera2D *camera);
void gvizLayerGraphUpdate(void *layer, float dt);
void gvizLayerGraphRelease(void *layer);
int gvizLayerGraphHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_LAYER_GRAPH_VTABLE = {
    .update = gvizLayerGraphUpdate,
    .draw = gvizLayerGraphDraw,
    .onEvent = gvizLayerGraphHandleEvent,
    .release = gvizLayerGraphRelease,
};
