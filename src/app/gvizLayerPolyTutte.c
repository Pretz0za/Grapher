#include "app/gvizLayerPolyTutte.h"
#include "core/alloc.h"
#include "core/event.h"
#include "core/gvizCamera.h"
#include "dsa/gvizArray.h"
#include "raylib.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizPlanarEmbedding.h"
#include "renderer/embeddings/gvizPlanarRotation.h"
#include "renderer/layers/gvizGraphVBO.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void pt_writeColors(gvizLayerPolyTutte *self, gvizEmbeddedGraph *eg);

/* ---- Internal highlight helpers (shape mirrors gvizLayerGraph) ----------- */

static size_t pt_computeEdgeStartIdx(gvizGraph *g, size_t *out) {
  size_t N = g->vertices.count;
  size_t total = 0;
  for (size_t i = 0; i < N; i++) {
    out[i] = total;
    total += gvizGraphGetVertexNeighbors(g, i)->count;
  }
  out[N] = total;
  return total;
}

static void pt_rebuildIndex(gvizLayerPolyTutte *self) {
  gvizGraph *g = &self->graph;
  size_t N = g->vertices.count;

  size_t *newStart =
      (size_t *)GVIZ_REALLOC(self->edgeStartIdx, (N + 1) * sizeof(size_t));
  if (!newStart)
    return;
  self->edgeStartIdx = newStart;
  size_t total = pt_computeEdgeStartIdx(g, self->edgeStartIdx);

  size_t eunits = GVIZ_ARRAY_UNITS(total ? total : 1);
  GVIZ_BIT_UNIT *neb = (GVIZ_BIT_UNIT *)GVIZ_REALLOC(
      self->edgeHighlight, eunits * sizeof(GVIZ_BIT_UNIT));
  if (neb) {
    self->edgeHighlight = neb;
    self->edgeHighlightUnits = eunits;
    memset(self->edgeHighlight, 0, eunits * sizeof(GVIZ_BIT_UNIT));
  }
  self->edgeHighlightBits = total;

  size_t vunits = GVIZ_ARRAY_UNITS(N ? N : 1);
  GVIZ_BIT_UNIT *nvb = (GVIZ_BIT_UNIT *)GVIZ_REALLOC(
      self->vertexHighlight, vunits * sizeof(GVIZ_BIT_UNIT));
  if (nvb) {
    self->vertexHighlight = nvb;
    self->vertexHighlightUnits = vunits;
    memset(self->vertexHighlight, 0, vunits * sizeof(GVIZ_BIT_UNIT));
  }
  self->vertexHighlightBits = N;
  self->highlightDirty = 1;
}

static void pt_clearHighlights(gvizLayerPolyTutte *self) {
  if (self->vertexHighlight && self->vertexHighlightUnits)
    memset(self->vertexHighlight, 0,
           self->vertexHighlightUnits * sizeof(GVIZ_BIT_UNIT));
  if (self->edgeHighlight && self->edgeHighlightUnits)
    memset(self->edgeHighlight, 0,
           self->edgeHighlightUnits * sizeof(GVIZ_BIT_UNIT));
  pt_writeColors(self, (gvizEmbeddedGraph *)&self->tutte);
  self->highlightDirty = 0;
}

static void pt_setVertexHL(gvizLayerPolyTutte *self, size_t v) {
  if (!self->vertexHighlight || v >= self->vertexHighlightBits)
    return;
  gvizSetBit(self->vertexHighlight, v);
  self->highlightDirty = 1;
}

static void pt_setEdgeHL(gvizLayerPolyTutte *self, size_t u, size_t v) {
  gvizGraph *g = &self->graph;
  if (u >= g->vertices.count)
    return;
  gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
  for (size_t j = 0; j < nb->count; j++) {
    if (*(size_t *)gvizArrayAtIndex(nb, j) == v) {
      size_t bit = self->edgeStartIdx[u] + j;
      if (bit < self->edgeHighlightBits)
        gvizSetBit(self->edgeHighlight, bit);
      break;
    }
  }
  if (!g->directed) {
    gvizArray *nb2 = gvizGraphGetVertexNeighbors(g, v);
    for (size_t j = 0; j < nb2->count; j++) {
      if (*(size_t *)gvizArrayAtIndex(nb2, j) == u) {
        size_t bit = self->edgeStartIdx[v] + j;
        if (bit < self->edgeHighlightBits)
          gvizSetBit(self->edgeHighlight, bit);
        break;
      }
    }
  }
  self->highlightDirty = 1;
}

