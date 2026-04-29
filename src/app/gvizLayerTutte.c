#include "app/gvizLayerTutte.h"
#include "core/alloc.h"
#include "core/event.h"
#include "core/gvizCamera.h"
#include "raylib.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizGraphVBO.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_LAYER_TUTTE_HIT_RADIUS 12.0

static int ensurePositionsCap(gvizLayerTutte *self, size_t need) {
  if (need <= self->positionsCap)
    return 0;
  size_t cap = self->positionsCap ? self->positionsCap : 16;
  while (cap < need)
    cap *= 2;
  double *grown =
      (double *)GVIZ_REALLOC(self->positions, cap * 2 * sizeof(double));
  if (!grown)
    return -1;
  self->positions = grown;
  self->positionsCap = cap;
  return 0;
}

static void syncPositionsFromTutte(gvizLayerTutte *self) {
  size_t N = self->graph.vertices.count;
  if (ensurePositionsCap(self, N) != 0)
    return;
  for (size_t i = 0; i < N; i++) {
    double *p =
        gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&self->tutte, i);
    self->positions[i * 2 + 0] = p[0];
    self->positions[i * 2 + 1] = p[1];
  }
}

static void copyPositionsIntoTutte(gvizLayerTutte *self) {
  size_t N = self->graph.vertices.count;
  for (size_t i = 0; i < N; i++) {
    double p[2] = {self->positions[i * 2 + 0], self->positions[i * 2 + 1]};
    gvizEmbeddedGraphSetVPosition((gvizEmbeddedGraph *)&self->tutte, i, p);
  }
}

static size_t findVertexAtWorld(gvizLayerTutte *self, double wx, double wy,
                                double radius) {
  size_t N = self->graph.vertices.count;
  double r2 = radius * radius;
  size_t hit = SIZE_MAX;
  double best = r2;
  for (size_t i = 0; i < N; i++) {
    double px, py;
    if (self->hasTutte) {
      double *p =
          gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&self->tutte, i);
      px = p[0];
      py = p[1];
    } else {
      px = self->positions[i * 2 + 0];
      py = self->positions[i * 2 + 1];
    }
    double dx = px - wx, dy = py - wy;
    double d2 = dx * dx + dy * dy;
    if (d2 < best) {
      best = d2;
      hit = i;
    }
  }
  return hit;
}

int gvizLayerTutteInit(gvizLayerTutte *layer, gvizGraph *g,
                       const size_t *boundary, size_t boundaryCount,
                       double boundaryRadius, size_t z) {
  gvizLayerInit((gvizLayer *)layer, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_TUTTE_VTABLE, z);
  layer->camera = gvizCameraMake2D((Vector2){0, 0}, (Vector2){0, 0}, 0.0f, 1.0f);
  layer->layer.flags = GVIZ_LAYER_VISIBLE;
  layer->paused = 0;
  layer->pendingVertex = SIZE_MAX;
  layer->positions = NULL;
  layer->positionsCap = 0;
  layer->scene = NULL;
  layer->graphHandle = GVIZ_SCENE_GRAPH_INVALID;

  if (gvizGraphClone(&layer->graph, g) != 0)
    return -1;

  if (gvizTutteEmbeddingInit(&layer->tutte, &layer->graph, 2, 1e-5) != 0) {
    gvizGraphRelease(&layer->graph);
    return -1;
  }
  gvizTutteFixConvexPolygon(&layer->tutte, boundary, boundaryCount,
                            boundaryRadius);
  gvizTutteEmbeddingSeedInterior(&layer->tutte);
  layer->hasTutte = 1;

  syncPositionsFromTutte(layer);

  gvizGraphVBOInit(&layer->vbo);
  gvizGraphVBOSetMode(&layer->vbo, GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS);
  layer->gpuDirty = 2;
  return 0;
}

int gvizLayerTutteInitEmpty(gvizLayerTutte *layer, size_t z) {
  gvizLayerInit((gvizLayer *)layer, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_TUTTE_VTABLE, z);
  layer->camera = gvizCameraMake2D((Vector2){0, 0}, (Vector2){0, 0}, 0.0f, 1.0f);
  layer->layer.flags = GVIZ_LAYER_VISIBLE;
  layer->paused = 0;
  layer->hasTutte = 0;
  layer->pendingVertex = SIZE_MAX;
  layer->positions = NULL;
  layer->positionsCap = 0;
  layer->scene = NULL;
  layer->graphHandle = GVIZ_SCENE_GRAPH_INVALID;

  if (gvizGraphInit(&layer->graph, 0) != 0)
    return -1;

  gvizGraphVBOInit(&layer->vbo);
  gvizGraphVBOSetMode(&layer->vbo, GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS);
  layer->gpuDirty = 2;
  return 0;
}

