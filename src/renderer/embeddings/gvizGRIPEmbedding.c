#include "cblas.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizDeque.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizGraphView.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizForceDirected.h"
#include "utils/helpers.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_EDGE_LENGTH 10.0

static const double gvizGRIPr = 0.15;
static const double gvizGRIPs = 3;

int gvizGRIPEmbeddingInitView(gvizGRIPState *state, gvizGraphView view,
                              size_t diameter, size_t dimension) {
  int res;
  size_t N = view.graph->vertices.count;

  res = gvizEmbeddedGraphInitView((gvizEmbeddedGraph *)state, view,
                                  GVIZ_EMBED_FULL_GRAPH, dimension);
  if (res < 0)
    return res;

  size_t tmp = 1024;
  if (diameter) {
    tmp = (size_t)log2(diameter) + 8;
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

  double *disp = GVIZ_ALLOC(sizeof(double) * N * dimension);
  double *oldDisp = GVIZ_ALLOC(sizeof(double) * N * dimension);

  if (!disp || !oldDisp)
    return -1;

  for (size_t i = 0; i < N; i++) {
    state->dec[i].disp = disp;
    state->dec[i].oldDisp = oldDisp;
    disp += dimension;
    oldDisp += dimension;
  }

  return 0;
}

void makeRegularSimplex(size_t n, double side_length, double *out) {
  memset(out, 0, sizeof(double) * (n + 1) * n);

  for (size_t k = 0; k <= n; k++) {
    double *vk = out + k * n;

    if (k == 0) {
      vk[0] = 1.0;
      continue;
    }

    double c = -1.0 / (double)n;

    for (size_t j = 0; j < k; j++) {
      double *vj = out + j * n;
      double dot_so_far = 0.0;
      for (size_t m = 0; m < j; m++)
        dot_so_far += vk[m] * vj[m];
      vk[j] = (c - dot_so_far) / vj[j];
    }

    double norm_sq = 0.0;
    for (size_t m = 0; m < k; m++)
      norm_sq += vk[m] * vk[m];
    if (k < n) {
      double rem = 1.0 - norm_sq;
      vk[k] = rem > 0.0 ? sqrt(rem) : 0.0;
    }
  }

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

  *out++ = 0;
  *out++ = R;

  *out++ = -sideLength / 2;
  *out++ = -R / 2;

  *out++ = sideLength / 2;
  *out = -R / 2;
}

void gvizGRIPEmbeddingRelease(gvizGRIPState *state) {
  gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)state);
  if (state->misBorder)
    GVIZ_DEALLOC(state->misBorder);
  if (state->misFiltration)
    GVIZ_DEALLOC(state->misFiltration);
  if (state->dec) {
    GVIZ_DEALLOC(state->dec[0].disp);
    GVIZ_DEALLOC(state->dec[0].oldDisp);
    GVIZ_DEALLOC(state->dec);
  }
  return;
}

void makeFirstMISPartition(gvizGRIPState *state, GVIZ_BIT_ARRAY firstLayer) {
  gvizGraphView *view = &state->graph.view;
  size_t N = view->graph->vertices.count;
  GVIZ_BIT_UNIT states[GVIZ_ARRAY_UNITS(N)];
  memset(states, 0, sizeof(states));

  size_t *curr = &state->misFiltration[N - 1];
  size_t misCount = 0;

  gvizGraphViewVertexIter vit;
  gvizGraphViewVertexIterInit(&vit, view);
  size_t i;
  while (gvizGraphViewVertexIterNext(&vit, &i)) {
    if (gvizTestBit(states, i))
      continue;

    gvizSetBit(states, i);
    gvizSetBit(firstLayer, i);
    misCount++;

    gvizGraphViewNeighborsIter nit;
    gvizGraphViewNeighborsIterInit(&nit, view, i);
    size_t neighbor;
    while (gvizGraphViewNeighborsIterNext(&nit, &neighbor)) {
      if (!gvizTestBit(states, neighbor)) {
        gvizSetBit(states, neighbor);
        *curr-- = neighbor;
      }
    }
    gvizGraphViewNeighborsIterRelease(&nit);
  }
  gvizGraphViewVertexIterRelease(&vit);

  state->misBorder[1] = misCount;
}

