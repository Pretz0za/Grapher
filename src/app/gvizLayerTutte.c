#include "app/gvizLayerTutte.h"
#include "core/event.h"
#include "core/gvizCamera.h"
#include "raylib.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizGraphVBO.h"

int gvizLayerTutteInit(gvizLayerTutte *layer, gvizGraph *g,
                       const size_t *boundary, size_t boundaryCount,
                       double boundaryRadius, size_t z) {
  gvizLayerInit((gvizLayer *)layer, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_TUTTE_VTABLE, z);
  layer->layer.flags = GVIZ_LAYER_VISIBLE;
  layer->paused = 0;

  if (gvizGraphClone(&layer->graph, g) != 0)
    return -1;

  if (gvizTutteEmbeddingInit(&layer->tutte, &layer->graph, 2, 1e-5) != 0) {
    gvizGraphRelease(&layer->graph);
    return -1;
  }
  layer->tutte.relaxationRate = 10.0;

  gvizTutteFixConvexPolygon(&layer->tutte, boundary, boundaryCount,
                            boundaryRadius);
  gvizTutteEmbeddingSeedInterior(&layer->tutte);

  gvizGraphVBOInit(&layer->vbo);
  layer->gpuDirty = 2;
  return 0;
}

void gvizLayerTutteDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->tutte;

  if (self->gpuDirty >= 2 || !self->vbo.vaoId)
    gvizGraphVBORebuild(&self->vbo, eg);
  else if (self->gpuDirty == 1)
    gvizGraphVBOUploadPositions(&self->vbo, eg);
  self->gpuDirty = 0;

  gvizGraphVBODraw(&self->vbo);
}

void gvizLayerTutteUpdate(void *layerV, float dt) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;
  if (!self->paused && !self->tutte.converged) {
    gvizTutteEmbeddingStep(&self->tutte, dt);
    self->gpuDirty = 1;
  }
}

void gvizLayerTutteRelease(void *layerV) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;
  gvizGraphVBORelease(&self->vbo);
  gvizTutteEmbeddingRelease(&self->tutte);
  gvizGraphRelease(&self->graph);
}

int gvizLayerTutteHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;
  if (event->type != GVIZ_EVENT_KEY_DOWN)
    return 0;
  switch (event->key.key) {
  case KEY_SPACE:
    self->paused = !self->paused;
    return 1;
  case KEY_S:
    gvizTutteEmbeddingStep(&self->tutte, 1.0f / 60.0f);
    self->gpuDirty = 1;
    return 1;
  case KEY_R:
    gvizTutteEmbeddingSeedInterior(&self->tutte);
    self->gpuDirty = 1;
    return 1;
  }
  return 0;
}
