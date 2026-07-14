#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizPlanarEmbedder.h"
#include "core/gvizVec.h"
#include "core/alloc.h"
#include "ds/gvizGraph.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void drawMaskDefaults(gvizDrawMask *mask) {
  mask->visibleVertices = NULL;
  mask->edgePolicy = GVIZ_DRAW_EDGES_ALL;
  mask->revision = 0;
}

static void drawMaskShowAllSubgraphVertices(gvizEmbeddedGraph *embedding) {
  size_t u;
  gvizSubgraphVertexIterator it =
      gvizSubgraphVertexIteratorCreate(&embedding->subgraph);
  while (gvizSubgraphVertexIterate(&it, &u))
    gvizVertexSubsetShowVertex(embedding->drawMask.visibleVertices, u);
}

int gvizEmbeddedGraphInit(gvizEmbeddedGraph *embedding, gvizSubgraph subgraph,
                          size_t n) {
  embedding->subgraph = subgraph;
  embedding->planarEmbedded = 0;
  embedding->highlight = (gvizSubgraph){0};
  const gvizGraph *graph = subgraph.g;
  embedding->embedding.dim = n;
  embedding->actions = (gvizActionRegistry){0};
  embedding->stats = (gvizStatRegistry){0};
  drawMaskDefaults(&embedding->drawMask);
  embedding->drawMask.visibleVertices = gvizVertexSubsetCreateEmpty(graph);
  if (!embedding->drawMask.visibleVertices)
    return -1;
  drawMaskShowAllSubgraphVertices(embedding);
  embedding->embedding.vertexPositions =
      GVIZ_ALLOC(sizeof(double) * gvizGraphSize(graph) * n);
  if (!embedding->embedding.vertexPositions)
    return -1;
  memset(embedding->embedding.vertexPositions, 0,
         sizeof(double) * gvizGraphSize(graph) * n);

  return 0;
}

void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding) {
  gvizEmbeddedGraphClearHighlight(embedding);
  gvizSubgraphRelease(&embedding->subgraph);
  if (embedding->embedding.vertexPositions) {
    GVIZ_DEALLOC(embedding->embedding.vertexPositions);
    embedding->embedding.vertexPositions = NULL;
  }
  if (embedding->actions.actions) {
    GVIZ_DEALLOC(embedding->actions.actions);
  }
  embedding->actions = (gvizActionRegistry){0};
  for (size_t i = 0; i < embedding->stats.count; i++) {
    if (embedding->stats.series[i].samples)
      GVIZ_DEALLOC(embedding->stats.series[i].samples);
  }
  if (embedding->stats.series)
    GVIZ_DEALLOC(embedding->stats.series);
  embedding->stats = (gvizStatRegistry){0};
  if (embedding->drawMask.visibleVertices)
    gvizVertexSubsetRelease(embedding->drawMask.visibleVertices);
  drawMaskDefaults(&embedding->drawMask);
}

void gvizEmbeddedGraphSetDrawMaskEdgePolicy(gvizEmbeddedGraph *embedding,
                                            gvizDrawEdgePolicy edgePolicy) {
  embedding->drawMask.edgePolicy = edgePolicy;
  embedding->drawMask.revision++;
}

void gvizEmbeddedGraphDrawMaskShowVertex(gvizEmbeddedGraph *embedding,
                                         size_t u) {
  gvizVertexSubsetShowVertex(embedding->drawMask.visibleVertices, u);
}

void gvizEmbeddedGraphDrawMaskHideVertex(gvizEmbeddedGraph *embedding,
                                         size_t u) {
  gvizVertexSubsetHideVertex(embedding->drawMask.visibleVertices, u);
}

void gvizEmbeddedGraphDrawMaskClearVertices(gvizEmbeddedGraph *embedding) {
  gvizVertexSubsetClearAll(embedding->drawMask.visibleVertices,
                           gvizGraphSize(embedding->subgraph.g));
}

void gvizEmbeddedGraphDrawMaskNotifyChanged(gvizEmbeddedGraph *embedding) {
  embedding->drawMask.revision++;
}

void gvizEmbeddedGraphResetDrawMask(gvizEmbeddedGraph *embedding) {
  embedding->drawMask.edgePolicy = GVIZ_DRAW_EDGES_ALL;
  drawMaskShowAllSubgraphVertices(embedding);
  embedding->drawMask.revision++;
}

