#include "renderer/layers/gvizLayerGraph.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizGraphVBO.h"
#include <stdlib.h>
#include <string.h>

static size_t computeEdgeStartIdx(gvizGraph *g, size_t *out) {
  size_t N = g->vertices.count;
  size_t total = 0;
  for (size_t i = 0; i < N; i++) {
    out[i] = total;
    total += gvizGraphGetVertexNeighbors(g, i)->count;
  }
  out[N] = total;
  return total;
}

static void allocHighlightBuffers(gvizLayerGraph *layer) {
  gvizGraph *g = layer->graph ? layer->graph->graph : NULL;
  size_t N = g ? g->vertices.count : 0;

  size_t vbits = N;
  size_t vunits = GVIZ_ARRAY_UNITS(vbits ? vbits : 1);
  layer->vertexHighlight = (GVIZ_BIT_UNIT *)GVIZ_ALLOC(vunits * sizeof(GVIZ_BIT_UNIT));
  if (layer->vertexHighlight)
    memset(layer->vertexHighlight, 0, vunits * sizeof(GVIZ_BIT_UNIT));
  layer->vertexHighlightBits = vbits;

  layer->edgeStartIdx = (size_t *)GVIZ_ALLOC((N + 1) * sizeof(size_t));
  size_t total = 0;
  if (layer->edgeStartIdx && g)
    total = computeEdgeStartIdx(g, layer->edgeStartIdx);
  else if (layer->edgeStartIdx)
    layer->edgeStartIdx[0] = 0;

  size_t ebits = total;
  size_t eunits = GVIZ_ARRAY_UNITS(ebits ? ebits : 1);
  layer->edgeHighlight = (GVIZ_BIT_UNIT *)GVIZ_ALLOC(eunits * sizeof(GVIZ_BIT_UNIT));
  if (layer->edgeHighlight)
    memset(layer->edgeHighlight, 0, eunits * sizeof(GVIZ_BIT_UNIT));
  layer->edgeHighlightBits = ebits;
}

void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        void (*releaseGraph)(gvizEmbeddedGraph *),
                        const gvizViewport viewport, size_t z) {
  gvizLayerInit((gvizLayer *)layer, viewport, &GVIZ_LAYER_GRAPH_VTABLE, z);
  layer->camera = gvizCameraMake2D((Vector2){0, 0},
                                   (Vector2){viewport.x + viewport.width * 0.5f,
                                             viewport.y + viewport.height * 0.5f},
                                   0.0f, 1.0f);
  layer->scene = NULL;
  layer->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  layer->graph = graph;
  layer->releaseGraph = releaseGraph;
  layer->onTopologyChanged = NULL;
  gvizGraphVBOInit(&layer->vbo);
  layer->vboMode = GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS;
  gvizGraphVBOSetMode(&layer->vbo, layer->vboMode);
  layer->gpuDirty = 2;
  layer->highlightDirty = 1;

  layer->vertexHighlight = NULL;
  layer->vertexHighlightBits = 0;
  layer->edgeHighlight = NULL;
  layer->edgeHighlightBits = 0;
  layer->edgeStartIdx = NULL;
  allocHighlightBuffers(layer);
}

void gvizLayerGraphSetVBOMode(gvizLayerGraph *layer, unsigned int mode) {
  layer->vboMode = mode;
  gvizGraphVBOSetMode(&layer->vbo, mode);
}

