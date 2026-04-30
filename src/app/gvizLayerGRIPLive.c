#include "app/gvizLayerGRIPLive.h"
#include "cblas.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizVertexDiscVBO.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_LAYER_GRIP_LIVE_SIMPLEX_SIDE 10000.0
#define GVIZ_LAYER_GRIP_LIVE_VERTEX_RADIUS 2.0f

static void applyRadiiMask(gvizLayerGRIPLive *self) {
  if (!self->vbo.radii)
    return;
  size_t N = self->graph.vertices.count;
  if (self->vbo.radiiCount < N)
    N = self->vbo.radiiCount;
  for (size_t i = 0; i < N; i++) {
    self->vbo.radii[i] = gvizTestBit(self->placedBits, i)
                             ? GVIZ_LAYER_GRIP_LIVE_VERTEX_RADIUS
                             : 0.0f;
  }
  if (self->vbo.discs.vaoId)
    gvizVertexDiscVBOUploadRadii(&self->vbo.discs, self->vbo.radii, N);
}

static int gvizLayerGRIPLiveInitWithDim(gvizLayerGRIPLive *self, gvizGraph *graph,
                                        size_t diameter, size_t z,
                                        size_t embDim, size_t drawDim) {
  gvizLayerInit((gvizLayer *)self, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_GRIP_LIVE_VTABLE, z);
  if (drawDim == 3) {
    self->camera = gvizCameraMake3D(
        (Vector3){0.0f, 0.0f, 1000.0f}, (Vector3){0.0f, 0.0f, 0.0f},
        (Vector3){0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE);
  } else {
    self->camera = gvizCameraMake2D((Vector2){0, 0}, (Vector2){0, 0}, 0.0f, 1.0f);
  }
  self->drawDim = drawDim;
  self->layer.flags = GVIZ_LAYER_VISIBLE;
  self->placedBits = NULL;
  self->layerKNNs = NULL;
  self->layerCount = 0;
  self->currentLayer = -1;
  self->currentRound = 0;
  self->gpuDirty = 2;
  self->scene = NULL;
  self->graphHandle = GVIZ_SCENE_GRAPH_INVALID;

  if (gvizGraphClone(&self->graph, graph) != 0)
    return -1;

  if (gvizGRIPEmbeddingInit(&self->grip, &self->graph, diameter, embDim) != 0) {
    gvizGraphRelease(&self->graph);
    return -1;
  }

  self->layerCount = createMISFiltration(&self->grip);

  size_t N = self->graph.vertices.count;
  size_t units = GVIZ_ARRAY_UNITS(N);
  self->placedBits = GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * units);
  if (!self->placedBits) {
    gvizGRIPEmbeddingRelease(&self->grip);
    gvizGraphRelease(&self->graph);
    return -1;
  }
  memset(self->placedBits, 0, sizeof(GVIZ_BIT_UNIT) * units);

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->grip;
  size_t simplexDim = eg->embedding.dim;
  double simplex[simplexDim * (simplexDim + 1)];
  makeRegularSimplex(simplexDim, GVIZ_LAYER_GRIP_LIVE_SIMPLEX_SIDE, simplex);
  for (size_t j = 0; j < simplexDim + 1; j++) {
    size_t v = self->grip.misFiltration[j];
    gvizSetBit(self->placedBits, v);
    cblas_dcopy(simplexDim, simplex + j * simplexDim, 1,
                gvizEmbeddedGraphGetVPosition(eg, v), 1);
  }

  self->currentLayer = (int)self->layerCount - 1;
  self->currentRound = 0;
  self->layerKNNs = gvizGRIPPrepareLayerKNNs(&self->grip,
                                              (size_t)self->currentLayer,
                                              self->placedBits);

  gvizGraphVBOInit(&self->vbo);
  self->vbo.drawDim = (int)drawDim;
  self->vbo.discFill = 1.0f;
  gvizGraphVBOSetMode(&self->vbo, GVIZ_GRAPH_VBO_DISCS);
  self->gpuDirty = 2;
  return 0;
}

int gvizLayerGRIPLiveInit(gvizLayerGRIPLive *self, gvizGraph *graph,
                          size_t diameter, size_t z) {
  return gvizLayerGRIPLiveInitWithDim(self, graph, diameter, z, 2, 2);
}

int gvizLayerGRIPLiveInit3D(gvizLayerGRIPLive *self, gvizGraph *graph,
                            size_t diameter, size_t z) {
  return gvizLayerGRIPLiveInitWithDim(self, graph, diameter, z, 3, 3);
}

