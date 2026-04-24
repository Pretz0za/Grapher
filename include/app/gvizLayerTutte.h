#ifndef _GVIZ_APP_LAYER_TUTTE_H_
#define _GVIZ_APP_LAYER_TUTTE_H_

#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizTutteEmbedding.h"
#include "renderer/layers/gvizGraphVBO.h"
#include "renderer/layers/gvizLayer.h"

/*
 * A world-space layer that owns a gvizGraph and a live gvizTutteState.
 * Each update(dt) call advances the Tutte relaxation by one step.
 * Keyboard events: SPACE toggles pause, S single-steps, R reseeds interior.
 */
typedef struct gvizLayerTutte {
    gvizLayer layer;     /* MUST be first */
    gvizGraph graph;
    gvizTutteState tutte;
    gvizGraphVBO vbo;
    int gpuDirty; /* 2=topology, 1=positions, 0=clean */
    int paused;
} gvizLayerTutte;

/*
 * Initialize @p layer with the given graph. Pins @p boundaryCount boundary
 * vertices onto a convex polygon of @p boundaryRadius and seeds interior
 * vertices to the centroid. Ownership of @p g is NOT transferred — the
 * layer clones the graph internally.
 *
 * Returns 0 on success, -1 on allocation or embedding failure.
 */
int gvizLayerTutteInit(gvizLayerTutte *layer, gvizGraph *g,
                       const size_t *boundary, size_t boundaryCount,
                       double boundaryRadius, size_t z);

void gvizLayerTutteDraw(void *layer, const gvizCamera *camera);
void gvizLayerTutteUpdate(void *layer, float dt);
void gvizLayerTutteRelease(void *layer);
int gvizLayerTutteHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_LAYER_TUTTE_VTABLE = {
    .draw    = gvizLayerTutteDraw,
    .update  = gvizLayerTutteUpdate,
    .release = gvizLayerTutteRelease,
    .onEvent = gvizLayerTutteHandleEvent,
    .hitTest = NULL,
};

#endif
