#include "renderer/layers/gvizLayerGraph.h"
#include "raylib.h"
#include "raymath.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include <string.h>

#define VERTEXRADIUS 7.0
#define VERTEXTHICKNESS 3.0
#define EDGETHICKNESS 2.0
#define MAXRADIUS 250

void gvizLayerGraphInit(gvizLayerGraph *layer, gvizEmbeddedGraph *graph,
                        const gvizViewport viewport, size_t z) {
  gvizLayerInit((gvizLayer *)layer, viewport, &GVIZ_LAYER_GRAPH_VTABLE, z);
  layer->graph = graph;
}

// TODO: split vertex raius clamping out of this function
float getEffectiveDist(float baseRadius, const Camera2D *camera) {
  float r = (baseRadius) / camera->zoom;
  float max = MAXRADIUS - EDGETHICKNESS / camera->zoom;
  return r > max ? max : r;
}

void drawEdge(Vector2 from, Vector2 to, int directed, unsigned char opacity,
              const Camera2D *camera) {

  Vector2 tmp = Vector2Subtract(to, from);
  float r = getEffectiveDist(VERTEXRADIUS + VERTEXTHICKNESS, camera);
  Vector2 delta1 = {getEffectiveDist(-7.5, camera),
                    getEffectiveDist(5.0, camera)};
  Vector2 delta2 = {getEffectiveDist(-7.5, camera),
                    getEffectiveDist(-5.0, camera)};
  float theta = atan2(tmp.y, tmp.x);
  delta1 = Vector2Rotate(delta1, theta);
  delta2 = Vector2Rotate(delta2, theta);
  float dist = Vector2Length(tmp);
  float tBegin = r / dist;
  float tEnd = 1 - r / dist;
  float tArrow = tEnd - 10.0 / dist;

  Vector2 v0 = Vector2Lerp(from, to, tEnd - 7.5 / dist);
  Vector2 v1 = Vector2Lerp(from, to, tEnd);

  DrawLineEx(Vector2Lerp(from, to, tBegin), directed ? v0 : v1,
             EDGETHICKNESS / camera->zoom,
             (Color){0x00, 0x00, 0x00, opacity}); // Draw a line

  if (directed)
    DrawTriangle(v1, Vector2Add(v1, delta2), Vector2Add(v1, delta1),
                 (Color){0x00, 0x00, 0x00, opacity}); // Draw Arrow
}

void gvizLayerGraphDraw(void *layer, const Camera2D *camera) {
  // gvizEmbeddedGraphDraw(((gvizLayerGraph *)layer)->graph);
  //
  gvizEmbeddedGraph *embedding = ((gvizLayerGraph *)layer)->graph;
  for (size_t i = 0; i < embedding->graph->vertices.count; i++) {
    double *position = gvizEmbeddedGraphGetVPosition(embedding, i);

    DrawRing((Vector2){position[0], position[1]},
             getEffectiveDist(VERTEXRADIUS, camera),
             getEffectiveDist(VERTEXRADIUS, camera) +
                 getEffectiveDist(VERTEXTHICKNESS, camera),
             0, 360, 1, (Color){0x00, 0x00, 0x00, 0xFF});
    gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, i);
    for (size_t j = 0; j < children->count; j++) {
      double *otherPosition = gvizEmbeddedGraphGetVPosition(
          embedding, *(size_t *)gvizArrayAtIndex(children, j));
      drawEdge((Vector2){position[0], position[1]},
               (Vector2){otherPosition[0], otherPosition[1]},
               embedding->graph->directed, 0xFF, camera);
    }
  }
}

void gvizLayerGraphUpdate(void *layer, float dt) {}
void gvizLayerGraphRelease(void *layer) {}
int gvizLayerGraphHandleEvent(void *layer, const gvizEvent *event) { return 0; }
