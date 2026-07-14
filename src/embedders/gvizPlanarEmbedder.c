#include "embedders/gvizPlanarEmbedder.h"
#include "embedders/gvizSchnyderWood.h"
#include "boyerMyrvold/appconst.h"
#include "boyerMyrvold/graph.h"
#include "core/alloc.h"
#include "ds/gvizArray.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void gvizAdjacencyFromGP(graphP theGraph, int v, gvizArray *out,
                                int *outDegree) {
  int N = theGraph->N;
  int J;

  *outDegree = 0;
  if (!out)
    return;

  out->count = 0;

  if (theGraph == NULL || v < 0 || v >= N)
    return;

  J = theGraph->G[v].link[1];
  while (J >= N) {
    size_t curr = (size_t)theGraph->G[J].v;
    if (curr < (size_t)N)
      gvizArrayPush(out, &curr);
    J = theGraph->G[J].link[1];
  }

  for (size_t i = 0; i + i < out->count; i++) {
    size_t a = *(size_t *)gvizArrayAtIndex(out, i);
    size_t b = *(size_t *)gvizArrayAtIndex(out, out->count - 1 - i);
    *(size_t *)gvizArrayAtIndex(out, i) = b;
    *(size_t *)gvizArrayAtIndex(out, out->count - 1 - i) = a;
  }

  *outDegree = (int)out->count;
}

static gvizGraph *kuratowskiFromBoyer(graphP g) {
  gvizGraph *kg = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!kg)
    return NULL;

  gvizGraphInitAtCapacity(kg, 0, g->N);

  for (int i = 0; i < g->N; i++)
    gvizGraphAddVertex(kg, NULL, NULL, NULL);

  for (int u = 0; u < g->N; u++) {
    int J = g->G[u].link[0];
    if (J < 2 * g->N)
      continue;
    int startArc = J;
    do {
      int v = g->G[J].v;
      if (v < u)
        gvizGraphAddEdge(kg, u, v);
      J = g->G[J].link[0];
    } while (J != startArc);
  }

  return kg;
}

static void mergeRotationIntoAdjacency(gvizGraph *graph, const gvizSubgraph *sg,
                                       graphP boyer, size_t u) {
  gvizArray boyerOrder;
  gvizArrayInit(&boyerOrder, sizeof(size_t));
  int degree = 0;
  gvizAdjacencyFromGP(boyer, (int)u, &boyerOrder, &degree);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(graph, u);
  gvizArray preserved;
  gvizArrayInit(&preserved, sizeof(size_t));

  for (size_t i = 0; i < neighbors->count; i++) {
    size_t v = *(size_t *)gvizArrayAtIndex(neighbors, i);
    if (!gvizSubgraphHasEdge(sg, u, v))
      gvizArrayPush(&preserved, &v);
  }

  neighbors->count = 0;
  for (size_t i = 0; i < boyerOrder.count; i++) {
    size_t v = *(size_t *)gvizArrayAtIndex(&boyerOrder, i);
    gvizArrayPush(neighbors, &v);
  }
  for (size_t i = 0; i < preserved.count; i++) {
    size_t v = *(size_t *)gvizArrayAtIndex(&preserved, i);
    if (gvizArrayFindOne(neighbors, &v) < 0)
      gvizArrayPush(neighbors, &v);
  }

  gvizArrayRelease(&boyerOrder);
  gvizArrayRelease(&preserved);
}

static int copyBoyerGraphIntoSubgraph(const gvizSubgraph *sg, graphP boyer) {
  size_t u;
  gvizSubgraphVertexIterator vit = gvizSubgraphVertexIteratorCreate(sg);
  while (gvizSubgraphVertexIterate(&vit, &u)) {
    size_t v;
    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, u);
    while (gvizSubgraphNeighborIterate(&nit, &v)) {
      if (v > u)
        continue;
      if (gp_AddEdge(boyer, (int)u, 0, (int)v, 0) != OK)
        return -1;
    }
  }
  return 0;
}