void gvizLayerTutteDraw(void *layerV, const gvizCamera *camera) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;

  gvizEmbeddedGraph tmpEG;
  gvizEmbeddedGraph *eg;
  if (self->hasTutte) {
    eg = (gvizEmbeddedGraph *)&self->tutte;
  } else {
    tmpEG.graph = &self->graph;
    tmpEG.embedding.dim = 2;
    tmpEG.embedding.vertexPositions = self->positions;
    eg = &tmpEG;
  }

  if (self->graph.vertices.count == 0) {
    self->gpuDirty = 0;
    return;
  }

  if (self->gpuDirty >= 2 || !self->vbo.vaoId)
    gvizGraphVBORebuild(&self->vbo, eg);
  else if (self->gpuDirty == 1)
    gvizGraphVBOUploadPositions(&self->vbo, eg);
  self->gpuDirty = 0;

  gvizGraphVBODraw(&self->vbo);

  if (camera->kind == GVIZ_CAMERA_2D) EndMode2D(); else EndMode3D();
  DrawText("SPACE  pause   S  single step   R  reseed   right-click  add vertex/edge   scroll/drag  pan+zoom",
           10, 10, 18, DARKGRAY);
  if (camera->kind == GVIZ_CAMERA_2D) BeginMode2D(camera->c2d);
  else BeginMode3D(camera->c3d);
}

void gvizLayerTutteUpdate(void *layerV, float dt) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;

  if (self->hasTutte && !self->paused && self->tutte.numInterior > 0 &&
      !self->tutte.converged) {
    gvizTutteEmbeddingStep(&self->tutte);
    syncPositionsFromTutte(self);
    if (self->gpuDirty < 1)
      self->gpuDirty = 1;
  }
}

struct gvizCamera *gvizLayerTutteGetCamera(void *layer) {
  return &((gvizLayerTutte *)layer)->camera;
}

void gvizLayerTutteBindHandle(gvizLayerTutte *layer, gvizScene *scene,
                              gvizSceneGraphHandle h, gvizGraphCallback cb) {
  layer->scene = scene;
  layer->graphHandle = h;
  gvizSceneRetainGraph(scene, h);
  if (cb) gvizSceneSubscribeGraph(scene, h, layer, cb);
}

void gvizLayerTutteRelease(void *layerV) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;
  if (self->scene && self->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizSceneUnsubscribeGraph(self->scene, self->graphHandle, self);
    gvizSceneReleaseGraph(self->scene, self->graphHandle);
    self->scene = NULL;
    self->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  }
  gvizGraphVBORelease(&self->vbo);
  if (self->hasTutte)
    gvizTutteEmbeddingRelease(&self->tutte);
  if (self->positions)
    GVIZ_DEALLOC(self->positions);
  self->positions = NULL;
  self->positionsCap = 0;
  gvizGraphRelease(&self->graph);
}

int gvizLayerTutteHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerTutte *self = (gvizLayerTutte *)layerV;

  if (event->type == GVIZ_EVENT_KEY_DOWN) {
    switch (event->key.key) {
    case KEY_SPACE:
      self->paused = !self->paused;
      return 1;
    case KEY_S:
      if (self->hasTutte) {
        gvizTutteEmbeddingStep(&self->tutte);
        syncPositionsFromTutte(self);
        self->gpuDirty = 1;
      }
      return 1;
    case KEY_R:
      if (self->hasTutte) {
        gvizTutteEmbeddingSeedInterior(&self->tutte);
        syncPositionsFromTutte(self);
        self->gpuDirty = 1;
      }
      return 1;
    }
    return 0;
  }

  if (event->type == GVIZ_EVENT_MOUSE_DOWN &&
      event->mouse.button == GVIZ_MOUSE_RIGHT) {
    double wx = (double)event->mouse.wx;
    double wy = (double)event->mouse.wy;
    size_t hit = findVertexAtWorld(self, wx, wy, GVIZ_LAYER_TUTTE_HIT_RADIUS);

    if (hit == SIZE_MAX) {
      gvizGraphAddVertex(&self->graph, NULL, NULL, NULL);
      size_t v = self->graph.vertices.count - 1;
      if (ensurePositionsCap(self, v + 1) != 0)
        return 1;
      self->positions[v * 2 + 0] = wx;
      self->positions[v * 2 + 1] = wy;
      self->hasTutte = 0;
      self->gpuDirty = 2;
      self->pendingVertex = SIZE_MAX;
    } else {
      if (self->pendingVertex == SIZE_MAX) {
        self->pendingVertex = hit;
      } else if (hit != self->pendingVertex) {
        gvizGraphAddEdge(&self->graph, self->pendingVertex, hit);
        self->pendingVertex = SIZE_MAX;
        self->hasTutte = 0;
        self->gpuDirty = 2;
      } else {
        self->pendingVertex = SIZE_MAX;
      }
    }
    return 1;
  }

  return 0;
}