const gvizDrawMask *
gvizEmbeddedGraphGetDrawMask(const gvizEmbeddedGraph *embedding) {
  return &embedding->drawMask;
}

uint64_t gvizEmbeddedGraphDrawMaskRevision(const gvizEmbeddedGraph *embedding) {
  return embedding->drawMask.revision;
}

bool gvizEmbeddedGraphIsVertexVisible(const gvizEmbeddedGraph *embedding,
                                      size_t u) {
  const gvizSubgraph *sg = &embedding->subgraph;
  if (!gvizSubgraphHasVertex(sg, u))
    return false;
  return gvizVertexSubsetTest(embedding->drawMask.visibleVertices, u);
}

bool gvizEmbeddedGraphIsEdgeVisible(const gvizEmbeddedGraph *embedding,
                                    size_t u, size_t v) {
  switch (embedding->drawMask.edgePolicy) {
  case GVIZ_DRAW_EDGES_NONE:
    return false;
  case GVIZ_DRAW_EDGES_IF_BOTH_VISIBLE:
    return gvizEmbeddedGraphIsVertexVisible(embedding, u) &&
           gvizEmbeddedGraphIsVertexVisible(embedding, v);
  case GVIZ_DRAW_EDGES_ALL:
  default:
    return gvizSubgraphHasEdge(&embedding->subgraph, u, v);
  }
}

size_t gvizEmbeddedGraphDim(const gvizEmbeddedGraph *embedding) {
  return embedding->embedding.dim;
}

size_t gvizEmbeddedGraphPositionCount(const gvizEmbeddedGraph *embedding) {
  return gvizGraphSize(embedding->subgraph.g);
}

const double *gvizEmbeddedGraphPositions(const gvizEmbeddedGraph *embedding) {
  return embedding->embedding.vertexPositions;
}

const gvizSubgraph *
gvizEmbeddedGraphStructure(const gvizEmbeddedGraph *embedding) {
  return &embedding->subgraph;
}

static gvizAction *findActionMutable(const gvizEmbeddedGraph *embedding,
                                     const char *name) {
  for (size_t i = 0; i < embedding->actions.count; i++) {
    if (strcmp(embedding->actions.actions[i].name, name) == 0)
      return &embedding->actions.actions[i];
  }
  return NULL;
}

int gvizEmbeddedGraphAddAction(gvizEmbeddedGraph *embedding, const char *name,
                               gvizActionHandler handler, void *userData) {
  if (!embedding || !name || !handler)
    return -1;

  gvizAction *existing = findActionMutable(embedding, name);
  if (existing) {
    existing->handler = handler;
    existing->userData = userData;
    return 0;
  }

  gvizActionRegistry *reg = &embedding->actions;
  if (reg->count == reg->capacity) {
    size_t newCapacity = reg->capacity ? reg->capacity * 2 : 4;
    gvizAction *grown =
        GVIZ_REALLOC(reg->actions, newCapacity * sizeof(gvizAction));
    if (!grown)
      return -1;
    reg->actions = grown;
    reg->capacity = newCapacity;
  }

  reg->actions[reg->count++] =
      (gvizAction){.name = name, .handler = handler, .userData = userData};
  return 0;
}

int gvizEmbeddedGraphRemoveAction(gvizEmbeddedGraph *embedding,
                                  const char *name) {
  if (!embedding || !name)
    return -1;

  gvizAction *found = findActionMutable(embedding, name);
  if (!found)
    return -1;

  gvizActionRegistry *reg = &embedding->actions;
  *found = reg->actions[--reg->count];
  return 0;
}

const gvizAction *
gvizEmbeddedGraphFindAction(const gvizEmbeddedGraph *embedding,
                            const char *name) {
  if (!embedding || !name)
    return NULL;
  return findActionMutable(embedding, name);
}

size_t gvizEmbeddedGraphActionCount(const gvizEmbeddedGraph *embedding) {
  return embedding->actions.count;
}

const gvizAction *gvizEmbeddedGraphActionAt(const gvizEmbeddedGraph *embedding,
                                            size_t idx) {
  if (idx >= embedding->actions.count)
    return NULL;
  return &embedding->actions.actions[idx];
}

