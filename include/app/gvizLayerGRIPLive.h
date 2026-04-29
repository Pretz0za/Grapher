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
  size_t drawDim;            /* 2 or 3; PCA target if embedding dim is higher */
} gvizLayerGRIPLive;

int  gvizLayerGRIPLiveInit(gvizLayerGRIPLive *layer, gvizGraph *graph,
                           size_t diameter, size_t z);

/*
 * Like gvizLayerGRIPLiveInit but builds the embedding in 3D and gives the
 * layer a 3D camera. The GRIP refinement is dimensionality-generic.
 */
int  gvizLayerGRIPLiveInit3D(gvizLayerGRIPLive *layer, gvizGraph *graph,
                             size_t diameter, size_t z);

/*
 * General N-dimensional GRIP init. @p embDim is the GRIP embedding
 * dimension (must be >= 2). @p drawDim is 2 or 3 — controls camera kind
 * and the target dimension for PCA projection at GPU upload time.
 */
int  gvizLayerGRIPLiveInitND(gvizLayerGRIPLive *layer, gvizGraph *graph,
                             size_t diameter, size_t z,
                             size_t embDim, size_t drawDim);

/*
 * Build an empty GRIP-Live layer (no graph) with either a 2D or 3D camera.
 * Useful as a 3D blank canvas; extension to a real graph is left to callers.
 */
int  gvizLayerGRIPLiveInitEmpty3D(gvizLayerGRIPLive *layer, size_t z);

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
