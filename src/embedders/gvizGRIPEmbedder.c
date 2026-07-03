#include "embedders/gvizGRIPEmbedder.h"
#include "cblas.h"
#include "core/alloc.h"
#include "algorithms/search/gvizBreadthFirst.h"
#include "algorithms/search/gvizKNearest.h"
#include "ds/gvizArray.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizDeque.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizForceDirected.h"
#include "utils/helpers.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_EDGE_LENGTH 10.0

#define INITIAL_PLACEMENT_K 32
#define REFINEMENT_K 16

static const double gvizGRIPr = 0.15;
static const double gvizGRIPs = 3;

/**
 * Actions exposed to front-ends (renderers, scripts). They let an interactive
 * session drive the multilevel schedule (e.g. bind a key to
 * "grip.refineRound") without the front-end knowing anything about GRIP.
 */
static void gripActionRefineRound(gvizEmbeddedGraph *embedding, void *userData,
                                  const gvizActionPayload *payload) {
  (void)userData;
  (void)payload;
  gvizGRIPState *state = (gvizGRIPState *)embedding;
  if (state->layerCount == 0)
    return; // gvizGRIPEmbedderBegin has not run yet
  runRefinementRound(state);
}

static void gripActionNextStage(gvizEmbeddedGraph *embedding, void *userData,
                                const gvizActionPayload *payload) {
  (void)userData;
  (void)payload;
  gvizGRIPState *state = (gvizGRIPState *)embedding;
  if (state->layerCount == 0)
    return;
  beginNewStage(state);
}

int gvizGRIPEmbedderInit(gvizGRIPState *state, gvizSubgraph subgraph,
                         size_t diameter, size_t dimension) {
  int res;
  const gvizGraph *graph = subgraph.g;
  size_t N = gvizGraphSize(graph);

  state->currLayer = 0;
  state->layerCount = 0;
  state->currRound = 0;

  res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, subgraph, dimension);
  if (res < 0)
    return res;

  size_t tmp = 1024;
  if (diameter) {
    tmp = (size_t)log2(diameter) + 8; // extra slots for safety
  }
  state->misBorder = GVIZ_ALLOC(sizeof(size_t) * tmp);
  if (!state->misBorder)
    return -1;
  state->misBorder[0] = N;

  state->misFiltration = GVIZ_ALLOC(sizeof(size_t) * N);
  if (!state->misFiltration)
    return -1;

  state->dec = GVIZ_ALLOC(sizeof(gvizGRIPDecorators) * N);
  if (!state->dec)
    return -1;
  memset(state->dec, 0, sizeof(gvizGRIPDecorators) * N);

  state->dispCalculated =
      GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));
  if (!state->dispCalculated) {
    return -1;
  }
  memset(state->dispCalculated, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));

  state->placed = gvizVertexSubsetCreateEmpty(graph);
  if (!state->placed) {
    return -1;
  }

  state->radiusBfsStamp = GVIZ_ALLOC(sizeof(size_t) * N);
  if (!state->radiusBfsStamp)
    return -1;
  state->radiusBfsEpoch = 1;
  res = gvizDequeInitAtCapacity(&state->radiusBfsQueue, sizeof(gvizFoundVertex),
                                256);
  if (res < 0)
    return res;

  double *disp = GVIZ_ALLOC(sizeof(double) * N * dimension);
  double *oldDisp = GVIZ_ALLOC(sizeof(double) * N * dimension);

  if (!disp || !oldDisp)
    return -1;

  gvizFoundVertex *gArr =
      GVIZ_ALLOC(sizeof(gvizFoundVertex) * N * REFINEMENT_K);
  if (!gArr)
    return -1;
  memset(gArr, 0, sizeof(gvizFoundVertex) * N * REFINEMENT_K);

  for (size_t i = 0; i < N; i++, gArr += REFINEMENT_K) {

    // manually initalize knn arrays
    gvizArray *arr = &state->dec[i].knn;
    arr->elementSize = sizeof(gvizFoundVertex);
    arr->capacity = REFINEMENT_K;
    arr->count = 0;
    arr->arr = gArr;

    // initalize other decorators
    state->dec[i].disp = disp;
    state->dec[i].oldDisp = oldDisp;
    disp += dimension;
    oldDisp += dimension;
  }

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  if (gvizEmbeddedGraphAddAction(embedding, "grip.refineRound",
                                 gripActionRefineRound, NULL) < 0)
    return -1;
  if (gvizEmbeddedGraphAddAction(embedding, "grip.nextStage",
                                 gripActionNextStage, NULL) < 0)
    return -1;

  return 0;
}