int gvizEmbeddedGraphInvokeAction(gvizEmbeddedGraph *embedding,
                                  const char *name,
                                  const gvizActionPayload *payload) {
  const gvizAction *action = gvizEmbeddedGraphFindAction(embedding, name);
  if (!action)
    return -1;

  gvizActionPayload zeroed = {0};
  action->handler(embedding, action->userData, payload ? payload : &zeroed);
  return 0;
}

static gvizStatSeries *findStatSeriesMutable(const gvizEmbeddedGraph *embedding,
                                             const char *name) {
  for (size_t i = 0; i < embedding->stats.count; i++) {
    if (strcmp(embedding->stats.series[i].name, name) == 0)
      return &embedding->stats.series[i];
  }
  return NULL;
}

gvizStatSeries *gvizEmbeddedGraphAddStatSeries(gvizEmbeddedGraph *embedding,
                                               const char *name,
                                               gvizStatChartKind kind) {
  if (!embedding || !name)
    return NULL;

  gvizStatSeries *existing = findStatSeriesMutable(embedding, name);
  if (existing) {
    existing->kind = kind;
    return existing;
  }

  gvizStatRegistry *reg = &embedding->stats;
  if (reg->count == reg->capacity) {
    size_t newCapacity = reg->capacity ? reg->capacity * 2 : 4;
    gvizStatSeries *grown =
        GVIZ_REALLOC(reg->series, newCapacity * sizeof(gvizStatSeries));
    if (!grown)
      return NULL;
    reg->series = grown;
    reg->capacity = newCapacity;
  }

  gvizStatSeries *series = &reg->series[reg->count++];
  *series = (gvizStatSeries){.name = name, .kind = kind};
  return series;
}

int gvizEmbeddedGraphStatAppend(gvizEmbeddedGraph *embedding, const char *name,
                                double value) {
  if (!embedding || !name)
    return -1;

  gvizStatSeries *series = findStatSeriesMutable(embedding, name);
  if (!series) {
    series = gvizEmbeddedGraphAddStatSeries(embedding, name,
                                            GVIZ_STAT_CHART_LINE);
    if (!series)
      return -1;
  }

  if (series->count == series->capacity) {
    size_t newCapacity = series->capacity ? series->capacity * 2 : 64;
    double *grown = GVIZ_REALLOC(series->samples, newCapacity * sizeof(double));
    if (!grown)
      return -1;
    series->samples = grown;
    series->capacity = newCapacity;
  }

  series->samples[series->count++] = value;
  series->revision++;
  return 0;
}

void gvizEmbeddedGraphStatClear(gvizEmbeddedGraph *embedding,
                                const char *name) {
  if (!embedding || !name)
    return;
  gvizStatSeries *series = findStatSeriesMutable(embedding, name);
  if (!series)
    return;
  series->count = 0;
  series->revision++;
}

size_t gvizEmbeddedGraphStatSeriesCount(const gvizEmbeddedGraph *embedding) {
  return embedding->stats.count;
}

const gvizStatSeries *
gvizEmbeddedGraphStatSeriesAt(const gvizEmbeddedGraph *embedding, size_t idx) {
  if (idx >= embedding->stats.count)
    return NULL;
  return &embedding->stats.series[idx];
}

const gvizStatSeries *
gvizEmbeddedGraphFindStatSeries(const gvizEmbeddedGraph *embedding,
                                const char *name) {
  if (!embedding || !name)
    return NULL;
  return findStatSeriesMutable(embedding, name);
}

double *gvizEmbeddedGraphGetVPosition(gvizEmbeddedGraph *embedding,
                                      size_t idx) {
  return embedding->embedding.vertexPositions + idx * embedding->embedding.dim;
}

void gvizEmbeddedGraphSetVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position) {
  gvizVecCopy(embedding->embedding.dim, position,
              gvizEmbeddedGraphGetVPosition(embedding, idx));
}

void gvizEmbeddedGraphAddVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position) {
  gvizVecAxpy(embedding->embedding.dim, 1.0, position,
              gvizEmbeddedGraphGetVPosition(embedding, idx));
}

