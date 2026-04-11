#include "renderer/embeddings/gvizPlanarEmbedding.h"
#include "boyerMyrvold/appconst.h"
#include "boyerMyrvold/graph.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "utils/serializers.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gvizAdjacencyFromGP(graphP theGraph, int v, gvizArray *out,
                         int *outDegree) {

  int N = theGraph->N;
  int startArc, J, degree = 0;

  *outDegree = 0;

  if (!out)
    return;

  out->count = 0;

  if (theGraph == NULL || v < 0 || v >= N)
    return;

  // No incident edges -> isolated vertex
  if (theGraph->G[v].link[0] < 2 * N) // link[0] points to vertex => no edges
    return;

  startArc = J = theGraph->G[v].link[0];

  do {
    // J is an edge record incident to v
    // Neighbor of v through this edge record
    size_t curr = theGraph->G[J].v;

    if (curr == v) {
      break;
    }

    degree++;
    gvizArrayPush(out, &curr);

    // Step to next edge record in v's circular adjacency list.
    // Use link[0] or link[1] consistently; it just picks direction.
    J = theGraph->G[J].link[0];

    // J should always be >= 2*N here if the structure is consistent
  } while (J != startArc);

  *outDegree = degree;
}
int gvizPlanarEmbeddingInit(gvizPlanarEmbeddingState *state, gvizGraph *graph) {
  state->kuratowskiSubdivision = NULL;

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizEmbeddedGraphInit(embedding, graph, 2);

  size_t N = embedding->graph->vertices.count;

  // Copy the gvizGraph into a new BoyerMyrvold graph struct.
  graphP g = gp_New();

  if (gp_InitGraph(g, N) != OK)
    return -1;

  for (size_t i = 0; i < N; i++) {
    gvizArray *neighbors = gvizGraphGetVertexNeighbors(embedding->graph, i);

    for (size_t j = 0; j < neighbors->count; j++) {
      size_t curr = *(size_t *)gvizArrayAtIndex(neighbors, j);

      if (curr > i)
        continue;

      if (gp_AddEdge(g, (int)i, 0, (int)curr, 0) != OK) {
        return -1;
      }
    }
  }

  // embed
  int res = gp_Embed(g, 0);

  if (res == NONPLANAR) {
    state->kuratowskiSubdivision = GVIZ_ALLOC(sizeof(gvizGraph));
    gvizGraph *kg = state->kuratowskiSubdivision;

    gvizGraphInitAtCapacity(kg, 0, g->N);

    for (int i = 0; i < g->N; i++)
      gvizGraphAddVertex(kg, NULL, NULL, NULL);

    for (int u = 0; u < g->N; u++) {
      int J = g->G[u].link[0];
      if (J < 2 * g->N)
        continue; // isolated
      int startArc = J;
      do {
        int v = g->G[J].v;
        if (v < u) { // add each undirected edge once
          gvizGraphAddEdge(kg, u, v);
        }
        J = g->G[J].link[0];
      } while (J != startArc);
    }

    gp_Free(&g);
    return -2; // or some code telling caller "non-planar, obstruction stored"
  }

  if (res != OK)
    return -1;

  // copy rotation order to gviz:
  for (size_t i = 0; i < N; i++) {
    gvizArray *neighbors = gvizGraphGetVertexNeighbors(embedding->graph, i);
    int degree;
    gvizAdjacencyFromGP(g, i, neighbors, &degree);
  }

  gp_Free(&g);
  return 0;
}

void gvizPlanarEmbeddingRelease(gvizPlanarEmbeddingState *g) {
  gvizEmbeddedGraphRelease(&g->embedding);
  if (g->kuratowskiSubdivision)
    GVIZ_DEALLOC(g->kuratowskiSubdivision);
}

int gvizFaceIteratorInit(const gvizPlanarEmbeddingState *state,
                         gvizFaceIteratorContext *context) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t N = embedding->graph->vertices.count;
  context->dCount = 0;

  context->borders = GVIZ_ALLOC(sizeof(size_t) * N);
  if (!context->borders)
    return -1;
  memset(context->borders, 0, sizeof(size_t) * N);

  // Counts darts. Each edge is counted twice, but thats intended.
  for (size_t i = 0; i < N; i++) {
    gvizArray *neighbors = gvizGraphGetVertexNeighbors(embedding->graph, i);
    context->borders[i] = context->dCount;
    context->dCount += neighbors->count;
  }

  context->visited =
      GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(context->dCount));
  if (!context->visited) {
    GVIZ_DEALLOC(context->borders);
    return -1;
  }
  memset(context->visited, 0,
         sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(context->dCount));

  if (gvizArrayInit(&context->faces, sizeof(gvizArray)) < 0) {
    GVIZ_DEALLOC(context->borders);
    GVIZ_DEALLOC(context->visited);
    return -1;
  }

  return 0;
}

void gvizFaceIteratorRelease(gvizFaceIteratorContext *context) {
  GVIZ_DEALLOC(context->borders);
  GVIZ_DEALLOC(context->visited);
  for (size_t i = 0; i < context->faces.count; i++)
    gvizArrayRelease(gvizArrayAtIndex(&context->faces, i));
  gvizArrayRelease(&context->faces);
  return;
}