// generates n+1 vertices of a regular n-simplex centered at the origin
// out must be able to hold (n+1) * n doubles (n+1 vertices, each n-dimensional)
// side_length scales the result
void makeRegularSimplex(size_t n, double side_length, double *out) {
  // construct via gram matrix method:
  // place vertices one at a time, each new vertex is constrained by
  // needing equal distance to all previous vertices.
  // v[0] = (1, 0, 0, ..., 0) * scale
  // v[k] is determined by: dot(v[k], v[j]) = same for all j < k
  // which gives a clean recursive construction.

  // all pairwise distances should equal side_length
  // dot product between any two distinct unit-scaled vertices:
  // ||v_i - v_j||^2 = side_length^2
  // ||v_i||^2 + ||v_j||^2 - 2*dot(v_i,v_j) = side_length^2
  // if all ||v_i|| = R, then dot(v_i, v_j) = R^2 - side_length^2/2

  // we build in (n) dimensions directly.
  // v[i] is stored at out + i*n

  memset(out, 0, sizeof(double) * (n + 1) * n);

  for (size_t k = 0; k <= n; k++) {
    double *vk = out + k * n;

    if (k == 0) {
      // first vertex: place on first axis
      vk[0] = 1.0;
      continue;
    }

    // each new vertex must satisfy:
    // dot(vk, vj) = c for all j < k, where c = cos of angle between verts
    // from equidistance: dot(vi,vj) = (1 - side_length^2/2) if unit verts
    // we compute c from the already-placed vertices.
    // dot(vk, v0) = c => vk[0] = c / v0[0] = c (since v0 = (1,0,...))

    // for unit simplex (scale later):
    // dot(vi, vj) = -1/n for all i != j (standard result)
    // so c = -1/n... but only if all verts are unit length.
    // we don't enforce unit length during construction, so track norms.

    // simpler: use the explicit construction.
    // for k < n: vk[j] = c / vj[j] - sum of previous components * vj[j] / vj[j]
    // just solve component by component:

    // for each previous axis component j < k:
    //   dot(vk, vj) must equal dot(v0, v1) = c
    //   vk[j] = (c - sum_{m<j} vk[m]*vj[m]) / vj[j]
    // then vk[k] = sqrt(1 - sum_{m<k} vk[m]^2)  [stay on unit sphere]

    // c for unit regular simplex in n dims = -1/n
    double c = -1.0 / (double)n;

    for (size_t j = 0; j < k; j++) {
      double *vj = out + j * n;
      // compute vk[j]: need dot(vk, vj) = c
      // dot so far (components 0..j-1 already set):
      double dot_so_far = 0.0;
      for (size_t m = 0; m < j; m++)
        dot_so_far += vk[m] * vj[m];
      vk[j] = (c - dot_so_far) / vj[j];
    }

    // set the new free component to stay on unit sphere
    double norm_sq = 0.0;
    for (size_t m = 0; m < k; m++)
      norm_sq += vk[m] * vk[m];
    double rem = 1.0 - norm_sq;
    if (k < n) {
      double rem = 1.0 - norm_sq;
      vk[k] = rem > 0.0 ? sqrt(rem) : 0.0;
    }
  }

  // scale by side_length / edge_length_of_unit_simplex
  // unit simplex edge length: ||v0 - v1||
  double *v0 = out;
  double *v1 = out + n;
  double unit_edge = 0.0;
  for (size_t m = 0; m < n; m++) {
    double d = v0[m] - v1[m];
    unit_edge += d * d;
  }
  unit_edge = sqrt(unit_edge);

  double scale = side_length / unit_edge;
  for (size_t i = 0; i < (n + 1) * n; i++)
    out[i] *= scale;
}

