#include "embedders/gvizEmbeddedGraph.h"
#include "cblas.h"
#include "core/alloc.h"
#include "ds/gvizGraph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gvizEmbeddedGraphInit(gvizEmbeddedGraph *embedding, gvizSubgraph subgraph,
                          size_t n) {
  embedding->subgraph = subgraph;
  const gvizGraph *graph = subgraph.g;
  embedding->embedding.dim = n;
  embedding->actions = (gvizActionRegistry){0};
  embedding->embedding.vertexPositions =
      GVIZ_ALLOC(sizeof(double) * gvizGraphSize(graph) * n);
  if (!embedding->embedding.vertexPositions)
    return -1;
  memset(embedding->embedding.vertexPositions, 0,
         sizeof(double) * gvizGraphSize(graph) * n);

  return 0;
}

void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding) {
  gvizSubgraphRelease(&embedding->subgraph);
  if (embedding->embedding.vertexPositions) {
    GVIZ_DEALLOC(embedding->embedding.vertexPositions);
    embedding->embedding.vertexPositions = NULL;
  }
  if (embedding->actions.actions) {
    GVIZ_DEALLOC(embedding->actions.actions);
  }
  embedding->actions = (gvizActionRegistry){0};
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

double *gvizEmbeddedGraphGetVPosition(gvizEmbeddedGraph *embedding,
                                      size_t idx) {
  return embedding->embedding.vertexPositions + idx * embedding->embedding.dim;
}

void gvizEmbeddedGraphSetVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position) {
  cblas_dcopy(embedding->embedding.dim, position, 1,
              gvizEmbeddedGraphGetVPosition(embedding, idx), 1);
}

void gvizEmbeddedGraphAddVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position) {
  cblas_daxpy(embedding->embedding.dim, 1, position, 1,
              gvizEmbeddedGraphGetVPosition(embedding, idx), 1);
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