int gvizSubgraphApplyPlanarRotation(gvizSubgraph *subgraph,
                                    gvizGraph **kuratowski) {
  if (!subgraph || !subgraph->g)
    return -1;

  const gvizGraph *graph = subgraph->g;
  size_t N = gvizGraphSize(graph);

  graphP boyer = gp_New();
  if (!boyer)
    return -1;

  if (gp_InitGraph(boyer, (int)N) != OK) {
    gp_Free(&boyer);
    return -1;
  }

  if (copyBoyerGraphIntoSubgraph(subgraph, boyer) < 0) {
    gp_Free(&boyer);
    return -1;
  }

  int res = gp_Embed(boyer, 0);
  if (res == NONPLANAR) {
    if (kuratowski) {
      *kuratowski = kuratowskiFromBoyer(boyer);
      if (!*kuratowski) {
        gp_Free(&boyer);
        return -1;
      }
    }
    gp_Free(&boyer);
    return -2;
  }

  if (res != OK) {
    gp_Free(&boyer);
    return -1;
  }

  if (gp_SortVertices(boyer) != OK) {
    gp_Free(&boyer);
    return -1;
  }

  gvizGraph *mutableGraph = (gvizGraph *)graph;
  gvizSubgraphVertexIterator vit = gvizSubgraphVertexIteratorCreate(subgraph);
  size_t u;
  while (gvizSubgraphVertexIterate(&vit, &u))
    mergeRotationIntoAdjacency(mutableGraph, subgraph, boyer, u);

  gp_Free(&boyer);

  gvizGraphBuildLayout(mutableGraph);
  if (gvizSubgraphRebuild(subgraph) < 0)
    return -1;

  return 0;
}

int gvizPlanarEmbedderInit(gvizPlanarEmbedderState *state,
                           gvizSubgraph subgraph) {
  state->kuratowskiSubdivision = NULL;

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  if (gvizEmbeddedGraphInit(embedding, subgraph, 2) < 0)
    return -1;

  int res = gvizSubgraphApplyPlanarRotation(&embedding->subgraph,
                                            &state->kuratowskiSubdivision);
  if (res == 0)
    embedding->planarEmbedded = 1;
  return res;
}

void gvizPlanarEmbedderRelease(gvizPlanarEmbedderState *g) {
  gvizEmbeddedGraphRelease(&g->embedding);
  if (g->kuratowskiSubdivision)
    GVIZ_DEALLOC(g->kuratowskiSubdivision);
}

size_t gvizPlanarPrevNeighborCCW(const gvizGraph *g, size_t u, size_t v) {
  gvizArray *neighbors = gvizGraphGetVertexNeighbors(g, u);
  int idx = gvizArrayFindOne(neighbors, &v);
  assert(idx >= 0);
  return *(size_t *)gvizArrayAtIndex(
      neighbors, (idx == 0 ? neighbors->count : (size_t)idx) - 1);
}

size_t gvizPlanarNextNeighborCCW(const gvizGraph *g, size_t u, size_t v) {
  gvizArray *neighbors = gvizGraphGetVertexNeighbors(g, u);
  int idx = gvizArrayFindOne(neighbors, &v);
  assert(idx >= 0);
  return *(size_t *)gvizArrayAtIndex(neighbors, ((size_t)idx + 1) % neighbors->count);
}

gvizPlanarHalfEdge gvizPlanarHalfEdgeTwin(gvizPlanarHalfEdge e) {
  return (gvizPlanarHalfEdge){e.v, e.u};
}

gvizPlanarHalfEdge gvizPlanarHalfEdgeNext(const gvizSubgraph *sg,
                                           gvizPlanarHalfEdge e) {
  size_t w = gvizPlanarPrevNeighborCCW(sg->g, e.v, e.u);
  return (gvizPlanarHalfEdge){e.v, w};
}

int gvizPlanarFaceWalkBegin(const gvizSubgraph *sg, gvizPlanarHalfEdge start,
                            gvizPlanarFaceWalk *walk) {
  if (!sg || !walk || !gvizSubgraphHasEdge(sg, start.u, start.v))
    return -1;

  walk->sg = sg;
  walk->start = start;
  walk->current = start;
  walk->active = 1;
  return 0;
}