void makeEquilateralTriangle2(double *out, double sideLength) {
  double R = sideLength / sqrt(3.0);

  // v1
  *out++ = 0;
  *out++ = R;

  // v2
  *out++ = -sideLength / 2;
  *out++ = -R / 2;

  // v3
  *out++ = sideLength / 2;
  *out = -R / 2;
}

void gvizGRIPEmbedderRelease(gvizGRIPState *state) {
  gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)state);
  if (state->misBorder)
    GVIZ_DEALLOC(state->misBorder);
  if (state->misFiltration)
    GVIZ_DEALLOC(state->misFiltration);
  if (state->dec) {
    GVIZ_DEALLOC(state->dec[0].disp);
    GVIZ_DEALLOC(state->dec[0].oldDisp);
    GVIZ_DEALLOC(state->dec[0].knn.arr);
    GVIZ_DEALLOC(state->dec);
  }
  if (state->dispCalculated)
    GVIZ_DEALLOC(state->dispCalculated);
  if (state->placed)
    gvizVertexSubsetRelease(state->placed);
  if (state->radiusBfsStamp)
    GVIZ_DEALLOC(state->radiusBfsStamp);
  gvizDequeRelease(&state->radiusBfsQueue);

  return;
}

// NOTE: Can be potentially made better by randomizing the order of the
// vertices. either by shuffling or assigning random priorities to each vertex;
void makeFirstMISPartition(gvizGRIPState *state, gvizVertexSubset out) {
  const gvizSubgraph *sg = &((gvizEmbeddedGraph *)state)->subgraph;
  size_t nvertices = gvizGraphSize(sg->g);
  GVIZ_BIT_UNIT states[GVIZ_ARRAY_UNITS(nvertices)];
  memset(states, 0, sizeof(states));

  size_t *curr = &state->misFiltration[nvertices - 1];

  for (size_t i = 0; i < nvertices; i++) {
    if (!gvizSubgraphHasVertex(sg, i))
      continue;
    if (!gvizTestBit(states, i)) {
      gvizVertexSubsetShowVertex(out, i);

      gvizSubgraphNeighborIterator nit = gvizSubgraphNeighborIteratorCreate(sg, i);
      size_t neighbor;
      while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
        if (!gvizTestBit(states, neighbor)) {
          gvizSetBit(states, neighbor);
          *curr-- = neighbor;
        }
      }
    }
  }

  state->misBorder[1] = gvizVertexSubsetCount(out, nvertices);
}

void migrateOneToFinalLayer(gvizGRIPState *state, GVIZ_BIT_ARRAY finalLayer,
                            size_t count) {
  const gvizSubgraph *sg = &((gvizEmbeddedGraph *)state)->subgraph;
  size_t distances[gvizGraphSize(sg->g)];
  size_t borderSpan[state->misBorder[count - 2]];
  for (size_t j = 0; j < state->misBorder[count - 2]; j++) {
    borderSpan[j] = SIZE_MAX;
  }

  gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg->g);

  for (size_t i = 0; i < state->misBorder[count - 1]; i++) {
    gvizSubgraphRelease(&bfs);
    bfs = gvizSubgraphCreateEmpty(sg->g);
    gvizSearchBreadthFirst(sg, &bfs, state->misFiltration[i], 0, distances);

    for (size_t j = state->misBorder[count - 1];
         j < state->misBorder[count - 2]; j++) {
      size_t dist = distances[state->misFiltration[j]];
      borderSpan[j] = borderSpan[j] > dist ? dist : borderSpan[j];
    }
  }

  gvizSubgraphRelease(&bfs);

  size_t *maxDistVertex, maxDist = 0;
  for (int i = state->misBorder[count - 1]; i < state->misBorder[count - 2];
       i++) {
    if (maxDist < borderSpan[i]) {
      maxDist = borderSpan[i];
      maxDistVertex = &state->misFiltration[i];
    }
    if (maxDist < borderSpan[i]) {
      maxDist = borderSpan[i];
      maxDistVertex = &state->misFiltration[i];
    }
  }

  xorSwap(maxDistVertex, &state->misFiltration[state->misBorder[count - 1]]);
  state->misBorder[count - 1]++;

  return;
}

