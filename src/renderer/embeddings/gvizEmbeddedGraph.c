#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "cblas.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>

int gvizEmbeddedGraphInit(gvizEmbeddedGraph *embedding, gvizGraph *graph,
                          size_t n) {
  embedding->graph = graph;
  embedding->embedding.dim = n;
  embedding->embedding.vertexPositions =
      GVIZ_ALLOC(sizeof(double) * graph->vertices.count * n);
  if (!embedding->embedding.vertexPositions)
    return -1;
  memset(embedding->embedding.vertexPositions, 0,
         sizeof(double) * graph->vertices.count * n);

  // Count the edges in the graph to avoid overallocating later.
  // O(1) initializtion doable by making an n * n 2d bit array, where we
  // have:
  //                arr[idx(u, v)] = uv visted ? 1 : 0;
  // The disadvantage in this in that we are allocating memory for
  // non-existent edges. In fact, in most cases, of the n * n array will be
  // unused.
  //
  // Alternative is only allocating for the number of edges we have. This is
  // O(n) instead. Additionally, it requires to keep track of where each
  // vertex's half edges list is stored. This is a natural side-effect of
  // managing an array of variable-sized arrays. In total this requires:
  //
  //          2*m bits for the bit array (2 halves to each edge) +
  //               8*n bits for the offsets arr (sizeof(size_t)) =
  //                          O(m + n) memory compared to O(n^2) ;
  //
  // Additionally, to do anything that useful with an embedded graph it'll
  // ususally be an algorithm thats O(n) in time at least. Which, makes this
  // O(n) for initialization less costly in the big picture.

  // size_t *offsets = GVIZ_ALLOC(sizeof(size_t) * graph->vertices.count);
  // size_t edgeCount = 0;
  // if (!offsets)
  //   return -1;
  // for (size_t i = 0; i < graph->vertices.count; i++) {
  //   offsets[i] = edgeCount;
  //   edgeCount += (gvizGraphGetVertexNeighbors(graph, i)->count);
  // }
  //
  // size_t size = sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(edgeCount);
  // GVIZ_BIT_ARRAY arr = GVIZ_ALLOC(size);
  // if (!arr)
  //   return -1;
  // memset(arr, 0, size);

  return 0;
}

size_t gvizIterateSearch(gvizEmbeddedGraph *embedding,
                         gvizFaceSearchState *state); //{
//   gvizArray *neighbors =
//       gvizGraphGetVertexNeighbors(embedding->graph, state->currVertex);
//   size_t *last = gvizArrayFindOne(neighbors, &(state->lastVertex));
//   state->lastVertex = state->currVertex;
//   size_t idx = last - (size_t *)neighbors->arr;
//   idx = idx == neighbors->count - 1 ? 0 : idx + 1;
//   state->currVertex = idx;
//   return idx;
// }
//
// int gvizEmbeddedGraphNextFace(gvizEmbeddedGraph *embedding,
//                               gvizFaceSearchState *state) {
//   size_t nextVertex = gvizIterateSearch(embedding, state);
//   if (GVI) {
//     return int(nextVertex);
//   }
//   return gvizEmbeddedGraphNextFace(embedding, state);
// }
//

void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding) {
  if (embedding->embedding.vertexPositions) {
    GVIZ_DEALLOC(embedding->embedding.vertexPositions);
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

  fprintf(f, "%zu %zu\n", embedding->graph->vertices.count,
          embedding->embedding.dim);
  for (size_t i = 0; i < embedding->graph->vertices.count; i++) {
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
  if (vertexCount != embedding->graph->vertices.count ||
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