void migrateOneToFinalLayer(gvizGRIPState *state, GVIZ_BIT_ARRAY finalLayer,
                            size_t count) {
  size_t distances[state->misBorder[count - 2]];
  for (size_t j = 0; j < state->misBorder[count - 2]; j++) {
    distances[j] = SIZE_MAX;
  }

  gvizGraph bfs;

  for (size_t i = 0; i < state->misBorder[count - 1]; i++) {
    printf("  source vertex: %zu (graph size: %zu)\n", state->misFiltration[i],
           ((gvizEmbeddedGraph *)state)->view.graph->vertices.count);

    gvizGraphBFSTree(((gvizEmbeddedGraph *)state)->view.graph, &bfs,
                     state->misFiltration[i], 0, 1);

    for (size_t j = state->misBorder[count - 1];
         j < state->misBorder[count - 2]; j++) {

      size_t vertex = bfs.map[state->misFiltration[j]];

      size_t dist = (size_t)gvizGraphGetVertexData(&bfs, vertex);
      distances[j] = distances[j] > dist ? dist : distances[j];
    }

    gvizGraphRelease(&bfs);
  }

  size_t *maxDistVertex, maxDist = 0;
  for (int i = state->misBorder[count - 1]; i < state->misBorder[count - 2];
       i++) {
    if (maxDist < distances[i]) {
      maxDist = distances[i];
      maxDistVertex = &state->misFiltration[i];
    }
    if (maxDist < distances[i]) {
      maxDist = distances[i];
      maxDistVertex = &state->misFiltration[i];
    }
  }

  xorSwap(maxDistVertex, &state->misFiltration[state->misBorder[count - 1]]);
  state->misBorder[count - 1]++;

  return;
}

static void verticesWithinRadiusView(gvizGraphView *view, size_t source,
                                     size_t maxDepth, GVIZ_BIT_ARRAY out) {
  int err;

  gvizDeque queue;
  err = gvizDequeInitAtCapacity(&queue, sizeof(gvizFoundVertex), maxDepth * 2);
  if (err < 0)
    return;

  gvizFoundVertex start = {.v = source, .dist = 0};
  err = gvizDequePush(&queue, &start);
  if (err < 0)
    return;

  while (!gvizDequeIsEmpty(&queue)) {
    gvizFoundVertex nd;
    gvizDequePopLeft(&queue, &nd);

    size_t curr = nd.v;
    size_t currDepth = nd.dist;

    if (maxDepth && currDepth >= maxDepth)
      continue;

    gvizGraphViewNeighborsIter nit;
    gvizGraphViewNeighborsIterInit(&nit, view, curr);
    size_t currNeighbor;
    while (gvizGraphViewNeighborsIterNext(&nit, &currNeighbor)) {
      if (gvizTestBit(out, currNeighbor))
        continue;
      gvizSetBit(out, currNeighbor);

      gvizFoundVertex next = {.v = currNeighbor, .dist = currDepth + 1};
      err = gvizDequePush(&queue, &next);
      if (err < 0) {
        gvizGraphViewNeighborsIterRelease(&nit);
        gvizDequeRelease(&queue);
        return;
      }
    }
    gvizGraphViewNeighborsIterRelease(&nit);
  }
  gvizDequeRelease(&queue);
}

int iterMISFiltration(gvizGRIPState *state, size_t i,
                      GVIZ_BIT_ARRAY currLayer) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t N = embedding->view.graph->vertices.count;
  GVIZ_BIT_UNIT newVertices[GVIZ_ARRAY_UNITS(N)];
  GVIZ_BIT_UNIT newMisStates[GVIZ_ARRAY_UNITS(N)];
  memset(newVertices, 0, sizeof(newVertices));
  memset(newMisStates, 0, sizeof(newMisStates));

  printf("creating new layer %zu\n", i);

  size_t *ptr = &(state->misFiltration[state->misBorder[i - 1] - 1]);
  size_t newCount = 0;

  gvizBitArrayIter it;
  gvizBitArrayIterInit(&it, currLayer, N);
  size_t curr;
  while (gvizBitArrayIterNext(&it, &curr)) {
    if (gvizTestBit(newMisStates, curr))
      continue;

    gvizSetBit(newVertices, curr);
    gvizSetBit(newMisStates, curr);
    newCount++;

    verticesWithinRadiusView(&embedding->view, curr, (1ULL << (i - 1)) + 1,
                             newMisStates);
  }
  gvizBitArrayIterRelease(&it);

  printf("second loop\n");

  gvizBitArrayIter dropIt;
  gvizBitArrayIterInit(&dropIt, currLayer, N);
  while (gvizBitArrayIterNext(&dropIt, &curr)) {
    if (!gvizTestBit(newVertices, curr)) {
      *(ptr--) = curr;
      gvizClearBit(currLayer, curr);
    }
  }
  gvizBitArrayIterRelease(&dropIt);

  state->misBorder[i] = newCount;

  memcpy(currLayer, newVertices, sizeof(newVertices));

  {
    gvizBitArrayIter headIt;
    gvizBitArrayIterInit(&headIt, currLayer, N);
    size_t k = 0;
    size_t v;
    while (gvizBitArrayIterNext(&headIt, &v)) {
      state->misFiltration[k++] = v;
    }
    gvizBitArrayIterRelease(&headIt);
  }

  while (state->misBorder[i] < embedding->embedding.dim + 1) {
    migrateOneToFinalLayer(state, newVertices, i + 1);
  }

  printf("Layer %zu created with %zu vertices.", i, state->misBorder[i]);

  if (state->misBorder[i] == embedding->embedding.dim + 1)
    return 0;
  return 1;
}