static void verticesWithinRadius(gvizGRIPState *state, const gvizSubgraph *sg,
                                 size_t source, size_t maxDepth,
                                 GVIZ_BIT_ARRAY out) {
  size_t nvertices = gvizGraphSize(sg->g);
  size_t *stamp = state->radiusBfsStamp;
  size_t epoch = ++state->radiusBfsEpoch;
  if (epoch == 0) {
    memset(stamp, 0, sizeof(size_t) * nvertices);
    epoch = state->radiusBfsEpoch = 1;
  }

  gvizDeque *queue = &state->radiusBfsQueue;
  queue->count = 0;
  queue->begin = NULL;

  stamp[source] = epoch;

  gvizFoundVertex start = {.v = source, .dist = 0};
  if (gvizDequePush(queue, &start) < 0)
    return;

  while (!gvizDequeIsEmpty(queue)) {
    gvizFoundVertex nd;
    gvizDequePopLeft(queue, &nd);

    size_t curr = nd.v;
    size_t currDepth = nd.dist;

    if (maxDepth && currDepth >= maxDepth)
      continue;

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, curr);
    size_t neighbor;
    while (gvizSubgraphNeighborIterate(&nit, &neighbor)) {
      if (stamp[neighbor] == epoch)
        continue;
      stamp[neighbor] = epoch;

      size_t nextDepth = currDepth + 1;
      if (nextDepth <= maxDepth)
        gvizSetBit(out, neighbor);

      gvizFoundVertex next = {.v = neighbor, .dist = nextDepth};
      if (gvizDequePush(queue, &next) < 0)
        return;
    }
  }
}

int iterMISFiltration(gvizGRIPState *state, size_t i,
                      gvizVertexSubset vertices) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  size_t nvertices = gvizGraphSize(sg->g);

  gvizVertexSubset newVertices = gvizVertexSubsetCreateEmpty(sg->g);
  gvizVertexSubset newMisStates = gvizVertexSubsetCreateEmpty(sg->g);
  if (!newVertices || !newMisStates) {
    if (newVertices)
      gvizVertexSubsetRelease(newVertices);
    if (newMisStates)
      gvizVertexSubsetRelease(newMisStates);
    return 0;
  }

  size_t curr;
  gvizBitArrayIterator it = gvizVertexSubsetIteratorCreate(vertices, nvertices);

  while (gvizBitArrayIterate(&it, &curr)) {
    if (gvizVertexSubsetTest(newMisStates, curr))
      continue;

    gvizVertexSubsetShowVertex(newVertices, curr);
    gvizVertexSubsetShowVertex(newMisStates, curr);

    verticesWithinRadius(state, sg, curr, 1ULL << (i - 1), newMisStates);
  }

  it = gvizVertexSubsetIteratorCreate(vertices, nvertices);
  size_t *ptr = &(state->misFiltration[state->misBorder[i - 1] - 1]);
  while (gvizBitArrayIterate(&it, &curr)) {
    if (!gvizVertexSubsetTest(newVertices, curr))
      *(ptr--) = curr;
  }

  state->misBorder[i] = gvizVertexSubsetCount(newVertices, nvertices);
  gvizBitArrayCopyBits(vertices, newVertices, nvertices);

  gvizVertexSubsetRelease(newVertices);
  gvizVertexSubsetRelease(newMisStates);

  if (state->misBorder[i] > embedding->embedding.dim + 1)
    return 1;
  return 0;
}

size_t createMISFiltration(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  size_t nvertices = gvizGraphSize(sg->g);
  size_t dim = embedding->embedding.dim;

  gvizVertexSubset curr = gvizVertexSubsetCreateEmpty(sg->g);
  if (!curr)
    return 0;

  makeFirstMISPartition(state, curr);

  size_t i = 2;
  while (iterMISFiltration(state, i, curr))
    i++;

  {
    size_t k = 0;
    gvizBitArrayIterator it = gvizVertexSubsetIteratorCreate(curr, nvertices);
    size_t vtx;
    while (gvizBitArrayIterate(&it, &vtx))
      state->misFiltration[k++] = vtx;
  }

  while (state->misBorder[i] < dim + 1)
    migrateOneToFinalLayer(state, curr, i + 1);

  gvizVertexSubsetRelease(curr);

  return i + 1;
}

