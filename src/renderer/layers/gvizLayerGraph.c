#include "renderer/layers/gvizLayerGraph.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizGraphVBO.h"
#include <stdlib.h>

void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        void (*releaseGraph)(gvizEmbeddedGraph *),
                        const gvizViewport viewport, size_t z) {
  gvizLayerInit((gvizLayer *)layer, viewport, &GVIZ_LAYER_GRAPH_VTABLE, z);
  layer->graph = graph;
  layer->releaseGraph = releaseGraph;
  gvizGraphVBOInit(&layer->vbo);
  layer->gpuDirty = 2;
}

void gvizLayerGraphDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerGraph *self = (gvizLayerGraph *)layerV;
  if (!self->graph) return;

  if (self->gpuDirty >= 2 || !self->vbo.vaoId)
    gvizGraphVBORebuild(&self->vbo, self->graph);
  else if (self->gpuDirty == 1)
    gvizGraphVBOUploadPositions(&self->vbo, self->graph);
  self->gpuDirty = 0;

  gvizGraphVBODraw(&self->vbo);
}

void gvizLayerGraphUpdate(void *layer, float dt) {
  (void)layer;
  (void)dt;
}

void gvizLayerGraphRelease(void *layer) {
  gvizLayerGraph *self = (gvizLayerGraph *)layer;
  gvizGraphVBORelease(&self->vbo);
  if (self->releaseGraph && self->graph)
    self->releaseGraph(self->graph);
  self->graph = NULL;
}

int gvizLayerGraphHandleEvent(void *layer, const gvizEvent *event) {
  (void)layer;
  (void)event;
  return 0;
}

int gvizLayerGraphHitTest(void *layer, float wx, float wy) {
  gvizLayerGraph *self = (gvizLayerGraph *)layer;
  gvizEmbeddedGraph *eg = self->graph;
  if (!eg)
    return 0;
  const float hitR = 6.0f;
  const float hitR2 = hitR * hitR;
  for (size_t i = 0; i < eg->graph->vertices.count; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, i);
    float dx = (float)p[0] - wx;
    float dy = (float)p[1] - wy;
    if (dx * dx + dy * dy <= hitR2)
      return 1;
  }
  return 0;
}