int gvizLayerGRIPLiveInitND(gvizLayerGRIPLive *self, gvizGraph *graph,
                            size_t diameter, size_t z,
                            size_t embDim, size_t drawDim) {
  if (embDim < 2) embDim = 2;
  if (drawDim != 2 && drawDim != 3) drawDim = 2;
  if (embDim < drawDim) embDim = drawDim;
  return gvizLayerGRIPLiveInitWithDim(self, graph, diameter, z, embDim, drawDim);
}

int gvizLayerGRIPLiveInitEmpty3D(gvizLayerGRIPLive *self, size_t z) {
  gvizLayerInit((gvizLayer *)self, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_GRIP_LIVE_VTABLE, z);
  self->camera = gvizCameraMake3D(
      (Vector3){0.0f, 0.0f, 1000.0f}, (Vector3){0.0f, 0.0f, 0.0f},
      (Vector3){0.0f, 1.0f, 0.0f}, 45.0f, CAMERA_PERSPECTIVE);
  self->drawDim = 3;
  self->layer.flags = GVIZ_LAYER_VISIBLE;
  self->placedBits = NULL;
  self->layerKNNs = NULL;
  self->layerCount = 0;
  self->currentLayer = -1;
  self->currentRound = 0;
  self->gpuDirty = 0;
  self->scene = NULL;
  self->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  if (gvizGraphInit(&self->graph, 0) != 0)
    return -1;
  memset(&self->grip, 0, sizeof(self->grip));
  gvizGraphVBOInit(&self->vbo);
  self->vbo.drawDim = 3;
  self->vbo.discFill = 1.0f;
  gvizGraphVBOSetMode(&self->vbo, GVIZ_GRAPH_VBO_DISCS);
  return 0;
}

void gvizLayerGRIPLiveAdvance(gvizLayerGRIPLive *self) {
  if (self->currentLayer < 0)
    return;

  gvizGRIPReleaseLayerKNNs(&self->grip, (size_t)self->currentLayer,
                            self->layerKNNs);
  self->layerKNNs = NULL;
  self->currentLayer--;

  if (self->currentLayer < 0) {
    gvizGraphVBOSetMode(&self->vbo, GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS);
    self->gpuDirty = 2;
    return;
  }

  placeLayerVertices(&self->grip, (size_t)self->currentLayer, self->placedBits);
  self->layerKNNs = gvizGRIPPrepareLayerKNNs(&self->grip,
                                              (size_t)self->currentLayer,
                                              self->placedBits);
  self->currentRound = 0;
  self->gpuDirty = 2;
}

void gvizLayerGRIPLiveGetContentBounds(void *layerV,
                                       Vector3 *centroid, float *radius) {
  gvizLayerGRIPLive *self = (gvizLayerGRIPLive *)layerV;
  gvizEmbeddedGraph *eg   = (gvizEmbeddedGraph *)&self->grip;
  size_t n   = self->graph.vertices.count;
  size_t dim = eg->embedding.dim;
  *centroid = (Vector3){0};
  *radius   = 0.0f;
  if (n == 0 || dim < 2) return;

  double cx = 0, cy = 0, cz = 0;
  for (size_t i = 0; i < n; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, i);
    cx += p[0]; cy += p[1];
    if (dim >= 3) cz += p[2];
  }
  cx /= (double)n; cy /= (double)n; cz /= (double)n;
  centroid->x = (float)cx; centroid->y = (float)cy; centroid->z = (float)cz;

  float maxR = 0.0f;
  for (size_t i = 0; i < n; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, i);
    float dx = (float)p[0] - centroid->x;
    float dy = (float)p[1] - centroid->y;
    float dz = (dim >= 3) ? (float)p[2] - centroid->z : 0.0f;
    float d  = sqrtf(dx*dx + dy*dy + dz*dz);
    if (d > maxR) maxR = d;
  }
  *radius = maxR;
}