// TODO: highly parallelizable
// DO NOT CALL FOR FIRST LAYER
void placeLayerVertices(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t layer = state->currLayer;

  // place each new vertex at barrycenter of previous vertices
  for (size_t i = state->misBorder[layer + 1]; i < state->misBorder[layer];
       i++) {
    assert(!gvizVertexSubsetTest(state->placed, state->misFiltration[i]));

    // printf("placing vertex %zu\n", state->misFiltration[i]);
    gvizFoundVertex found[INITIAL_PLACEMENT_K];

    size_t count =
        gvizSearchKNearest(&embedding->subgraph, found,
                                      INITIAL_PLACEMENT_K,
                                      state->misFiltration[i], state->placed);

    assert(count > 0);

    // printf("%d knns found: ", count);
    // for (size_t x = 0; x < count; x++) {
    // printf("(v: %zu, dist: %zu, placed: %d), ", knn[x].v, knn[x].dist,
    // gvizTestBit(placedVertices, knn[x].v));
    // }

    size_t indices[count];

    for (size_t j = 0; j < count; j++) {
      indices[j] = found[j].v * embedding->embedding.dim;
    }

    gvizArray tmp = {indices, sizeof(size_t), count, count};

    // printf("calculating barrycenter\n");
    double pos[embedding->embedding.dim];
    barrycenter(embedding->embedding.dim, embedding->embedding.vertexPositions,
                &tmp, pos);
    gvizEmbeddedGraphSetVPosition(embedding, state->misFiltration[i], pos);
    // printf("barrycenter: (%f, %f)", pos[0], pos[1]);
  }

  // Set all as placed
  for (size_t i = state->misBorder[layer + 1]; i < state->misBorder[layer]; i++)
    gvizVertexSubsetShowVertex(state->placed, state->misFiltration[i]);
}

void updateLocalTemp(gvizGRIPState *state, size_t v) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  double nrm = cblas_dnrm2(embedding->embedding.dim, state->dec[v].disp, 1);
  double oldNrm =
      cblas_dnrm2(embedding->embedding.dim, state->dec[v].oldDisp, 1);

  if (fabs(nrm) < gvizNumericEpsilon || fabs(oldNrm) < gvizNumericEpsilon)
    return;

  double cos = cblas_ddot(embedding->embedding.dim, state->dec[v].disp, 1,
                          state->dec[v].oldDisp, 1) /
               (nrm * oldNrm);

  double heat;

  if (cos > 0 && state->dec[v].oldCos > 0)
    state->dec[v].heat *= (1 + cos * gvizGRIPr * gvizGRIPs);
  else
    state->dec[v].heat *= (1 + cos * gvizGRIPr);

  state->dec[v].oldCos = cos;
}

// TODO: highly parallelizable
void calculateSpringForces(gvizGRIPState *state, size_t v, size_t layer) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  double f[embedding->embedding.dim];
  memset(f, 0, sizeof(f));
  gvizArray *knn = &state->dec[v].knn;

  if (layer > 0) {

    for (size_t i = 0; i < REFINEMENT_K; i++) {
      gvizFoundVertex other = *(gvizFoundVertex *)gvizArrayAtIndex(knn, i);

      gvizPairwiseKKForce(embedding->embedding.dim,
                          gvizEmbeddedGraphGetVPosition(embedding, v),
                          gvizEmbeddedGraphGetVPosition(embedding, other.v),
                          other.dist, GVIZ_EDGE_LENGTH, f);
    }

  } else {

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(&embedding->subgraph, v);
    size_t other;
    while (gvizSubgraphNeighborIterate(&nit, &other)) {
      gvizPairwiseFRRepForce(
          embedding->embedding.dim, gvizEmbeddedGraphGetVPosition(embedding, v),
          gvizEmbeddedGraphGetVPosition(embedding, other), GVIZ_EDGE_LENGTH, f);
    }

    for (size_t i = 0; i < REFINEMENT_K; i++) {
      gvizFoundVertex other = *(gvizFoundVertex *)gvizArrayAtIndex(knn, i);

      gvizPairwiseFRAttForce(embedding->embedding.dim,
                             gvizEmbeddedGraphGetVPosition(embedding, v),
                             gvizEmbeddedGraphGetVPosition(embedding, other.v),
                             GVIZ_EDGE_LENGTH, f);
    }
  }

  // printf("force vector: %f %f\n", f[0], f[1]);

  // disp = force vector
  cblas_dcopy(embedding->embedding.dim, f, 1, state->dec[v].disp, 1);
  return;
}

