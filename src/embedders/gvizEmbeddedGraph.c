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