static void pt_highlightFace(gvizLayerPolyTutte *self, size_t faceIdx) {
  pt_clearHighlights(self);
  if (faceIdx >= self->faces.count)
    return;
  gvizArray *face = (gvizArray *)gvizArrayAtIndex(&self->faces, faceIdx);
  size_t n = face->count;
  size_t *verts = (size_t *)face->arr;
  for (size_t i = 0; i < n; i++)
    pt_setVertexHL(self, verts[i]);
  for (size_t i = 0; i < n; i++) {
    size_t a = verts[i];
    size_t b = verts[(i + 1) % n];
    pt_setEdgeHL(self, a, b);
  }
}

static void pt_writeColors(gvizLayerPolyTutte *self, gvizEmbeddedGraph *eg) {
  if (self->vbo.colorsCount == 0 || !self->vbo.colors)
    return;
  gvizGraph *g = eg->graph;
  size_t N = g->vertices.count;
  size_t fi = 0;
  for (size_t u = 0; u < N; u++) {
    gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, u);
    int uHi = (self->vertexHighlight && u < self->vertexHighlightBits)
                  ? (gvizTestBit(self->vertexHighlight, u) ? 1 : 0)
                  : 0;
    for (size_t j = 0; j < nbrs->count; j++) {
      size_t v = *(size_t *)gvizArrayAtIndex(nbrs, j);
      size_t bit = self->edgeStartIdx[u] + j;
      int eHi = (self->edgeHighlight && bit < self->edgeHighlightBits)
                    ? (gvizTestBit(self->edgeHighlight, bit) ? 1 : 0)
                    : 0;
      int vHi = (self->vertexHighlight && v < self->vertexHighlightBits)
                    ? (gvizTestBit(self->vertexHighlight, v) ? 1 : 0)
                    : 0;
      float r1 = (uHi || eHi) ? 1.0f : 0.0f;
      float r2 = (vHi || eHi) ? 1.0f : 0.0f;
      if (fi + 6 > self->vbo.colorsCount)
        return;
      self->vbo.colors[fi++] = r1;
      self->vbo.colors[fi++] = 0.0f;
      self->vbo.colors[fi++] = 0.0f;
      self->vbo.colors[fi++] = r2;
      self->vbo.colors[fi++] = 0.0f;
      self->vbo.colors[fi++] = 0.0f;
    }
  }
  gvizGraphVBOUploadEndpointColors(&self->vbo, self->vbo.colors);

  if (self->vbo.discHighlights && self->vbo.radiiCount == N) {
    for (size_t u = 0; u < N; u++) {
      int hi = (self->vertexHighlight && u < self->vertexHighlightBits)
                   ? (gvizTestBit(self->vertexHighlight, u) ? 1 : 0)
                   : 0;
      self->vbo.discHighlights[u] = hi ? 1.0f : 0.0f;
    }
    gvizGraphVBOSetDiscHighlights(&self->vbo, self->vbo.discHighlights, N);
  }
}

/* ---- Faces / area -------------------------------------------------------- */

static double polygonArea2D(const size_t *faceVerts, size_t n,
                            gvizEmbeddedGraph *eg) {
  if (n < 3)
    return 0.0;
  double area = 0.0;
  for (size_t i = 0; i < n; i++) {
    size_t a = faceVerts[i];
    size_t b = faceVerts[(i + 1) % n];
    double *pa = gvizEmbeddedGraphGetVPosition(eg, a);
    double *pb = gvizEmbeddedGraphGetVPosition(eg, b);
    area += pa[0] * pb[1] - pb[0] * pa[1];
  }
  return fabs(area) * 0.5;
}