void clearDecorators(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  memset(state->dispCalculated, 0,
         sizeof(GVIZ_BIT_UNIT) *
             GVIZ_ARRAY_UNITS(gvizGraphSize(embedding->subgraph.g)));

  // TODO: can be made into one memset call. take out all arrays from decorator
  for (size_t i = 0; i < state->misBorder[state->currLayer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;
    dec->heat = 0.0;
    dec->oldCos = 0.0;
    dec->knn.count = 0;
    memset(dec->disp, 0, embedding->embedding.dim * sizeof(double));
    memset(dec->oldDisp, 0, embedding->embedding.dim * sizeof(double));
  }
}

void updateKNNs(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  for (size_t i = 0; i < state->misBorder[state->currLayer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizArray knn = state->dec[curr].knn;
    knn.count =
        gvizSearchKNearest(&embedding->subgraph, knn.arr, REFINEMENT_K,
                                      state->misFiltration[i], state->placed);
  }
}

void gvizGRIPEmbedderBegin(gvizGRIPState *state) {

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  // Step 1: Create a Maximal Independent Set, then filter into coarser subsets
  state->layerCount = createMISFiltration(state);
  state->currLayer = state->layerCount - 1;

  double simplex[embedding->embedding.dim * (embedding->embedding.dim + 1)];
  makeRegularSimplex(embedding->embedding.dim, GVIZ_EDGE_LENGTH * 1000,
                     simplex);

  for (size_t j = 0; j < embedding->embedding.dim + 1; j++) {
    gvizVertexSubsetShowVertex(state->placed, state->misFiltration[j]);
    cblas_dcopy(
        embedding->embedding.dim, simplex + j * embedding->embedding.dim, 1,
        gvizEmbeddedGraphGetVPosition(embedding, state->misFiltration[j]), 1);
  }

  clearDecorators(state);
  updateKNNs(state);
}

// state->placed and state->currLayer assumed to be updated here:

void beginNewStage(gvizGRIPState *state) {

  // TODO: set placed and call updateKNN

  if (state->currLayer == 0)
    return;
  state->currLayer--;
  state->currRound = 0;
  placeLayerVertices(state);
  clearDecorators(state);
  updateKNNs(state);
}

void runRefinementRound(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t layer = state->currLayer;

  printf("Refining layer %zu... round %zu\n", state->currLayer,
         state->currRound);

  // calculate normalized displacements
  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;

    // oldDisp = disp
    cblas_dcopy(embedding->embedding.dim, dec->disp, 1, dec->oldDisp, 1);

    calculateSpringForces(state, curr, layer);

    if (!gvizTestBit(state->dispCalculated, curr)) {
      gvizSetBit(state->dispCalculated, curr);
      dec->heat = GVIZ_EDGE_LENGTH / 6.0;
    } else
      updateLocalTemp(state, curr);

    dec->heat = fmin(dec->heat, GVIZ_EDGE_LENGTH * 10);
    dec->heat = fmax(dec->heat, GVIZ_EDGE_LENGTH * 1e-4);

    double nrm = cblas_dnrm2(embedding->embedding.dim, dec->disp, 1);
    // avoid division by zero here:
    if (nrm > gvizNumericEpsilon) {
      cblas_dscal(embedding->embedding.dim, dec->heat / nrm, dec->disp, 1);
    } else {
      printf("AVOIDED DIVISION BY 0. @ runRefinementRound.\n");
    }
  }

  // update positions, pos = pos + disp
  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;
    gvizEmbeddedGraphAddVPosition(embedding, curr, dec->disp);

    assert(!isnan(gvizEmbeddedGraphGetVPosition(embedding, curr)[0]) &&
           !isnan((gvizEmbeddedGraphGetVPosition(embedding, curr)[1])));
  }

  state->currRound++;
}
