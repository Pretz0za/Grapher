#include "renderer/layers/gvizLayerGraph.h"
#include "core/alloc.h"
#include "raylib.h"
#include "raymath.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define VERTEXRADIUS 1.0
#define VERTEXTHICKNESS 1.0
#define EDGETHICKNESS 1.0
#define MAXRADIUS 250

void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        const gvizViewport viewport, size_t z) {
  gvizLayerInit((gvizLayer *)layer, viewport, &GVIZ_LAYER_GRAPH_VTABLE, z);
  layer->graph = graph;
  layer->ownsGraph = 1;
}

void gvizLayerGraphDraw(void *layer, const gvizCamera *camera) {
  (void)camera;
  gvizEmbeddedGraph *embedding = ((gvizLayerGraph *)layer)->graph;
  rlColor4ub(0, 0, 0, 255);

  rlBegin(RL_LINES);
  for (size_t i = 0; i < embedding->graph->vertices.count; i++) {
    double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
    gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, i);
    for (size_t j = 0; j < children->count; j++) {
      double *otherPos = gvizEmbeddedGraphGetVPosition(
          embedding, *(size_t *)gvizArrayAtIndex(children, j));

      rlVertex3f(pos[0], pos[1], pos[2]);
      rlVertex3f(otherPos[0], otherPos[1], otherPos[2]);
    }
  }
  rlEnd();
}

void gvizLayerGraphUpdate(void *layer, float dt) {
  (void)layer;
  (void)dt;
}

void gvizLayerGraphRelease(void *layer) {
  gvizLayerGraph *self = (gvizLayerGraph *)layer;
  if (self->ownsGraph && self->graph) {
    gvizGraph *g = self->graph->graph;
    gvizEmbeddedGraphRelease(self->graph);
    GVIZ_DEALLOC(self->graph);
    if (g) {
      gvizGraphRelease(g);
      GVIZ_DEALLOC(g);
    }
  }
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