static void writeColorBuffer(gvizLayerGraph *self) {
  if (!self->graph || self->vbo.colorsCount == 0 || !self->vbo.colors) return;
  gvizGraph *g = self->graph->graph;
  size_t N = g->vertices.count;
  size_t fi = 0;
  for (size_t u = 0; u < N; u++) {
    gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, u);
    int uHi = (self->vertexHighlight && u < self->vertexHighlightBits)
                  ? (gvizTestBit(self->vertexHighlight, u) ? 1 : 0)
                  : 0;
    for (size_t j = 0; j < nbrs->count; j++) {
      size_t v = *(size_t *)gvizArrayAtIndex(nbrs, j);
      size_t bit = (self->edgeStartIdx ? self->edgeStartIdx[u] : 0) + j;
      int eHi = (self->edgeHighlight && bit < self->edgeHighlightBits)
                    ? (gvizTestBit(self->edgeHighlight, bit) ? 1 : 0)
                    : 0;
      int vHi = (self->vertexHighlight && v < self->vertexHighlightBits)
                    ? (gvizTestBit(self->vertexHighlight, v) ? 1 : 0)
                    : 0;
      float r1 = (uHi || eHi) ? 1.0f : 0.0f;
      float r2 = (vHi || eHi) ? 1.0f : 0.0f;
      if (fi + 6 > self->vbo.colorsCount) return;
      self->vbo.colors[fi++] = r1;
      self->vbo.colors[fi++] = 0.0f;
      self->vbo.colors[fi++] = 0.0f;
      self->vbo.colors[fi++] = r2;
      self->vbo.colors[fi++] = 0.0f;
      self->vbo.colors[fi++] = 0.0f;
    }
  }
  gvizGraphVBOUploadEndpointColors(&self->vbo, self->vbo.colors);
}

void gvizLayerGraphDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerGraph *self = (gvizLayerGraph *)layerV;
  if (!self->graph) return;

  int rebuilt = 0;
  if (self->gpuDirty >= 2 || !self->vbo.vaoId) {
    gvizGraphVBORebuild(&self->vbo, self->graph);
    rebuilt = 1;
  } else if (self->gpuDirty == 1)
    gvizGraphVBOUploadPositions(&self->vbo, self->graph);
  self->gpuDirty = 0;

  if (rebuilt || self->highlightDirty) {
    writeColorBuffer(self);
    self->highlightDirty = 0;
  }

  gvizGraphVBODraw(&self->vbo);
}

void gvizLayerGraphUpdate(void *layer, float dt) {
  (void)layer;
  (void)dt;
}

void gvizLayerGraphBindHandle(gvizLayerGraph *layer, gvizScene *scene,
                              gvizSceneGraphHandle h, gvizGraphCallback cb) {
  layer->scene = scene;
  layer->graphHandle = h;
  gvizSceneRetainGraph(scene, h);
  if (cb) gvizSceneSubscribeGraph(scene, h, layer, cb);
}

void gvizLayerGraphRelease(void *layer) {
  gvizLayerGraph *self = (gvizLayerGraph *)layer;
  gvizGraphVBORelease(&self->vbo);
  if (self->scene && self->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizSceneUnsubscribeGraph(self->scene, self->graphHandle, self);
    gvizSceneReleaseGraph(self->scene, self->graphHandle);
    self->scene = NULL;
    self->graphHandle = GVIZ_SCENE_GRAPH_INVALID;
  }
  if (self->releaseGraph && self->graph)
    self->releaseGraph(self->graph);
  self->graph = NULL;

  if (self->vertexHighlight) { GVIZ_DEALLOC(self->vertexHighlight); self->vertexHighlight = NULL; }
  if (self->edgeHighlight)   { GVIZ_DEALLOC(self->edgeHighlight);   self->edgeHighlight = NULL; }
  if (self->edgeStartIdx)    { GVIZ_DEALLOC(self->edgeStartIdx);    self->edgeStartIdx = NULL; }
  self->vertexHighlightBits = 0;
  self->edgeHighlightBits = 0;
}

int gvizLayerGraphHandleEvent(void *layer, const gvizEvent *event) {
  (void)layer;
  (void)event;
  return 0;
}