static void releaseFaces(gvizLayerPolyTutte *self) {
  for (size_t i = 0; i < self->faces.count; i++) {
    gvizArray *f = (gvizArray *)gvizArrayAtIndex(&self->faces, i);
    gvizArrayRelease(f);
  }
  gvizArrayRelease(&self->faces);
  gvizArrayInit(&self->faces, sizeof(gvizArray));
}

/* ---- Lifecycle ----------------------------------------------------------- */

int gvizLayerPolyTutteInit(gvizLayerPolyTutte *layer, gvizGraph *mesh,
                           const size_t *outerFace, size_t outerFaceLen,
                           size_t z) {
  if (outerFaceLen < 3)
    outerFaceLen = 3;
  gvizLayerInit((gvizLayer *)layer, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_POLY_TUTTE_VTABLE, z);
  layer->camera = gvizCameraMake2D((Vector2){0, 0}, (Vector2){0, 0}, 0.0f, 1.0f);
  layer->layer.flags = GVIZ_LAYER_VISIBLE;
  layer->gpuDirty = 2;
  layer->highlightDirty = 1;
  layer->hasTutte = 0;
  layer->vertexHighlight = NULL;
  layer->vertexHighlightBits = 0;
  layer->vertexHighlightUnits = 0;
  layer->edgeHighlight = NULL;
  layer->edgeHighlightBits = 0;
  layer->edgeHighlightUnits = 0;
  layer->edgeStartIdx = NULL;
  layer->selectedFaceIdx = SIZE_MAX;
  layer->outerFaceIdx = SIZE_MAX;
  layer->phase = GVIZ_POLY_TUTTE_INITIAL;
  layer->scanFaceIdx = 0;
  layer->bestFaceIdx = 0;
  layer->bestFaceVertCount = 0;
  layer->boundaryRadius = 300.0;
  layer->scanTimer = 0.0f;
  layer->lastCamera = NULL;
  layer->scene = NULL;
  layer->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  gvizArrayInit(&layer->faces, sizeof(gvizArray));

  if (gvizGraphClone(&layer->graph, mesh) != 0)
    return -1;

  if (gvizTutteEmbeddingInit(&layer->tutte, &layer->graph, 2, 0) != 0) {
    gvizGraphRelease(&layer->graph);
    return -1;
  }

  gvizTutteFixConvexPolygon(&layer->tutte, outerFace, outerFaceLen,
                            layer->boundaryRadius);
  gvizTutteEmbeddingSeedInterior(&layer->tutte);
  layer->hasTutte = 1;

  gvizGraphVBOInit(&layer->vbo);
  gvizGraphVBOSetMode(&layer->vbo, GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS);
  layer->vbo.discFill = 1.0f;
  gvizGraphVBOSetAllRadii(&layer->vbo, 3.5f);

  pt_rebuildIndex(layer);
  return 0;
}

void gvizLayerPolyTutteDraw(void *layerV, const gvizCamera *camera) {
  gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;
  self->lastCamera = camera;
  if (self->graph.vertices.count == 0) {
    self->gpuDirty = 0;
    return;
  }

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->tutte;

  int rebuilt = 0;
  if (self->gpuDirty >= 2 || !self->vbo.vaoId) {
    gvizGraphVBORebuild(&self->vbo, eg);
    gvizGraphVBOSetAllRadii(&self->vbo, 3.5f);
    rebuilt = 1;
  } else if (self->gpuDirty == 1)
    gvizGraphVBOUploadPositions(&self->vbo, eg);
  self->gpuDirty = 0;

  if (rebuilt || self->highlightDirty) {
    pt_writeColors(self, eg);
    self->highlightDirty = 0;
  }

  gvizGraphVBODraw(&self->vbo);

  EndMode2D();
  const char *hud;
  switch (self->phase) {
  case GVIZ_POLY_TUTTE_SCANNING:
    hud = "scanning faces...   R  stop & pick random";
    break;
  case GVIZ_POLY_TUTTE_FINAL:
    hud = "embedding...   SPACE  scan again   R  new random face";
    break;
  case GVIZ_POLY_TUTTE_INITIAL:
  default:
    hud = "SPACE  scan all faces   R  random face   cmd-click  select face   "
          "ENTER  fix & re-embed";
    break;
  }
  DrawText(hud, 10, 10, 18, DARKGRAY);
  if (camera->kind == GVIZ_CAMERA_2D)
    BeginMode2D(camera->c2d);
}