size_t createMISFiltration(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t N = embedding->view.graph->vertices.count;

  printf("creating first MIS\n");
  GVIZ_BIT_ARRAY currLayer = gvizBitArrayAlloc(N);

  makeFirstMISPartition(state, currLayer);
  printf("MIS created, size: %zu.\n", gvizBitArrayCount(currLayer, N));
  {
    gvizBitArrayIter headIt;
    gvizBitArrayIterInit(&headIt, currLayer, N);
    size_t k = 0;
    size_t v;
    while (gvizBitArrayIterNext(&headIt, &v)) {
      state->misFiltration[k++] = v;
    }
    gvizBitArrayIterRelease(&headIt);
  }

  printf("creating next layers...\n");
  size_t i = 2;
  while (iterMISFiltration(state, i, currLayer)) {
    i++;
  }

  gvizBitArrayFree(currLayer);

  return i + 1;
}

void placeLayerVertices(gvizGRIPState *state, size_t layer,
                        GVIZ_BIT_ARRAY placedVertices) {
  printf("placing layer using barrycenters\n");
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  for (size_t i = state->misBorder[layer + 1]; i < state->misBorder[layer];
       i++) {
    assert(!gvizTestBit(placedVertices, state->misFiltration[i]));

    gvizFoundVertex found[8];

    size_t count = gvizGraphViewKNearestNeighbors(
        &embedding->view, found, 8, state->misFiltration[i], placedVertices);

    assert(count > 0);

    size_t indices[count];

    for (size_t j = 0; j < count; j++) {
      indices[j] = found[j].v * embedding->embedding.dim;
    }

    gvizArray tmp = {indices, sizeof(size_t), count, count};

    double pos[embedding->embedding.dim];
    barrycenter(embedding->embedding.dim, embedding->embedding.vertexPositions,
                &tmp, pos);
    gvizEmbeddedGraphSetVPosition(embedding, state->misFiltration[i], pos);
  }

  for (size_t i = state->misBorder[layer + 1]; i < state->misBorder[layer]; i++)
    gvizSetBit(placedVertices, state->misFiltration[i]);
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

  if (cos > 0 && state->dec[v].oldCos > 0)
    state->dec[v].heat *= (1 + cos * gvizGRIPr * gvizGRIPs);
  else
    state->dec[v].heat *= (1 + cos * gvizGRIPr);

  state->dec[v].oldCos = cos;
}

void calculateSpringForces(gvizGRIPState *state, size_t v, gvizArray *knn,
                           size_t layer) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  double f[embedding->embedding.dim];
  memset(f, 0, sizeof(f));
  if (layer > 0) {

    for (size_t i = 0; i < knn->count; i++) {
      gvizFoundVertex other = *(gvizFoundVertex *)gvizArrayAtIndex(knn, i);

      gvizPairwiseKKForce(embedding->embedding.dim,
                          gvizEmbeddedGraphGetVPosition(embedding, v),
                          gvizEmbeddedGraphGetVPosition(embedding, other.v),
                          other.dist, GVIZ_EDGE_LENGTH, f);
    }

  } else {

    gvizGraphViewNeighborsIter nit;
    gvizGraphViewNeighborsIterInit(&nit, &embedding->view, v);
    size_t other;
    while (gvizGraphViewNeighborsIterNext(&nit, &other)) {
      gvizPairwiseFRRepForce(
          embedding->embedding.dim, gvizEmbeddedGraphGetVPosition(embedding, v),
          gvizEmbeddedGraphGetVPosition(embedding, other), GVIZ_EDGE_LENGTH, f);
    }
    gvizGraphViewNeighborsIterRelease(&nit);

    for (size_t i = 0; i < knn->count; i++) {
      gvizFoundVertex kother = *(gvizFoundVertex *)gvizArrayAtIndex(knn, i);

      gvizPairwiseFRAttForce(embedding->embedding.dim,
                             gvizEmbeddedGraphGetVPosition(embedding, v),
                             gvizEmbeddedGraphGetVPosition(embedding, kother.v),
                             GVIZ_EDGE_LENGTH, f);
    }
  }

  cblas_dcopy(embedding->embedding.dim, f, 1, state->dec[v].disp, 1);
  return;
}