void gvizLayerGRIPLiveUpdate(void *layerV, float dt) {
  gvizLayerGRIPLive *self = (gvizLayerGRIPLive *)layerV;
  gvizEmbeddedGraph *eg   = (gvizEmbeddedGraph *)&self->grip;
  size_t n   = self->graph.vertices.count;
  size_t dim = eg->embedding.dim;

  /* R held: rotate all positions about centroid along camera up axis */
  if (IsKeyDown(KEY_R) && n > 0 && dim >= 3 && self->drawDim >= 3) {
    Vector3 centroid; float radius;
    gvizLayerGRIPLiveGetContentBounds(self, &centroid, &radius);
    Vector3 up  = Vector3Normalize(self->camera.c3d.up);
    Matrix  rot = MatrixRotate(up, 1.5f * dt);
    for (size_t i = 0; i < n; i++) {
      double *p = gvizEmbeddedGraphGetVPosition(eg, i);
      Vector3 v = {(float)p[0] - centroid.x,
                   (float)p[1] - centroid.y,
                   (float)p[2] - centroid.z};
      v = Vector3Transform(v, rot);
      p[0] = (double)(v.x + centroid.x);
      p[1] = (double)(v.y + centroid.y);
      p[2] = (double)(v.z + centroid.z);
    }
    if (self->gpuDirty < 1) self->gpuDirty = 1;
  }

  if (self->currentLayer < 0) return;

  gvizGRIPRefineOneRound(&self->grip, (size_t)self->currentLayer,
                         self->layerKNNs);
  self->currentRound++;
  if (self->gpuDirty < 1) self->gpuDirty = 1;
  (void)dt;
}

void gvizLayerGRIPLiveDraw(void *layerV, const gvizCamera *camera) {
  gvizLayerGRIPLive *self = (gvizLayerGRIPLive *)layerV;
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->grip;

  if (self->graph.vertices.count == 0) {
    self->gpuDirty = 0;
    return;
  }

  if (self->gpuDirty >= 2 || !self->vbo.vaoId)
    gvizGraphVBORebuild(&self->vbo, eg);
  else if (self->gpuDirty == 1)
    gvizGraphVBOUploadPositions(&self->vbo, eg);

  if (self->gpuDirty > 0)
    applyRadiiMask(self);

  self->gpuDirty = 0;
  gvizGraphVBODraw(&self->vbo);

  /* Draw HUD text in screen space. */
  if (camera->kind == GVIZ_CAMERA_2D) EndMode2D(); else EndMode3D();

  char buf[128];
  if (self->currentLayer >= 0) {
    int displayLayer = (int)self->layerCount - 1 - self->currentLayer;
    snprintf(buf, sizeof(buf),
             "Layer %d / %d   Round %d   SPACE  advance   scroll/drag  pan+zoom",
             displayLayer, (int)self->layerCount - 1, self->currentRound);
  } else {
    snprintf(buf, sizeof(buf),
             "Done   %zu vertices   %zu layers   scroll/drag  pan+zoom",
             self->graph.vertices.count, self->layerCount);
  }
  DrawText(buf, 10, 10, 18, DARKGRAY);

  /* Restart the matching mode so subsequent layers draw correctly. */
  if (camera->kind == GVIZ_CAMERA_2D) BeginMode2D(camera->c2d);
  else BeginMode3D(camera->c3d);
}

struct gvizCamera *gvizLayerGRIPLiveGetCamera(void *layer) {
  return &((gvizLayerGRIPLive *)layer)->camera;
}

void gvizLayerGRIPLiveBindHandle(gvizLayerGRIPLive *layer, gvizScene *scene,
                                 gvizSceneGraphHandle h, gvizGraphCallback cb) {
  layer->scene = scene;
  layer->graphHandle = h;
  gvizSceneRetainGraph(scene, h);
  if (cb) gvizSceneSubscribeGraph(scene, h, layer, cb);
}

void gvizLayerGRIPLiveRelease(void *layerV) {
  gvizLayerGRIPLive *self = (gvizLayerGRIPLive *)layerV;
  if (self->scene && self->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizSceneUnsubscribeGraph(self->scene, self->graphHandle, self);
    gvizSceneReleaseGraph(self->scene, self->graphHandle);
    self->scene = NULL;
    self->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  }
  if (self->layerKNNs && self->currentLayer >= 0) {
    gvizGRIPReleaseLayerKNNs(&self->grip, (size_t)self->currentLayer,
                             self->layerKNNs);
    self->layerKNNs = NULL;
  }
  gvizGraphVBORelease(&self->vbo);
  gvizGRIPEmbeddingRelease(&self->grip);
  gvizGraphRelease(&self->graph);
  if (self->placedBits) {
    GVIZ_DEALLOC(self->placedBits);
    self->placedBits = NULL;
  }
}

int gvizLayerGRIPLiveHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerGRIPLive *self = (gvizLayerGRIPLive *)layerV;
  if (event->type == GVIZ_EVENT_KEY_DOWN && event->key.key == KEY_SPACE) {
    gvizLayerGRIPLiveAdvance(self);
    return 1;
  }
  return 0;
}