void gvizLayerPolyTutteUpdate(void *layerV, float dt) {
  gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;

  if (self->hasTutte && !self->tutte.converged) {
    for (int i = 0; i < 5; i++)
      gvizTutteEmbeddingStep(&self->tutte);
    if (self->gpuDirty < 1)
      self->gpuDirty = 1;
  }

  if (self->phase == GVIZ_POLY_TUTTE_SCANNING) {
    self->scanTimer += dt;
    if (self->scanTimer < 0.2f)
      return;
    self->scanTimer = 0.0f;

    fprintf(stderr, "[PolyTutte] scanning face %zu / %zu\n", self->scanFaceIdx,
            self->faces.count);

    if (self->scanFaceIdx < self->faces.count) {
      gvizArray *face =
          (gvizArray *)gvizArrayAtIndex(&self->faces, self->scanFaceIdx);
      size_t n = face->count;
      if (n > self->bestFaceVertCount) {
        self->bestFaceVertCount = n;
        self->bestFaceIdx = self->scanFaceIdx;
      }
    }

    pt_clearHighlights(self);
    self->scanFaceIdx++;
    if (self->scanFaceIdx >= self->faces.count) {
      self->phase = GVIZ_POLY_TUTTE_FINAL;
      return;
    }
    pt_highlightFace(self, self->scanFaceIdx);
    return;
  }

  if (self->phase == GVIZ_POLY_TUTTE_FINAL) {
    pt_clearHighlights(self);
    if (self->faces.count > 0 && self->bestFaceIdx < self->faces.count) {
      gvizArray *best =
          (gvizArray *)gvizArrayAtIndex(&self->faces, self->bestFaceIdx);
      size_t bn = best->count;
      if (bn >= 3) {
        size_t *bv = (size_t *)best->arr;
        if (self->hasTutte) {
          gvizTutteEmbeddingRelease(&self->tutte);
          self->hasTutte = 0;
        }
        if (gvizTutteEmbeddingInit(&self->tutte, &self->graph, 2, 0) == 0) {
          gvizTutteFixConvexPolygon(&self->tutte, bv, bn, self->boundaryRadius);
          gvizTutteEmbeddingSeedInterior(&self->tutte);
          self->hasTutte = 1;
          self->gpuDirty = 2;
        }
      }
    }
    self->phase = GVIZ_POLY_TUTTE_INITIAL;
    return;
  }
}

struct gvizCamera *gvizLayerPolyTutteGetCamera(void *layer) {
  return &((gvizLayerPolyTutte *)layer)->camera;
}

void gvizLayerPolyTutteBindHandle(gvizLayerPolyTutte *layer, gvizScene *scene,
                                  gvizSceneGraphHandle h, gvizGraphCallback cb) {
  layer->scene = scene;
  layer->graphHandle = h;
  gvizSceneRetainGraph(scene, h);
  if (cb) gvizSceneSubscribeGraph(scene, h, layer, cb);
}

void gvizLayerPolyTutteRelease(void *layerV) {
  gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;
  if (self->scene && self->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizSceneUnsubscribeGraph(self->scene, self->graphHandle, self);
    gvizSceneReleaseGraph(self->scene, self->graphHandle);
    self->scene = NULL;
    self->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  }
  gvizGraphVBORelease(&self->vbo);
  if (self->hasTutte)
    gvizTutteEmbeddingRelease(&self->tutte);
  releaseFaces(self);
  gvizArrayRelease(&self->faces);
  gvizGraphRelease(&self->graph);
  if (self->vertexHighlight)
    GVIZ_DEALLOC(self->vertexHighlight);
  if (self->edgeHighlight)
    GVIZ_DEALLOC(self->edgeHighlight);
  if (self->edgeStartIdx)
    GVIZ_DEALLOC(self->edgeStartIdx);
  self->vertexHighlight = NULL;
  self->edgeHighlight = NULL;
  self->edgeStartIdx = NULL;
}

