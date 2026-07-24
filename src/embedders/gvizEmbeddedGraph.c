#include "embedders/gvizEmbeddedGraph.h"
#include "core/gvizVec.h"
#include "core/alloc.h"
#include "ds/gvizGraph.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  if (!embedding->embedding.vertexPositions) {
    gvizVertexSubsetRelease(embedding->drawMask.visibleVertices);
    embedding->drawMask.visibleVertices = NULL;
    return -1;
  }
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

int gvizEmbeddedGraphAddVertex(gvizEmbeddedGraph *embedding, void *data) {
  if (!embedding)
    return -1;

  gvizGraph *g = (gvizGraph *)embedding->subgraph.g;
  size_t oldCap = embedding->subgraph.vertexCapacity;

  if (gvizGraphAddVertex(g, data, NULL, NULL) < 0)
    return -1;
  size_t idx = gvizGraphSize(g) - 1;

  if (gvizSubgraphRebuild(&embedding->subgraph) < 0)
    return -1;

  size_t newCap = embedding->subgraph.vertexCapacity;
  if (newCap > oldCap) {
    size_t dim = embedding->embedding.dim;
    double *grownPositions = GVIZ_REALLOC(embedding->embedding.vertexPositions,
                                          sizeof(double) * newCap * dim);
    if (!grownPositions)
      return -1;
    memset(grownPositions + oldCap * dim, 0,
           sizeof(double) * (newCap - oldCap) * dim);
    embedding->embedding.vertexPositions = grownPositions;

    GVIZ_BIT_ARRAY grownMask = gvizBitArrayResize(
        embedding->drawMask.visibleVertices, oldCap, newCap);
    if (!grownMask)
      return -1;
    gvizVertexSubsetRelease(embedding->drawMask.visibleVertices);
    embedding->drawMask.visibleVertices = grownMask;
  }

  gvizSubgraphShowVertex(&embedding->subgraph, idx);
  gvizEmbeddedGraphDrawMaskShowVertex(embedding, idx);
  gvizEmbeddedGraphDrawMaskNotifyChanged(embedding);

  return 0;
}

int gvizEmbeddedGraphAddEdge(gvizEmbeddedGraph *embedding, size_t from,
                             size_t to) {
  if (!embedding)
    return -1;

  gvizGraph *g = (gvizGraph *)embedding->subgraph.g;
  if (gvizGraphAddEdge(g, from, to) < 0)
    return -1;

  if (gvizSubgraphIsFull(&embedding->subgraph)) {
    if (gvizSubgraphRebuild(&embedding->subgraph) < 0)
      return -1;
    gvizSubgraphShowEdge(&embedding->subgraph, from, to);
    gvizEmbeddedGraphDrawMaskNotifyChanged(embedding);
  }

  return 0;
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

void gvizEmbeddedGraphRandomizePositions(gvizEmbeddedGraph *embedding,
                                         double boxExtent, unsigned int seed) {
  if (!embedding)
    return;

  if (seed == 0)
    seed = (unsigned int)time(NULL);

  size_t dim = embedding->embedding.dim;
  double pos[dim];
  gvizSubgraphVertexIterator vit =
      gvizSubgraphVertexIteratorCreate(&embedding->subgraph);
  size_t u;
  while (gvizSubgraphVertexIterate(&vit, &u)) {
    for (size_t d = 0; d < dim; d++) {
      double unit = (double)rand_r(&seed) / ((double)RAND_MAX + 1.0);
      pos[d] = boxExtent * (2.0 * unit - 1.0);
    }
    gvizEmbeddedGraphSetVPosition(embedding, u, pos);
  }
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
  if (!fgets(name, sizeof(name), f)) {
    fclose(f);
    return -1;
  }

  size_t vertexCount, dim;
  if (fscanf(f, "%zu %zu", &vertexCount, &dim) != 2) {
    fclose(f);
    return -1;
  }
  if (vertexCount != gvizGraphSize(embedding->subgraph.g) ||
      dim != embedding->embedding.dim) {
    fclose(f);
    return -1;
  }

  for (size_t i = 0; i < vertexCount; i++) {
    double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
    for (size_t j = 0; j < dim; j++) {
      if (fscanf(f, "%lf", &pos[j]) != 1) {
        fclose(f);
        return -1;
      }
    }
  }

  fclose(f);
  return 0;
}

int gvizEmbeddedGraphIsPlanarEmbedded(const gvizEmbeddedGraph *embedding) {
  return embedding && embedding->planarEmbedded;
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