int gvizEmbeddedGraphSaveEmbedding(gvizEmbeddedGraph *embedding,
                                   const char *name, const char *filename) {
  FILE *f = fopen(filename, "w");
  if (!f)
    return -1;

  fprintf(f, "%s\n", name);

  size_t nvertices = gvizGraphSize(embedding->subgraph.g);
  fprintf(f, "%zu %zu\n", nvertices, embedding->embedding.dim);
  for (size_t i = 0; i < nvertices; i++) {
    double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
    for (size_t j = 0; j < embedding->embedding.dim; j++) {
      fprintf(f, "%f ", pos[j]);
    }
    fprintf(f, "\n");
  }

  fclose(f);
  return 0;
}

int gvizEmbeddedGraphLoadEmbedding(gvizEmbeddedGraph *embedding,
                                   const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return -1;

  char name[256];
  fgets(name, 256, f);

  printf("loading embedding: %s\n", name);

  size_t vertexCount, dim;
  fscanf(f, "%zu %zu", &vertexCount, &dim);
  if (vertexCount != gvizGraphSize(embedding->subgraph.g) ||
      dim != embedding->embedding.dim) {
    fclose(f);
    return -1;
  }

  for (size_t i = 0; i < vertexCount; i++) {
    double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
    for (size_t j = 0; j < dim; j++) {
      fscanf(f, "%lf", &pos[j]);
    }
  }

  fclose(f);
  return 0;
}

static int pointInPolygon(const gvizEmbeddedGraph *embedding,
                          const gvizArray *face, double x, double y) {
  size_t n = face->count;
  if (n < 3)
    return 0;

  int inside = 0;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    size_t vi = *(size_t *)gvizArrayAtIndex(face, i);
    size_t vj = *(size_t *)gvizArrayAtIndex(face, j);
    double *pi =
        gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)embedding, vi);
    double *pj =
        gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)embedding, vj);

    int intersect =
        ((pi[1] > y) != (pj[1] > y)) &&
        (x < (pj[0] - pi[0]) * (y - pi[1]) / (pj[1] - pi[1] + 0.0) + pi[0]);
    if (intersect)
      inside = !inside;
  }
  return inside;
}

static double polygonSignedArea(const gvizEmbeddedGraph *embedding,
                                const gvizArray *face) {
  double area = 0.0;
  size_t n = face->count;
  for (size_t i = 0; i < n; i++) {
    size_t u = *(size_t *)gvizArrayAtIndex(face, i);
    size_t v = *(size_t *)gvizArrayAtIndex(face, (i + 1) % n);
    double *pu = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)embedding, u);
    double *pv = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)embedding, v);
    area += pu[0] * pv[1] - pv[0] * pu[1];
  }
  return area * 0.5;
}

static int faceToSubgraph(const gvizGraph *g, const gvizArray *face,
                          gvizSubgraph *out) {
  if (!g->layout)
    gvizGraphBuildLayout((gvizGraph *)g);

  *out = gvizSubgraphCreateEmpty(g);
  if (!out->vs)
    return -1;

  for (size_t i = 0; i < face->count; i++) {
    size_t u = *(size_t *)gvizArrayAtIndex(face, i);
    gvizSubgraphShowVertex(out, u);
  }

  for (size_t i = 0; i < face->count; i++) {
    size_t u = *(size_t *)gvizArrayAtIndex(face, i);
    size_t v = *(size_t *)gvizArrayAtIndex(face, (i + 1) % face->count);
    gvizSubgraphShowEdge(out, u, v);
  }

  return 0;
}

void gvizEmbeddedGraphFaceSearchRelease(gvizFaceSearchState *state) {
  if (!state)
    return;
  for (size_t i = 0; i < state->faces.count; i++)
    gvizArrayRelease(gvizArrayAtIndex(&state->faces, i));
  gvizArrayRelease(&state->faces);
  memset(state, 0, sizeof(*state));
}

int gvizEmbeddedGraphFaceSearchInit(gvizEmbeddedGraph *embedding,
                                    gvizFaceSearchState *state) {
  if (!embedding || !state || !embedding->planarEmbedded)
    return -1;

  memset(state, 0, sizeof(*state));
  state->embedding = embedding;

  gvizPlanarEmbedderState planar = {.embedding = *embedding};
  gvizFaceIteratorContext ctx;
  if (gvizFaceIteratorInit(&planar, &ctx) < 0)
    return -1;

  if (gvizPlanarEmbedderFaces(&planar, &ctx) < 0) {
    gvizFaceIteratorRelease(&ctx);
    return -1;
  }

  state->faces = ctx.faces;
  ctx.faces = (gvizArray){0};
  gvizFaceIteratorRelease(&ctx);
  return 0;
}