// The previous half-edge out of u, in it's rotation system
size_t previousNeighbor(const gvizGraph *g, size_t u, size_t v) {
  gvizArray *neighbors = gvizGraphGetVertexNeighbors(g, u);
  size_t idx = gvizArrayFindOne(neighbors, &v);
  assert(idx >= 0);
  return (idx == 0 ? neighbors->count : idx) - 1;
}

int gvizPlanarEmbeddingFaces(const gvizPlanarEmbeddingState *state,
                             gvizFaceIteratorContext *context) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t N = embedding->graph->vertices.count, M = context->dCount / 2;

  typedef struct {
    size_t u;
    size_t idx; // idx in u's adjacency list
  } dart;

#define VISITED_DART(d)                                                        \
  (gvizTestBit(context->visited, context->borders[d.u] + d.idx))
#define MARK_DART(d)                                                           \
  (gvizSetBit(context->visited, context->borders[d.u] + d.idx))

  dart d;

  for (size_t u = 0; u < N; u++) {
    gvizArray *neighbors = gvizGraphGetVertexNeighbors(embedding->graph, u);

    for (size_t i = 0; i < neighbors->count; i++) {
      d = (dart){u, i};

      // check if the half edge u -> v is visited
      if (VISITED_DART(d))
        continue;

      gvizArray face;
      if (gvizArrayInit(&face, sizeof(size_t)) < 0)
        return -1;

      // main loop. traverse current face:
      while (!VISITED_DART(d)) {
        MARK_DART(d);

        gvizArray *uNeighbors =
            gvizGraphGetVertexNeighbors(embedding->graph, d.u);

        size_t v = *(size_t *)gvizArrayAtIndex(uNeighbors, d.idx);
        gvizArrayPush(&face, &v);

        d = (dart){v, previousNeighbor(embedding->graph, v, d.u)};
      }

      if (face.count > 0) {
        gvizArrayPush(&context->faces, &face);
        // new face:
        gvizArrayInit(&face, sizeof(size_t));
      } else
        gvizArrayRelease(&face);
    }
  }

  return 0;
}

// iterate faces. Any face with 4 or more vertices -> add one edge
void gvizPlanarEmbeddingTriangulate(const gvizPlanarEmbeddingState *state,
                                    gvizFaceIteratorContext *context) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  for (size_t i = 0; i < context->faces.count; i++) {
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&context->faces, i);
  t:
    if (face->count == 3)
      continue;

    printf("current face: ");
    gvizArrayPrint(face, stdout, gvizSerializeUINT64, 8);

    for (size_t x = 0; x < 4; x++) {
      for (size_t y = x + 1; y < 4; y++) {
        size_t u = *(size_t *)gvizArrayAtIndex(face, x);
        size_t v = *(size_t *)gvizArrayAtIndex(face, y);

        if (u == v || gvizGraphEdgeExists(embedding->graph, u, v))
          continue;

        // indices of the adjacency list, so we add edges without breaking the
        // rotation order.
        size_t idx1 = *(size_t *)gvizArrayAtIndex(face, (x + 1) % face->count);

        idx1 = gvizArrayFindOne(
            gvizGraphGetVertexNeighbors(embedding->graph, u), &idx1);
        assert(idx1 != -1);
        idx1 = (idx1 + 1) %
               gvizGraphGetVertexNeighbors(embedding->graph, u)->count;

        size_t idx2 = *(size_t *)gvizArrayAtIndex(face, (y - 1) % face->count);
        idx2 = gvizArrayFindOne(
            gvizGraphGetVertexNeighbors(embedding->graph, v), &idx2);
        assert(idx2 != -1);

        gvizArray newFace;
        gvizArrayInit(&newFace, sizeof(size_t));
        gvizArrayPush(&newFace, &u);
        for (size_t z = x + 1; z < y; z++) {
          size_t w = *(size_t *)gvizArrayAtIndex(face, z);
          gvizArrayDeleteAtIndex(face, z);
          gvizArrayPush(&newFace, &w);
        }
        gvizArrayPush(&newFace, &v);
        gvizArrayPush(&context->faces, &newFace);

        printf("adding edge from %zu to %zu.\n", u, v);
        printf("u adjacency list before: ");
        gvizArrayPrint(gvizGraphGetVertexNeighbors(embedding->graph, u), stdout,
                       gvizSerializeUINT64, 8);

        printf("v adjacency list before: ");
        gvizArrayPrint(gvizGraphGetVertexNeighbors(embedding->graph, v), stdout,
                       gvizSerializeUINT64, 8);

        gvizArrayInsert(gvizGraphGetVertexNeighbors(embedding->graph, u), &v,
                        idx1);
        gvizArrayInsert(gvizGraphGetVertexNeighbors(embedding->graph, v), &u,
                        idx2);

        printf("u adjacency list after: ");
        gvizArrayPrint(gvizGraphGetVertexNeighbors(embedding->graph, u), stdout,
                       gvizSerializeUINT64, 8);

        printf("u adjacency list after: ");
        gvizArrayPrint(gvizGraphGetVertexNeighbors(embedding->graph, v), stdout,
                       gvizSerializeUINT64, 8);

        context->dCount += 2;

        goto t;
      }
    }
  }
}

int gvizPlanarEmbeddingEmbed(gvizPlanarEmbeddingState *state) {}