struct gvizCamera *gvizLayerGraphGetCamera(void *layer) {
  return &((gvizLayerGraph *)layer)->camera;
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

/* ---- Highlight API ------------------------------------------------------- */

void gvizLayerGraphSetVertexHighlight(gvizLayerGraph *layer, size_t v, int on) {
  if (!layer->vertexHighlight || v >= layer->vertexHighlightBits) return;
  if (on) gvizSetBit(layer->vertexHighlight, v);
  else    gvizClearBit(layer->vertexHighlight, v);
  layer->highlightDirty = 1;
}

void gvizLayerGraphClearVertexHighlights(gvizLayerGraph *layer) {
  if (!layer->vertexHighlight) return;
  size_t units = GVIZ_ARRAY_UNITS(layer->vertexHighlightBits ? layer->vertexHighlightBits : 1);
  memset(layer->vertexHighlight, 0, units * sizeof(GVIZ_BIT_UNIT));
  layer->highlightDirty = 1;
}

static int findEdgeBit(gvizLayerGraph *layer, size_t u, size_t v, size_t *out) {
  if (!layer->graph || !layer->edgeStartIdx) return -1;
  gvizGraph *g = layer->graph->graph;
  if (u >= g->vertices.count) return -1;
  gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, u);
  for (size_t j = 0; j < nbrs->count; j++) {
    if (*(size_t *)gvizArrayAtIndex(nbrs, j) == v) {
      *out = layer->edgeStartIdx[u] + j;
      return 0;
    }
  }
  return -1;
}

void gvizLayerGraphSetEdgeHighlight(gvizLayerGraph *layer, size_t u, size_t v,
                                    int on) {
  if (!layer->graph || !layer->edgeHighlight) return;
  size_t bit;
  if (findEdgeBit(layer, u, v, &bit) == 0 && bit < layer->edgeHighlightBits) {
    if (on) gvizSetBit(layer->edgeHighlight, bit);
    else    gvizClearBit(layer->edgeHighlight, bit);
    layer->highlightDirty = 1;
  }
  if (!layer->graph->graph->directed) {
    if (findEdgeBit(layer, v, u, &bit) == 0 && bit < layer->edgeHighlightBits) {
      if (on) gvizSetBit(layer->edgeHighlight, bit);
      else    gvizClearBit(layer->edgeHighlight, bit);
      layer->highlightDirty = 1;
    }
  }
}

void gvizLayerGraphClearEdgeHighlights(gvizLayerGraph *layer) {
  if (!layer->edgeHighlight) return;
  size_t units = GVIZ_ARRAY_UNITS(layer->edgeHighlightBits ? layer->edgeHighlightBits : 1);
  memset(layer->edgeHighlight, 0, units * sizeof(GVIZ_BIT_UNIT));
  layer->highlightDirty = 1;
}

void gvizLayerGraphRebuildEdgeIndex(gvizLayerGraph *layer) {
  if (!layer->graph) return;
  gvizGraph *g = layer->graph->graph;
  size_t N = g->vertices.count;

  size_t *newStart = (size_t *)GVIZ_REALLOC(layer->edgeStartIdx, (N + 1) * sizeof(size_t));
  if (!newStart) return;
  layer->edgeStartIdx = newStart;
  size_t total = computeEdgeStartIdx(g, layer->edgeStartIdx);

  size_t units = GVIZ_ARRAY_UNITS(total ? total : 1);
  GVIZ_BIT_UNIT *neb = (GVIZ_BIT_UNIT *)GVIZ_REALLOC(layer->edgeHighlight,
                                                     units * sizeof(GVIZ_BIT_UNIT));
  if (neb) {
    layer->edgeHighlight = neb;
    memset(layer->edgeHighlight, 0, units * sizeof(GVIZ_BIT_UNIT));
  }
  layer->edgeHighlightBits = total;

  size_t vunits = GVIZ_ARRAY_UNITS(N ? N : 1);
  GVIZ_BIT_UNIT *nvb = (GVIZ_BIT_UNIT *)GVIZ_REALLOC(layer->vertexHighlight,
                                                     vunits * sizeof(GVIZ_BIT_UNIT));
  if (nvb) {
    layer->vertexHighlight = nvb;
    memset(layer->vertexHighlight, 0, vunits * sizeof(GVIZ_BIT_UNIT));
  }
  layer->vertexHighlightBits = N;
  layer->highlightDirty = 1;
}

/* ---- Dynamic mutations --------------------------------------------------- */