gvizArray *gvizGRIPPrepareLayerKNNs(gvizGRIPState *state, size_t layer,
                                    GVIZ_BIT_UNIT *placed) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;
    dec->heat = 0.0;
    dec->oldCos = 0.0;
    dec->initialized = 0;
    memset(dec->disp, 0, embedding->embedding.dim * sizeof(double));
    memset(dec->oldDisp, 0, embedding->embedding.dim * sizeof(double));
  }

  gvizArray *knns = GVIZ_ALLOC(sizeof(gvizArray) * state->misBorder[layer]);
  if (!knns)
    return NULL;
  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    gvizArrayInitAtCapacity(&knns[i], sizeof(gvizFoundVertex), 32);
    knns[i].count = gvizGraphViewKNearestNeighbors(
        &embedding->view, knns[i].arr, 32, state->misFiltration[i], placed);
  }
  return knns;
}

void gvizGRIPRefineOneRound(gvizGRIPState *state, size_t layer,
                            gvizArray *knns) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;

    cblas_dcopy(embedding->embedding.dim, dec->disp, 1, dec->oldDisp, 1);

    calculateSpringForces(state, curr, knns + i, layer);

    if (!dec->initialized) {
      dec->initialized = 1;
      dec->heat = GVIZ_EDGE_LENGTH / 6.0;
    } else
      updateLocalTemp(state, curr);

    dec->heat = fmin(dec->heat, GVIZ_EDGE_LENGTH * 10);
    dec->heat = fmax(dec->heat, GVIZ_EDGE_LENGTH * 1e-4);

    double nrm = cblas_dnrm2(embedding->embedding.dim, dec->disp, 1);
    if (nrm > gvizNumericEpsilon) {
      cblas_dscal(embedding->embedding.dim, dec->heat / nrm, dec->disp, 1);
    }
  }

  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;
    gvizEmbeddedGraphAddVPosition(embedding, curr, dec->disp);

    assert(!isnan(gvizEmbeddedGraphGetVPosition(embedding, curr)[0]) &&
           !isnan((gvizEmbeddedGraphGetVPosition(embedding, curr)[1])));
  }
}

void gvizGRIPReleaseLayerKNNs(gvizGRIPState *state, size_t layer,
                              gvizArray *knns) {
  if (!knns)
    return;
  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    gvizArrayRelease(&knns[i]);
  }
  GVIZ_DEALLOC(knns);
}

void refineGRIPPositions(gvizGRIPState *state, size_t layer,
                         GVIZ_BIT_ARRAY placedVertices) {
  gvizArray *knns = gvizGRIPPrepareLayerKNNs(state, layer, placedVertices);
  if (!knns)
    return;
  for (size_t r = 0; r < 24; r++) {
    gvizGRIPRefineOneRound(state, layer, knns);
  }
  gvizGRIPReleaseLayerKNNs(state, layer, knns);
}

int gvizGRIPRefineEmbedding(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  GVIZ_BIT_UNIT placed[GVIZ_ARRAY_UNITS(
      ((gvizEmbeddedGraph *)state)->view.graph->vertices.count)];
  memset(placed, 0, sizeof(placed));

  size_t layerCount = createMISFiltration(state);

  for (size_t i = 0; i < state->misBorder[layerCount - 1]; i++) {
    gvizSetBit(placed, state->misFiltration[i]);
  }

  for (size_t i = layerCount; i-- > 0;) {

    if (i != layerCount - 1) {
      for (size_t j = state->misBorder[i + 1]; j < state->misBorder[i]; j++) {
        gvizSetBit(placed, state->misFiltration[j]);
      }
    }

    refineGRIPPositions(state, i, placed);
  }
  return 0;
}

int gvizGRIPEmbeddingEmbed(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  GVIZ_BIT_UNIT placed[GVIZ_ARRAY_UNITS(
      ((gvizEmbeddedGraph *)state)->view.graph->vertices.count)];
  memset(placed, 0, sizeof(placed));

  size_t layerCount = createMISFiltration(state);

  for (size_t i = layerCount; i-- > 0;) {

    if (i == layerCount - 1) {
      printf("placing first layer. vertices: %zu\n",
             state->misBorder[layerCount - 1]);
      double simplex[embedding->embedding.dim * (embedding->embedding.dim + 1)];
      makeRegularSimplex(embedding->embedding.dim, GVIZ_EDGE_LENGTH * 10000,
                         simplex);
      for (size_t j = 0; j < embedding->embedding.dim + 1; j++) {
        gvizSetBit(placed, state->misFiltration[j]);
        cblas_dcopy(
            embedding->embedding.dim, simplex + j * embedding->embedding.dim, 1,
            gvizEmbeddedGraphGetVPosition(embedding, state->misFiltration[j]),
            1);
      }
    } else
      placeLayerVertices(state, i, placed);

    refineGRIPPositions(state, i, placed);
  }
  return 0;
}
