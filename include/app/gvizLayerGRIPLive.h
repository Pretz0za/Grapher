#ifndef _GVIZ_APP_LAYER_GRIP_LIVE_H_
#define _GVIZ_APP_LAYER_GRIP_LIVE_H_

#include "core/gvizCamera.h"
#include "core/event.h"
#include "core/gvizScene.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/layers/gvizGraphVBO.h"
#include "renderer/layers/gvizLayer.h"
#include <stddef.h>

/*
 * Like gvizLayerGRIP but runs an unbounded number of force rounds per layer.
 * A layer is only advanced when gvizLayerGRIPLiveAdvance is called explicitly
 * (or the user presses SPACE). Useful for inspecting convergence per layer.
 *
 * Press SPACE to advance to the next MIS layer.
 */
typedef struct gvizLayerGRIPLive {
  gvizLayer layer; /* MUST be first */
  gvizCamera camera;
  /* Optional registry binding. See gvizLayerGRIPLiveBindHandle. */
  gvizScene *scene;
  gvizSceneGraphHandle graphHandle;
  gvizGraph graph;
  gvizGRIPState grip;
  gvizGraphVBO vbo;
  GVIZ_BIT_UNIT *placedBits;
  gvizArray *layerKNNs;
  size_t layerCount;
  int currentLayer; /* layerCount-1 down to 0; -1 = all done */
  int currentRound;
  int gpuDirty;
} gvizLayerGRIPLive;

int  gvizLayerGRIPLiveInit(gvizLayerGRIPLive *layer, gvizGraph *graph,
                           size_t diameter, size_t z);

/*
 * Bind this layer to a scene-registered graph handle. Retains the handle
 * and subscribes to mutation events. The layer keeps its own graph clone.
 */
void gvizLayerGRIPLiveBindHandle(gvizLayerGRIPLive *layer, gvizScene *scene,
                                 gvizSceneGraphHandle h, gvizGraphCallback cb);
void gvizLayerGRIPLiveAdvance(gvizLayerGRIPLive *layer);
void gvizLayerGRIPLiveDraw(void *layer, const gvizCamera *camera);
void gvizLayerGRIPLiveUpdate(void *layer, float dt);
void gvizLayerGRIPLiveRelease(void *layer);
int  gvizLayerGRIPLiveHandleEvent(void *layer, const gvizEvent *event);
struct gvizCamera *gvizLayerGRIPLiveGetCamera(void *layer);

static const gvizLayerVTable GVIZ_LAYER_GRIP_LIVE_VTABLE = {
    .draw    = gvizLayerGRIPLiveDraw,
    .update  = gvizLayerGRIPLiveUpdate,
    .release = gvizLayerGRIPLiveRelease,
    .onEvent = gvizLayerGRIPLiveHandleEvent,
    .hitTest = NULL,
    .getCamera = gvizLayerGRIPLiveGetCamera,
};

#endif