int gvizPlanarFaceWalkStep(gvizPlanarFaceWalk *walk, size_t *outV) {
  if (!walk || !walk->active || !outV)
    return -1;

  *outV = walk->current.v;
  walk->current = gvizPlanarHalfEdgeNext(walk->sg, walk->current);

  if (walk->current.u == walk->start.u && walk->current.v == walk->start.v)
    return 0;
  return 1;
}

static size_t subgraphDartCount(const gvizSubgraph *sg) {
  size_t darts = 0;
  size_t u;
  gvizSubgraphVertexIterator vit = gvizSubgraphVertexIteratorCreate(sg);
  while (gvizSubgraphVertexIterate(&vit, &u))
    darts += gvizGraphGetVertexNeighbors(sg->g, u)->count;
  return darts;
}

static void buildDartBorders(const gvizSubgraph *sg, size_t *borders,
                             size_t *dCount) {
  const gvizGraph *graph = sg->g;
  size_t N = gvizGraphSize(graph);
  *dCount = 0;

  for (size_t u = 0; u < N; u++) {
    borders[u] = *dCount;
    if (gvizSubgraphHasVertex(sg, u))
      *dCount += gvizGraphGetVertexNeighbors(graph, u)->count;
  }
}

int gvizFaceIteratorInit(const gvizPlanarEmbedderState *state,
                         gvizFaceIteratorContext *context) {
  const gvizEmbeddedGraph *embedding = (const gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  size_t N = gvizGraphSize(sg->g);

  context->dCount = subgraphDartCount(sg);
  context->borders = GVIZ_ALLOC(sizeof(size_t) * N);
  if (!context->borders)
    return -1;

  buildDartBorders(sg, context->borders, &context->dCount);

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
}

int gvizPlanarTraceFace(const gvizSubgraph *sg, gvizFaceIteratorContext *context,
                        size_t u, size_t adjIdx, gvizArray *face) {
  typedef struct {
    size_t u;
    size_t idx;
  } dart;

#define VISITED_DART(d)                                                        \
  (gvizTestBit(context->visited, context->borders[d.u] + d.idx))
#define MARK_DART(d)                                                           \
  (gvizSetBit(context->visited, context->borders[d.u] + d.idx))

  dart d = {u, adjIdx};

  while (!VISITED_DART(d)) {
    MARK_DART(d);

    gvizArray *uNeighbors = gvizGraphGetVertexNeighbors(sg->g, d.u);
    size_t v = *(size_t *)gvizArrayAtIndex(uNeighbors, d.idx);
    gvizArrayPush(face, &v);

    gvizArray *vNeighbors = gvizGraphGetVertexNeighbors(sg->g, v);
    int prev = gvizArrayFindOne(vNeighbors, &d.u);
    assert(prev >= 0);
    d = (dart){v, (prev == 0 ? vNeighbors->count : (size_t)prev) - 1};
  }

  return 0;
}

int gvizPlanarEmbedderFaces(const gvizPlanarEmbedderState *state,
                             gvizFaceIteratorContext *context) {
  const gvizEmbeddedGraph *embedding = (const gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  size_t N = gvizGraphSize(sg->g);

  size_t u;
  gvizSubgraphVertexIterator vit = gvizSubgraphVertexIteratorCreate(sg);
  while (gvizSubgraphVertexIterate(&vit, &u)) {
    gvizArray *neighbors = gvizGraphGetVertexNeighbors(sg->g, u);

    for (size_t i = 0; i < neighbors->count; i++) {
      size_t v = *(size_t *)gvizArrayAtIndex(neighbors, i);
      if (!gvizSubgraphHasEdge(sg, u, v))
        continue;

      if (gvizTestBit(context->visited, context->borders[u] + i))
        continue;

      gvizArray face;
      if (gvizArrayInit(&face, sizeof(size_t)) < 0)
        return -1;

      if (gvizPlanarTraceFace(sg, context, u, i, &face) < 0) {
        gvizArrayRelease(&face);
        return -1;
      }

      if (face.count >= 3)
        gvizArrayPush(&context->faces, &face);
      else
        gvizArrayRelease(&face);
    }
  }

  (void)N;
  return 0;
}

void gvizPlanarEmbedderTriangulate(const gvizPlanarEmbedderState *state,
                                    gvizFaceIteratorContext *context) {
  const gvizEmbeddedGraph *embedding = (const gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  gvizGraph *graph = (gvizGraph *)sg->g;

  for (size_t i = 0; i < context->faces.count; i++) {
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&context->faces, i);
  t:
    if (face->count == 3)
      continue;

    for (size_t x = 0; x < 4; x++) {
      for (size_t y = x + 1; y < 4; y++) {
        size_t u = *(size_t *)gvizArrayAtIndex(face, x);
        size_t v = *(size_t *)gvizArrayAtIndex(face, y);

        if (u == v || gvizGraphEdgeExists(graph, u, v))
          continue;

        size_t idx1 = *(size_t *)gvizArrayAtIndex(face, (x + 1) % face->count);
        idx1 = gvizArrayFindOne(gvizGraphGetVertexNeighbors(graph, u), &idx1);
        assert(idx1 != (size_t)-1);
        idx1 = (idx1 + 1) % gvizGraphGetVertexNeighbors(graph, u)->count;

        size_t idx2 = *(size_t *)gvizArrayAtIndex(face, (y - 1) % face->count);
        idx2 = gvizArrayFindOne(gvizGraphGetVertexNeighbors(graph, v), &idx2);
        assert(idx2 != (size_t)-1);

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

        gvizArrayInsert(gvizGraphGetVertexNeighbors(graph, u), &v, idx1);
        gvizArrayInsert(gvizGraphGetVertexNeighbors(graph, v), &u, idx2);
        gvizSubgraphShowEdge((gvizSubgraph *)&embedding->subgraph, u, v);

        context->dCount += 2;
        goto t;
      }
    }
  }

  gvizGraphBuildLayout(graph);
  gvizSubgraphRebuild((gvizSubgraph *)&embedding->subgraph);
}

int gvizPlanarLargestFaceBoundary(const gvizGraph *g, const gvizSubgraph *sg,
                                  size_t *out, size_t max, size_t *count) {
  if (!g || !sg || !out || !count)
    return -1;

  gvizPlanarEmbedderState state;
  memset(&state, 0, sizeof(state));
  state.embedding.subgraph = *sg;
  state.embedding.subgraph.g = g;

  gvizFaceIteratorContext ctx;
  if (gvizFaceIteratorInit(&state, &ctx) < 0)
    return -1;

  if (gvizPlanarEmbedderFaces(&state, &ctx) < 0) {
    gvizFaceIteratorRelease(&ctx);
    return -1;
  }

  size_t best = 0;
  size_t bestIdx = (size_t)-1;
  for (size_t i = 0; i < ctx.faces.count; i++) {
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&ctx.faces, i);
    if (face->count < 3)
      continue;
    if (face->count > best) {
      best = face->count;
      bestIdx = i;
    }
  }

  if (bestIdx == (size_t)-1 || best > max) {
    gvizFaceIteratorRelease(&ctx);
    return -1;
  }

  gvizArray *face = (gvizArray *)gvizArrayAtIndex(&ctx.faces, bestIdx);
  *count = face->count;
  for (size_t i = 0; i < face->count; i++)
    out[i] = *(size_t *)gvizArrayAtIndex(face, i);

  gvizFaceIteratorRelease(&ctx);
  return 0;
}

int gvizPlanarEmbedderEmbed(gvizPlanarEmbedderState *state) {
  gvizFaceIteratorContext faces;
  if (gvizFaceIteratorInit(state, &faces) < 0)
    return -1;

  if (gvizPlanarEmbedderFaces(state, &faces) < 0) {
    gvizFaceIteratorRelease(&faces);
    return -1;
  }

  gvizPlanarEmbedderTriangulate(state, &faces);

  gvizSchnyderWood sw;
  gvizGraph *g = (gvizGraph *)state->embedding.subgraph.g;
  if (gvizSchnyderWoodInit(&sw, g) < 0) {
    gvizFaceIteratorRelease(&faces);
    return -1;
  }

  gvizSchnyderWoodEmbed(&sw, (gvizEmbeddedGraph *)state);
  gvizSchnyderWoodRelease(&sw);
  gvizFaceIteratorRelease(&faces);
  return 0;
}
