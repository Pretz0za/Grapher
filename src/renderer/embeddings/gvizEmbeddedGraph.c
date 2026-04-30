#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "cblas.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizGraphView.h"
#include "raylib.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int allocPositions(gvizEmbeddedGraph *eg, size_t count, size_t dim) {
  if (count == 0 || dim == 0) {
    eg->embedding.vertexPositions = NULL;
    return 0;
  }
  eg->embedding.vertexPositions =
      GVIZ_ALLOC(sizeof(double) * count * dim);
  if (!eg->embedding.vertexPositions) {
    return -1;
  }
  memset(eg->embedding.vertexPositions, 0, sizeof(double) * count * dim);
  return 0;
}

static int buildLocalTable(gvizEmbeddedGraph *eg) {
  size_t n = eg->view.graph->vertices.count;
  eg->local = GVIZ_ALLOC(sizeof(size_t) * n);
  if (n > 0 && !eg->local) {
    return -1;
  }
  for (size_t i = 0; i < n; i++) {
    eg->local[i] = SIZE_MAX;
  }
  size_t next = 0;
  gvizGraphViewVertexIter it;
  gvizGraphViewVertexIterInit(&it, &eg->view);
  size_t u;
  while (gvizGraphViewVertexIterNext(&it, &u)) {
    eg->local[u] = next++;
  }
  gvizGraphViewVertexIterRelease(&it);
  return 0;
}

int gvizEmbeddedGraphInitView(gvizEmbeddedGraph *eg, gvizGraphView view,
                              gvizEmbeddingMode mode, size_t dim) {
  if (!eg || !view.graph) {
    return -1;
  }
  eg->view = view;
  eg->graph = view.graph;
  eg->mode = mode;
  eg->embedding.dim = dim;
  eg->embedding.vertexPositions = NULL;
  eg->local = NULL;

  size_t posCount;
  if (mode == GVIZ_EMBED_VIEW_ONLY) {
    posCount = eg->view.count;
    if (buildLocalTable(eg) != 0) {
      gvizGraphViewRelease(&eg->view);
      return -1;
    }
  } else {
    posCount = eg->view.graph->vertices.count;
  }

  if (allocPositions(eg, posCount, dim) != 0) {
    if (eg->local) {
      GVIZ_DEALLOC(eg->local);
      eg->local = NULL;
    }
    gvizGraphViewRelease(&eg->view);
    return -1;
  }
  return 0;
}

int gvizEmbeddedGraphInit(gvizEmbeddedGraph *eg, gvizGraph *graph,
                          size_t dim) {
  if (!eg || !graph) {
    return -1;
  }
  gvizGraphView view;
  if (gvizGraphViewInitFull(&view, graph) != 0) {
    return -1;
  }
  return gvizEmbeddedGraphInitView(eg, view, GVIZ_EMBED_FULL_GRAPH, dim);
}

size_t gvizEmbeddedGraphLocalIndex(const gvizEmbeddedGraph *eg, size_t u) {
  if (!eg || !eg->view.graph || u >= eg->view.graph->vertices.count) {
    return SIZE_MAX;
  }
  if (eg->mode == GVIZ_EMBED_FULL_GRAPH) {
    return u;
  }
  return eg->local ? eg->local[u] : SIZE_MAX;
}

void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *eg) {
  if (!eg) {
    return;
  }
  if (eg->embedding.vertexPositions) {
    GVIZ_DEALLOC(eg->embedding.vertexPositions);
    eg->embedding.vertexPositions = NULL;
  }
  if (eg->local) {
    GVIZ_DEALLOC(eg->local);
    eg->local = NULL;
  }
  gvizGraphViewRelease(&eg->view);
  eg->graph = NULL;
}

size_t gvizIterateSearch(gvizEmbeddedGraph *embedding,
                         gvizFaceSearchState *state);

double *gvizEmbeddedGraphGetVPosition(gvizEmbeddedGraph *eg, size_t idx) {
  size_t local = gvizEmbeddedGraphLocalIndex(eg, idx);
  if (local == SIZE_MAX) {
    return NULL;
  }
  return eg->embedding.vertexPositions + local * eg->embedding.dim;
}

void gvizEmbeddedGraphSetVPosition(gvizEmbeddedGraph *eg, size_t idx,
                                   double *position) {
  double *dest = gvizEmbeddedGraphGetVPosition(eg, idx);
  if (!dest) {
    return;
  }
  cblas_dcopy(eg->embedding.dim, position, 1, dest, 1);
}

void gvizEmbeddedGraphAddVPosition(gvizEmbeddedGraph *eg, size_t idx,
                                   double *position) {
  double *dest = gvizEmbeddedGraphGetVPosition(eg, idx);
  if (!dest) {
    return;
  }
  cblas_daxpy(eg->embedding.dim, 1, position, 1, dest, 1);
}

int gvizEmbeddedGraphSaveEmbedding(gvizEmbeddedGraph *eg, const char *name,
                                   const char *filename) {
  FILE *f = fopen(filename, "w");
  if (!f) {
    return -1;
  }
  fprintf(f, "%s\n", name);
  fprintf(f, "%d\n", (int)eg->mode);
  size_t n = eg->view.graph->vertices.count;
  fprintf(f, "%zu %zu\n", n, eg->embedding.dim);
  for (size_t i = 0; i < n; i++) {
    double *pos = gvizEmbeddedGraphGetVPosition(eg, i);
    if (!pos) {
      for (size_t j = 0; j < eg->embedding.dim; j++) {
        fprintf(f, "0.0 ");
      }
    } else {
      for (size_t j = 0; j < eg->embedding.dim; j++) {
        fprintf(f, "%f ", pos[j]);
      }
    }
    fprintf(f, "\n");
  }
  fclose(f);
  return 0;
}

int gvizEmbeddedGraphLoadEmbedding(gvizEmbeddedGraph *eg,
                                   const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f) {
    return -1;
  }
  char name[256];
  fgets(name, 256, f);

  int mode;
  fscanf(f, "%d", &mode);

  size_t vertexCount, dim;
  fscanf(f, "%zu %zu", &vertexCount, &dim);
  if (vertexCount != eg->view.graph->vertices.count ||
      dim != eg->embedding.dim || (gvizEmbeddingMode)mode != eg->mode) {
    fclose(f);
    return -1;
  }
  for (size_t i = 0; i < vertexCount; i++) {
    double *pos = gvizEmbeddedGraphGetVPosition(eg, i);
    for (size_t j = 0; j < dim; j++) {
      double x;
      if (fscanf(f, "%lf", &x) != 1) {
        fclose(f);
        return -1;
      }
      if (pos) {
        pos[j] = x;
      }
    }
  }
  fclose(f);
  return 0;
}
