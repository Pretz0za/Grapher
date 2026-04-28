#ifndef _GVIZ_APP_LAYER_GRIP_H_
#define _GVIZ_APP_LAYER_GRIP_H_

#include "core/gvizCamera.h"
#include "core/gvizScene.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/layers/gvizGraphVBO.h"
#include "renderer/layers/gvizLayer.h"
#include <stddef.h>

/*
 * A world-space layer that runs a GRIP embedding incrementally, one force
 * round per update(dt). Vertices are invisible (radius=0) until their MIS
 * layer is placed, then animate into position. Edges only render after the
 * finest layer (layer 0) finishes refining.
 */
typedef struct gvizLayerGRIP {
  gvizLayer layer; /* MUST be first */
  gvizCamera camera;
  /* Optional registry binding. See gvizLayerGRIPBindHandle. */
  gvizScene *scene;
  gvizSceneGraphHandle graphHandle;
  gvizGraph graph;
  gvizGRIPState grip;
  gvizGraphVBO vbo;
  GVIZ_BIT_UNIT *placedBits; /* heap, GVIZ_ARRAY_UNITS(N) units */
  gvizArray *layerKNNs;      /* current layer's KNN arrays; NULL when idle */
  size_t layerCount;
  int currentLayer; /* layerCount-1 down to 0; -1 = all done */
  int currentRound;
  int gpuDirty; /* 2=topology, 1=positions, 0=clean */
} gvizLayerGRIP;

int gvizLayerGRIPInit(gvizLayerGRIP *layer, gvizGraph *graph, size_t diameter,
                      size_t z);

/*
 * Bind this layer to a scene-registered graph handle. Retains the handle
 * and subscribes the layer to mutation events. The layer keeps its own
 * graph clone — the binding only delivers shared-graph notifications.
 */
void gvizLayerGRIPBindHandle(gvizLayerGRIP *layer, gvizScene *scene,
                             gvizSceneGraphHandle h, gvizGraphCallback cb);
void gvizLayerGRIPDraw(void *layer, const gvizCamera *camera);
void gvizLayerGRIPUpdate(void *layer, float dt);
void gvizLayerGRIPRelease(void *layer);
struct gvizCamera *gvizLayerGRIPGetCamera(void *layer);

static const gvizLayerVTable GVIZ_LAYER_GRIP_VTABLE = {
    .draw = gvizLayerGRIPDraw,
    .update = gvizLayerGRIPUpdate,
    .release = gvizLayerGRIPRelease,
    .onEvent = NULL,
    .hitTest = NULL,
    .getCamera = gvizLayerGRIPGetCamera,
};

#endif