int gvizEmbeddedGraphNextFace(gvizEmbeddedGraph *embedding,
                              gvizFaceSearchState *state) {
  if (!embedding || !state)
    return -1;

  if (state->nextFace >= state->faces.count)
    return 1;

  state->nextFace++;
  return 0;
}

int gvizEmbeddedGraphApplyPlanarEmbedding(gvizEmbeddedGraph *embedding) {
  if (!embedding)
    return -1;

  int res = gvizSubgraphApplyPlanarRotation(&embedding->subgraph, NULL);
  if (res == 0)
    embedding->planarEmbedded = 1;
  return res;
}

int gvizEmbeddedGraphIsPlanarEmbedded(const gvizEmbeddedGraph *embedding) {
  return embedding && embedding->planarEmbedded;
}

int gvizEmbeddedGraphFaceSubgraphAt(gvizEmbeddedGraph *embedding, double worldX,
                                    double worldY, gvizSubgraph *out) {
  if (!embedding || !out || !embedding->planarEmbedded)
    return -1;

  gvizPlanarEmbedderState planar = {.embedding = *embedding};
  gvizFaceIteratorContext ctx;
  if (gvizFaceIteratorInit(&planar, &ctx) < 0)
    return -1;

  if (gvizPlanarEmbedderFaces(&planar, &ctx) < 0) {
    gvizFaceIteratorRelease(&ctx);
    return -1;
  }

  double minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
  size_t u;
  gvizSubgraphVertexIterator vit =
      gvizSubgraphVertexIteratorCreate(&embedding->subgraph);
  while (gvizSubgraphVertexIterate(&vit, &u)) {
    double *p = gvizEmbeddedGraphGetVPosition(embedding, u);
    if (p[0] < minX)
      minX = p[0];
    if (p[1] < minY)
      minY = p[1];
    if (p[0] > maxX)
      maxX = p[0];
    if (p[1] > maxY)
      maxY = p[1];
  }

  int pickLargest = (worldX < minX || worldX > maxX || worldY < minY || worldY > maxY);

  size_t bestIdx = (size_t)-1;
  double bestMetric = pickLargest ? -1.0 : INFINITY;

  for (size_t i = 0; i < ctx.faces.count; i++) {
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&ctx.faces, i);
    if (face->count < 3)
      continue;

    if (!pickLargest && !pointInPolygon(embedding, face, worldX, worldY))
      continue;

    double area = fabs(polygonSignedArea(embedding, face));
    if (pickLargest) {
      if (area > bestMetric) {
        bestMetric = area;
        bestIdx = i;
      }
    } else if (area < bestMetric) {
      bestMetric = area;
      bestIdx = i;
    }
  }

  if (bestIdx == (size_t)-1) {
    gvizFaceIteratorRelease(&ctx);
    return -1;
  }

  gvizArray *face = (gvizArray *)gvizArrayAtIndex(&ctx.faces, bestIdx);
  int res = faceToSubgraph(embedding->subgraph.g, face, out);
  gvizFaceIteratorRelease(&ctx);
  return res;
}

void gvizEmbeddedGraphSetHighlight(gvizEmbeddedGraph *embedding,
                                   gvizSubgraph highlight) {
  if (!embedding)
    return;
  gvizEmbeddedGraphClearHighlight(embedding);
  embedding->highlight = highlight;
}

void gvizEmbeddedGraphClearHighlight(gvizEmbeddedGraph *embedding) {
  if (!embedding)
    return;
  if (embedding->highlight.vs || embedding->highlight.es.bitset)
    gvizSubgraphRelease(&embedding->highlight);
  embedding->highlight = (gvizSubgraph){0};
}

int gvizEmbeddedGraphHasHighlight(const gvizEmbeddedGraph *embedding) {
  return embedding && embedding->highlight.vs != NULL;
}

const gvizSubgraph *
gvizEmbeddedGraphGetHighlight(const gvizEmbeddedGraph *embedding) {
  if (!embedding || !gvizEmbeddedGraphHasHighlight(embedding))
    return NULL;
  return &embedding->highlight;
}
