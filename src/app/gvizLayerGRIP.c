#include "app/gvizLayerGRIP.h"
#include "cblas.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizVertexDiscVBO.h"
#include <stdlib.h>
#include <string.h>

#define GVIZ_LAYER_GRIP_ROUNDS 36
#define GVIZ_LAYER_GRIP_SIMPLEX_SIDE 10000.0
#define GVIZ_LAYER_GRIP_VERTEX_RADIUS 2.0f

static void applyRadiiMask(gvizLayerGRIP *self) {
  if (!self->vbo.radii)
    return;
  size_t N = self->graph.vertices.count;
  if (self->vbo.radiiCount < N)
    N = self->vbo.radiiCount;
  for (size_t i = 0; i < N; i++) {
    self->vbo.radii[i] =
        gvizTestBit(self->placedBits, i) ? GVIZ_LAYER_GRIP_VERTEX_RADIUS : 0.0f;
  }
  if (self->vbo.discs.vaoId)
    gvizVertexDiscVBOUploadRadii(&self->vbo.discs, self->vbo.radii, N);
}

int gvizLayerGRIPInit(gvizLayerGRIP *self, gvizGraph *graph, size_t diameter,
                      size_t z) {
  gvizLayerInit((gvizLayer *)self, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_GRIP_VTABLE, z);
  self->camera = gvizCameraMake2D((Vector2){0, 0}, (Vector2){0, 0}, 0.0f, 1.0f);
  self->layer.flags = GVIZ_LAYER_VISIBLE;
  self->placedBits = NULL;
  self->layerKNNs = NULL;
  self->layerCount = 0;
  self->currentLayer = -1;
  self->currentRound = 0;
  self->gpuDirty = 2;

  if (gvizGraphClone(&self->graph, graph) != 0)
    return -1;

  if (gvizGRIPEmbeddingInit(&self->grip, &self->graph, diameter, 2) != 0) {
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

  /* Place coarsest layer on a regular simplex. */
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->grip;
  size_t dim = eg->embedding.dim;
  double simplex[dim * (dim + 1)];
  makeRegularSimplex(dim, GVIZ_LAYER_GRIP_SIMPLEX_SIDE, simplex);
  for (size_t j = 0; j < dim + 1; j++) {
    size_t v = self->grip.misFiltration[j];
    gvizSetBit(self->placedBits, v);
    cblas_dcopy(dim, simplex + j * dim, 1, gvizEmbeddedGraphGetVPosition(eg, v),
                1);
  }

  self->currentLayer = (int)self->layerCount - 1;
  self->currentRound = 0;
  self->layerKNNs = gvizGRIPPrepareLayerKNNs(
      &self->grip, (size_t)self->currentLayer, self->placedBits);

  gvizGraphVBOInit(&self->vbo);
  self->vbo.discFill = 1.0f;
  gvizGraphVBOSetMode(&self->vbo, GVIZ_GRAPH_VBO_DISCS);
  self->gpuDirty = 2;
  return 0;
}

void gvizLayerGRIPUpdate(void *layerV, float dt) {
  (void)dt;
  gvizLayerGRIP *self = (gvizLayerGRIP *)layerV;

  if (self->currentLayer < 0)
    return;

  gvizGRIPRefineOneRound(&self->grip, (size_t)self->currentLayer,
                         self->layerKNNs);
  if (self->gpuDirty < 1)
    self->gpuDirty = 1;
  self->currentRound++;

  if (self->currentRound < GVIZ_LAYER_GRIP_ROUNDS)
    return;

  /* Layer done — tear down its KNNs and advance. */
  gvizGRIPReleaseLayerKNNs(&self->grip, (size_t)self->currentLayer,
                           self->layerKNNs);
  self->layerKNNs = NULL;
  self->currentLayer--;

  if (self->currentLayer < 0) {
    /* All layers placed — turn on edge rendering. */
    gvizGraphVBOSetMode(&self->vbo,
                        GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS);
    self->gpuDirty = 2;
    return;
  }

  placeLayerVertices(&self->grip, (size_t)self->currentLayer, self->placedBits);
  self->layerKNNs = gvizGRIPPrepareLayerKNNs(
      &self->grip, (size_t)self->currentLayer, self->placedBits);
  self->currentRound = 0;
  self->gpuDirty = 2;
}

void gvizLayerGRIPDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerGRIP *self = (gvizLayerGRIP *)layerV;

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
}

struct gvizCamera *gvizLayerGRIPGetCamera(void *layer) {
  return &((gvizLayerGRIP *)layer)->camera;
}

void gvizLayerGRIPRelease(void *layerV) {
  gvizLayerGRIP *self = (gvizLayerGRIP *)layerV;
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