static int growPositions(gvizLayerGraph *layer, size_t newCount) {
  gvizEmbeddedGraph *eg = layer->graph;
  if (!eg) return -1;
  double *grown = (double *)GVIZ_REALLOC(eg->embedding.vertexPositions,
                                         newCount * eg->embedding.dim * sizeof(double));
  if (!grown) return -1;
  eg->embedding.vertexPositions = grown;
  return 0;
}

int gvizLayerGraphAddVertex(gvizLayerGraph *layer, const double *startPos) {
  if (!layer->graph) return -1;
  gvizEmbeddedGraph *eg = layer->graph;
  gvizGraph *g = eg->graph;
  size_t newIdx = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) != 0) return -1;
  if (growPositions(layer, newIdx + 1) != 0) return -1;
  size_t dim = eg->embedding.dim;
  for (size_t d = 0; d < dim; d++)
    eg->embedding.vertexPositions[newIdx * dim + d] = startPos ? startPos[d] : 0.0;

  gvizLayerGraphRebuildEdgeIndex(layer);
  layer->gpuDirty = 2;
  if (layer->onTopologyChanged) layer->onTopologyChanged(eg);
  if (layer->scene && layer->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphVertexEvent ev = {newIdx};
    gvizSceneNotifyGraphChanged(layer->scene, layer->graphHandle, layer,
                                GVIZ_GRAPH_VERTEX_ADDED, &ev);
  }
  return 0;
}

int gvizLayerGraphRemoveVertex(gvizLayerGraph *layer, size_t v) {
  if (!layer->graph) return -1;
  gvizEmbeddedGraph *eg = layer->graph;
  gvizGraph *g = eg->graph;
  if (v >= g->vertices.count) return -1;

  size_t N = g->vertices.count;
  size_t dim = eg->embedding.dim;
  if (eg->embedding.vertexPositions && v + 1 < N) {
    memmove(&eg->embedding.vertexPositions[v * dim],
            &eg->embedding.vertexPositions[(v + 1) * dim],
            (N - v - 1) * dim * sizeof(double));
  }

  if (gvizGraphRemoveVertex(g, v) != 0) return -1;

  gvizLayerGraphRebuildEdgeIndex(layer);
  layer->gpuDirty = 2;
  if (layer->onTopologyChanged) layer->onTopologyChanged(eg);
  if (layer->scene && layer->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphVertexEvent ev = {v};
    gvizSceneNotifyGraphChanged(layer->scene, layer->graphHandle, layer,
                                GVIZ_GRAPH_VERTEX_REMOVED, &ev);
  }
  return 0;
}

int gvizLayerGraphAddEdge(gvizLayerGraph *layer, size_t u, size_t v) {
  if (!layer->graph) return -1;
  if (gvizGraphAddEdge(layer->graph->graph, u, v) != 0) return -1;
  gvizLayerGraphRebuildEdgeIndex(layer);
  layer->gpuDirty = 2;
  if (layer->onTopologyChanged) layer->onTopologyChanged(layer->graph);
  if (layer->scene && layer->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphEdgeEvent ev = {u, v};
    gvizSceneNotifyGraphChanged(layer->scene, layer->graphHandle, layer,
                                GVIZ_GRAPH_EDGE_ADDED, &ev);
  }
  return 0;
}

int gvizLayerGraphRemoveEdge(gvizLayerGraph *layer, size_t u, size_t v) {
  if (!layer->graph) return -1;
  if (gvizGraphRemoveEdge(layer->graph->graph, u, v) != 0) return -1;
  gvizLayerGraphRebuildEdgeIndex(layer);
  layer->gpuDirty = 2;
  if (layer->onTopologyChanged) layer->onTopologyChanged(layer->graph);
  if (layer->scene && layer->graphHandle != GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphEdgeEvent ev = {u, v};
    gvizSceneNotifyGraphChanged(layer->scene, layer->graphHandle, layer,
                                GVIZ_GRAPH_EDGE_REMOVED, &ev);
  }
  return 0;
}