static int pt_pointInFace(const size_t *verts, size_t n, double px, double py,
                          gvizEmbeddedGraph *eg) {
  if (n < 3)
    return 0;
  py += 1e-10;
  int inside = 0;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    double *pa = gvizEmbeddedGraphGetVPosition(eg, verts[i]);
    double *pb = gvizEmbeddedGraphGetVPosition(eg, verts[j]);
    double xi = pa[0], yi = pa[1];
    double xj = pb[0], yj = pb[1];
    if (((yi > py) != (yj > py)) &&
        (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
      inside = !inside;
  }
  return inside;
}

static size_t pt_findOuterFace(gvizLayerPolyTutte *self) {
  if (self->faces.count == 0)
    return SIZE_MAX;
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->tutte;
  size_t bestIdx = 0;
  double bestArea = -DBL_MAX;
  for (size_t i = 0; i < self->faces.count; i++) {
    gvizArray *f = (gvizArray *)gvizArrayAtIndex(&self->faces, i);
    double a = polygonArea2D((size_t *)f->arr, f->count, eg);
    if (a > bestArea) {
      bestArea = a;
      bestIdx = i;
    }
  }
  return bestIdx;
}

static int pt_enumerateFaces(gvizLayerPolyTutte *self) {
  gvizComputeRotationSystem((gvizEmbeddedGraph *)&self->tutte);
  pt_rebuildIndex(self);
  self->gpuDirty = 2;

  releaseFaces(self);

  gvizPlanarEmbeddingState ps = {0};
  ps.embedding = self->tutte.graph;
  ps.kuratowskiSubdivision = NULL;

  gvizFaceIteratorContext ctx;
  if (gvizFaceIteratorInit(&ps, &ctx) != 0)
    return -1;
  if (gvizPlanarEmbeddingFaces(&ps, &ctx) != 0) {
    gvizFaceIteratorRelease(&ctx);
    return -1;
  }
  for (size_t i = 0; i < ctx.faces.count; i++) {
    gvizArray *src = (gvizArray *)gvizArrayAtIndex(&ctx.faces, i);
    gvizArray dst;
    gvizArrayClone(&dst, src);
    gvizArrayPush(&self->faces, &dst);
  }
  gvizFaceIteratorRelease(&ctx);
  self->outerFaceIdx = pt_findOuterFace(self);
  return 0;
}

int gvizLayerPolyTutteHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;

  if (event->type == GVIZ_EVENT_MOUSE_DOWN &&
      event->mouse.button == GVIZ_MOUSE_LEFT &&
      (event->mouse.mods & GVIZ_MOD_SUPER)) {
    if (self->faces.count == 0) {
      if (pt_enumerateFaces(self) != 0)
        return 1;
    }
    if (self->faces.count == 0)
      return 1;

    double px, py;
    if (self->lastCamera != NULL) {
      Vector2 w = gvizCameraScreenToWorld2D(
          self->lastCamera, (Vector2){event->mouse.sx, event->mouse.sy});
      px = (double)w.x;
      py = (double)w.y;
    } else {
      px = (double)event->mouse.wx;
      py = (double)event->mouse.wy;
    }
    fprintf(stderr,
            "[PolyTutte] cmd+click screen=(%.1f,%.1f) world=(%.2f,%.2f) "
            "faces=%zu\n",
            event->mouse.sx, event->mouse.sy, px, py, self->faces.count);
    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->tutte;

    size_t hit = SIZE_MAX;
    for (size_t i = 0; i < self->faces.count; i++) {
      if (i == self->outerFaceIdx)
        continue;
      gvizArray *f = (gvizArray *)gvizArrayAtIndex(&self->faces, i);
      if (pt_pointInFace((size_t *)f->arr, f->count, px, py, eg)) {
        hit = i;
        break;
      }
    }
    fprintf(stderr, "[PolyTutte] hit face=%zu (SIZE_MAX=%d)\n", hit,
            hit == SIZE_MAX);
    if (hit == SIZE_MAX)
      hit = self->outerFaceIdx;

    self->selectedFaceIdx = hit;
    pt_clearHighlights(self);
    pt_highlightFace(self, hit);
    return 1;
  }

  if (event->type != GVIZ_EVENT_KEY_DOWN)
    return 0;

  if (event->key.key == KEY_SPACE) {
    if (self->phase != GVIZ_POLY_TUTTE_INITIAL)
      return 0;
    if (pt_enumerateFaces(self) != 0)
      return 1;
    fprintf(stderr, "[PolyTutte] faces enumerated: %zu\n", self->faces.count);
    self->phase = GVIZ_POLY_TUTTE_SCANNING;
    self->scanFaceIdx = 0;
    self->bestFaceIdx = 0;
    self->bestFaceVertCount = 0;
    self->scanTimer = 0.0f;
    if (self->faces.count > 0)
      pt_highlightFace(self, 0);
    return 1;
  }

  if (event->key.key == KEY_ENTER) {
    if (self->selectedFaceIdx == SIZE_MAX || self->faces.count == 0)
      return 0;
    if (self->selectedFaceIdx >= self->faces.count)
      return 1;
    gvizArray *face =
        (gvizArray *)gvizArrayAtIndex(&self->faces, self->selectedFaceIdx);
    size_t bn = face->count;
    if (bn < 3)
      return 1;
    size_t *bv = (size_t *)face->arr;

    if (self->hasTutte) {
      gvizTutteEmbeddingRelease(&self->tutte);
      self->hasTutte = 0;
    }
    if (gvizTutteEmbeddingInit(&self->tutte, &self->graph, 2, 0) == 0) {
      gvizTutteFixConvexPolygon(&self->tutte, bv, bn, self->boundaryRadius);
      gvizTutteEmbeddingSeedInterior(&self->tutte);
      self->hasTutte = 1;
    }

    pt_rebuildIndex(self);
    pt_clearHighlights(self);
    releaseFaces(self);
    self->outerFaceIdx = SIZE_MAX;
    self->gpuDirty = 2;
    self->phase = GVIZ_POLY_TUTTE_INITIAL;
    self->selectedFaceIdx = SIZE_MAX;
    self->scanFaceIdx = 0;
    self->bestFaceVertCount = 0;
    self->scanTimer = 0.0f;
    return 1;
  }

  if (event->key.key == KEY_R) {
    if (self->faces.count == 0) {
      if (pt_enumerateFaces(self) != 0)
        return 1;
    }
    if (self->faces.count == 0)
      return 1;

    size_t randIdx = (size_t)rand() % self->faces.count;
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&self->faces, randIdx);
    size_t n = face->count;
    if (n < 3)
      return 1;
    size_t *verts = (size_t *)face->arr;

    if (self->hasTutte) {
      gvizTutteEmbeddingRelease(&self->tutte);
      self->hasTutte = 0;
    }
    if (gvizTutteEmbeddingInit(&self->tutte, &self->graph, 2, 0) == 0) {
      gvizTutteFixConvexPolygon(&self->tutte, verts, n, self->boundaryRadius);
      gvizTutteEmbeddingSeedInterior(&self->tutte);
      self->hasTutte = 1;
      self->gpuDirty = 2;
    }

    pt_clearHighlights(self);
    releaseFaces(self);
    self->outerFaceIdx = SIZE_MAX;
    self->phase = GVIZ_POLY_TUTTE_INITIAL;
    self->scanFaceIdx = 0;
    self->bestFaceVertCount = 0;
    self->scanTimer = 0.0f;
    return 1;
  }

  return 0;
}
