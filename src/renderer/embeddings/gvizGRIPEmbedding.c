#include "cblas.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizDeque.h"
#include "dsa/gvizGraph.h"
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

int gvizGRIPEmbeddingInit(gvizGRIPState *state, gvizGraph *graph,
                          size_t diameter, size_t dimension) {
  int res;
  size_t N = graph->vertices.count;

  res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, graph, dimension);
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
  if (state->dispCalculated)
    GVIZ_DEALLOC(state->dispCalculated);

  return;
}

// NOTE: Can be potentially made better by randomizing the order of the
// vertices. either by shuffling or assigning random priorities to each vertex;
void makeFirstMISPartition(gvizGRIPState *state, gvizArray *out) {
  // creates a maximal independent set of the graph and maintains mis filtration
  // state arrays.

  // 0 undecided
  // 1 decided. could be in/out
  gvizGraph *g = state->graph.graph;
  GVIZ_BIT_UNIT states[GVIZ_ARRAY_UNITS(g->vertices.count)];
  memset(states, 0, sizeof(states));

  size_t *curr = &state->misFiltration[g->vertices.count - 1];

  for (size_t i = 0; i < g->vertices.count; i++) {
    if (!gvizTestBit(states, i)) {
      // IN
      gvizSetBit(states, i);
      gvizArrayPush(out, &i);

      // OUT
      gvizArray *neighbors = gvizGraphGetVertexNeighbors(g, i);
      for (size_t j = 0; j < neighbors->count; j++) {
        size_t neighbor = *(size_t *)gvizArrayAtIndex(neighbors, j);
        if (!gvizTestBit(states, neighbor)) {
          gvizSetBit(states, neighbor);
          *curr-- = neighbor;
        }
      }
    }
  }

  state->misBorder[1] = out->count;
}

void migrateOneToFinalLayer(gvizGRIPState *state, GVIZ_BIT_ARRAY finalLayer,
                            size_t count) {
  size_t distances[state->misBorder[count - 2]];
  for (size_t j = 0; j < state->misBorder[count - 2]; j++) {
    distances[j] = SIZE_MAX;
  }

  gvizGraph bfs;

  for (size_t i = 0; i < state->misBorder[count - 1]; i++) {
    // NOTE: No max depth. place for optimization here
    printf("  source vertex: %zu (graph size: %zu)\n", state->misFiltration[i],
           ((gvizEmbeddedGraph *)state)->graph->vertices.count);

    // last parameter specifies we want a bfs.map that maps from original idx
    // to bfs idx, instead of the other way around.
    gvizGraphBFSTree(((gvizEmbeddedGraph *)state)->graph, &bfs,
                     state->misFiltration[i], 0, 1);

    for (size_t j = state->misBorder[count - 1];
         j < state->misBorder[count - 2]; j++) {

      size_t vertex = bfs.map[state->misFiltration[j]];

      // interpret the pointer as a size_t, read the code for BFS.
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

void verticesWithinRadius(gvizGraph *g, size_t source, size_t maxDepth,
                          GVIZ_BIT_ARRAY out) {
  int err;

  // init queue
  gvizDeque queue;
  err = gvizDequeInitAtCapacity(&queue, sizeof(gvizFoundVertex), maxDepth * 2);
  if (err < 0) {
    return;
  }

  // enqueue start node with depth 0
  gvizFoundVertex start = {.v = source, .dist = 0};
  err = gvizDequePush(&queue, &start);
  if (err < 0)
    return;

  // BFS
  while (!gvizDequeIsEmpty(&queue)) {
    gvizFoundVertex nd;
    gvizDequePopLeft(&queue, &nd);

    size_t curr = nd.v;
    size_t currDepth = nd.dist;

    // if we've reached maxDepth, don't expand this node further
    if (maxDepth && currDepth >= maxDepth)
      continue;

    gvizArray *currNeighbors = gvizGraphGetVertexNeighbors(g, curr);

    for (size_t i = 0; i < currNeighbors->count; i++) {
      size_t currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);

      // seen keyed by vertex ID
      if (gvizTestBit(out, currNeighbor))
        continue;
      gvizSetBit(out, currNeighbor);

      // enqueue neighbor with depth+1
      gvizFoundVertex next = {.v = currNeighbor, .dist = currDepth + 1};
      err = gvizDequePush(&queue, &next);
      if (err < 0)
        return;
    }
  }
  gvizDequeRelease(&queue);
}

int iterMISFiltration(gvizGRIPState *state, size_t i, GVIZ_BIT_ARRAY vertices,
                      gvizArray *verticesArr) {

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  GVIZ_BIT_UNIT newVertices[GVIZ_ARRAY_UNITS(embedding->graph->vertices.count)];
  GVIZ_BIT_UNIT
  newMisStates[GVIZ_ARRAY_UNITS(embedding->graph->vertices.count)];
  size_t curr;
  memset(newVertices, 0, sizeof(newVertices));
  memset(newMisStates, 0, sizeof(newMisStates));

  printf("creating new layer %zu\n", i);

  for (size_t j = 0; j < verticesArr->count; j++) {
    curr = *(size_t *)gvizArrayAtIndex(verticesArr, j);

    // already chosen or too close to a prev choice
    if (gvizTestBit(newMisStates, curr))
      continue;

    // chosen for next layer
    gvizSetBit(newVertices, curr);
    gvizSetBit(newMisStates, curr);

    // Any vertex that is too close cannot be chosen
    verticesWithinRadius(embedding->graph, curr, (1ULL << (i - 1)) + 1,
                         newMisStates);
  }

  printf("second loop\n");

  // Loop again, to find the vertices that did not make it to the new subset
  // We loop backwards since we are mutating the array we are looping over.
  // The way this is done, it is safe.
  size_t *ptr = &(state->misFiltration[state->misBorder[i - 1] - 1]);
  for (size_t j = verticesArr->count; j-- > 0;) {
    curr = *(size_t *)gvizArrayAtIndex(verticesArr, j);
    if (!gvizTestBit(newVertices, curr)) {
      // printf("writing to mistfiltration array, curr - arr = %zu\n",
      // ptr - state->misFiltration);
      // Write to misFiltration array
      *(ptr--) = curr;

      // printf("attempting delete %zu\n", j);
      // Update verticesArr. O(1). not order-preserving
      gvizArraySwapDelete(verticesArr, j);
    }
  }

  state->misBorder[i] = verticesArr->count;

  memcpy(vertices, newVertices, sizeof(newVertices));

  // write current MIS vertices into misFiltration before migrating
  for (size_t k = 0; k < verticesArr->count; k++) {
    state->misFiltration[k] = *(size_t *)gvizArrayAtIndex(verticesArr, k);
  }

  while (state->misBorder[i] < embedding->embedding.dim + 1) {
    migrateOneToFinalLayer(state, newVertices, i + 1);
  }

  printf("Layer %zu created with %zu vertices.", i, state->misBorder[i]);

  if (state->misBorder[i] == embedding->embedding.dim + 1) {
    for (size_t k = 0; k < verticesArr->count; k++) {
      state->misFiltration[k] = *(size_t *)gvizArrayAtIndex(verticesArr, k);
    }
    return 0;
  } else
    return 1;
}

size_t createMISFiltration(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  GVIZ_BIT_UNIT curr[GVIZ_ARRAY_UNITS(embedding->graph->vertices.count)];
  memset(curr, 0, sizeof(curr));

  printf("creating first MIS\n");
  gvizArray currVertices;
  gvizArrayInitAtCapacity(&currVertices, sizeof(size_t),
                          embedding->graph->vertices.count);

  makeFirstMISPartition(state, &currVertices);
  printf("MIS created, size: %zu. setting bitset\n", currVertices.count);
  for (size_t i = 0; i < currVertices.count; i++) {
    gvizSetBit(curr, *(size_t *)gvizArrayAtIndex(&currVertices, i));
  }

  printf("creating next layers...\n");
  size_t i = 2;
  while (iterMISFiltration(state, i, curr, &currVertices)) {
    i++;
  }

  gvizArrayRelease(&currVertices);

  return i + 1;
}

void placeLayerVertices(gvizGRIPState *state, size_t layer,
                        GVIZ_BIT_ARRAY placedVertices) {
  printf("placing layer using barrycenters\n");
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  // place each new vertex at barrycenter of previous vertices
  for (size_t i = state->misBorder[layer + 1]; i < state->misBorder[layer];
       i++) {
    assert(!gvizTestBit(placedVertices, state->misFiltration[i]));

    // printf("placing vertex %zu\n", state->misFiltration[i]);
    gvizFoundVertex found[32];

    size_t count = gvizGraphKNearestNeighbors(
        embedding->graph, found, 32, state->misFiltration[i], placedVertices);

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

  double heat;

  if (cos > 0 && state->dec[v].oldCos > 0)
    state->dec[v].heat *= (1 + cos * gvizGRIPr * gvizGRIPs);
  else
    state->dec[v].heat *= (1 + cos * gvizGRIPr);

  state->dec[v].oldCos = cos;
}

void calculateSpringForces(gvizGRIPState *state, size_t v, gvizArray *knn,
                           size_t layer, GVIZ_BIT_ARRAY filter) {
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

    gvizArray *neighbors = gvizGraphGetVertexNeighbors(embedding->graph, v);
    for (size_t i = 0; i < neighbors->count; i++) {
      size_t other = *(size_t *)gvizArrayAtIndex(neighbors, i);

      gvizPairwiseFRRepForce(
          embedding->embedding.dim, gvizEmbeddedGraphGetVPosition(embedding, v),
          gvizEmbeddedGraphGetVPosition(embedding, other), GVIZ_EDGE_LENGTH, f);
    }

    for (size_t i = 0; i < knn->count; i++) {
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

void refineGRIPPositions(gvizGRIPState *state, size_t layer,
                         GVIZ_BIT_ARRAY placedVertices) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  memset(state->dispCalculated, 0,
         sizeof(GVIZ_BIT_UNIT) *
             GVIZ_ARRAY_UNITS(embedding->graph->vertices.count));

  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    size_t curr = state->misFiltration[i];
    gvizGRIPDecorators *dec = state->dec + curr;
    dec->heat = 0.0;
    dec->oldCos = 0.0;
    memset(dec->disp, 0, embedding->embedding.dim * sizeof(double));
    memset(dec->oldDisp, 0, embedding->embedding.dim * sizeof(double));
  }

  gvizArray *knns = GVIZ_ALLOC(sizeof(gvizArray) * state->misBorder[layer]);
  // gvizArray knns[state->misBorder[layer]];
  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    gvizArrayInitAtCapacity(&knns[i], sizeof(gvizFoundVertex), 128);
    knns[i].count =
        gvizGraphKNearestNeighbors(embedding->graph, knns[i].arr, 128,
                                   state->misFiltration[i], placedVertices);
  }

  // TODO: implement rounds instead of hardcoding
  for (size_t r = 0; r < 36; r++) {
    printf("Refining layer %zu... round %zu\n", layer, r);
    // calculate normalized displacements
    for (size_t i = 0; i < state->misBorder[layer]; i++) {
      size_t curr = state->misFiltration[i];
      gvizGRIPDecorators *dec = state->dec + curr;

      // oldDisp = disp
      cblas_dcopy(embedding->embedding.dim, dec->disp, 1, dec->oldDisp, 1);

      calculateSpringForces(state, curr, knns + i, layer, placedVertices);

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
      }
    }

    size_t nonzero, zero;
    zero = nonzero = 0;

    // update positions, pos = pos + disp
    for (size_t i = 0; i < state->misBorder[layer]; i++) {
      size_t curr = state->misFiltration[i];
      gvizGRIPDecorators *dec = state->dec + curr;
      gvizEmbeddedGraphAddVPosition(embedding, curr, dec->disp);

      double nrm = cblas_dnrm2(embedding->embedding.dim, dec->disp, 1);
      if (nrm > gvizNumericEpsilon)
        nonzero++;
      else
        zero++;

      assert(!isnan(gvizEmbeddedGraphGetVPosition(embedding, curr)[0]) &&
             !isnan((gvizEmbeddedGraphGetVPosition(embedding, curr)[1])));
    }

    // printf("layer %zu, round %zu. zero vector disp count: %zu, non-zero
    // vector "
    //        "disp count: %zu\n",
    // layer, r, zero, nonzero);
  }

  // clean up
  for (size_t i = 0; i < state->misBorder[layer]; i++) {
    gvizArrayRelease(&knns[i]);
  }
  GVIZ_DEALLOC(knns);
}

int gvizGRIPRefineEmbedding(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  GVIZ_BIT_UNIT placed[GVIZ_ARRAY_UNITS(
      ((gvizEmbeddedGraph *)state)->graph->vertices.count)];
  memset(placed, 0, sizeof(placed));

  // Step 1: Create a Maximal Independent Set, then filter into coarser subsets
  size_t layerCount = createMISFiltration(state);

  // Vertices already placed. Only refine and maintain placed bitest./

  // initialize with the smallest layer
  for (size_t i = 0; i < state->misBorder[layerCount - 1]; i++) {
    gvizSetBit(placed, state->misFiltration[i]);
  }

  for (size_t i = layerCount; i-- > 0;) {

    // update placed vertices to include the new layer's vertices
    if (i != layerCount - 1) {
      for (size_t j = state->misBorder[i + 1]; j < state->misBorder[i]; j++) {
        gvizSetBit(placed, state->misFiltration[j]);
      }
    }

    // printf("placing layer %zu\n", i);

    // printf("simulating springs \n");
    refineGRIPPositions(state, i, placed);
  }
  // for (size_t i = 0; i < embedding->graph->vertices.count; i++) {
  //   printf("Vertex #%zu, placed: %d, position (%lf, %lf)\n", i,
  //          gvizTestBit(placed, i), state->dec[i].disp[0],
  //          state->dec[i].disp[i + 1]);
  // }
  //
  return 0;
}

int gvizGRIPEmbeddingEmbed(gvizGRIPState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  GVIZ_BIT_UNIT placed[GVIZ_ARRAY_UNITS(
      ((gvizEmbeddedGraph *)state)->graph->vertices.count)];
  memset(placed, 0, sizeof(placed));

  // Step 1: Create a Maximal Independent Set, then filter into coarser subsets
  size_t layerCount = createMISFiltration(state);

  // Step 2: Place the smallest layers first, and perform local force simulation

  for (size_t i = layerCount; i-- > 0;) {
    // printf("placing layer %zu\n", i);

    if (i == layerCount - 1) {
      printf("placing first layer. vertices: %zu\n",
             state->misBorder[layerCount - 1]);
      // first layer placement
      double simplex[embedding->embedding.dim * (embedding->embedding.dim + 1)];
      makeRegularSimplex(embedding->embedding.dim, GVIZ_EDGE_LENGTH * 1000,
                         simplex);
      // makeEquilateralTriangle2(triangle, GVIZ_EDGE_LENGTH * 1000);
      for (size_t j = 0; j < embedding->embedding.dim + 1; j++) {
        gvizSetBit(placed, state->misFiltration[j]);
        cblas_dcopy(
            embedding->embedding.dim, simplex + j * embedding->embedding.dim, 1,
            gvizEmbeddedGraphGetVPosition(embedding, state->misFiltration[j]),
            1);
      }
    } else
      placeLayerVertices(state, i, placed); // barrycenter

    // printf("simulating springs \n");
    refineGRIPPositions(state, i, placed);
  }
  // for (size_t i = 0; i < embedding->graph->vertices.count; i++) {
  //   printf("Vertex #%zu, placed: %d, position (%lf, %lf)\n", i,
  //          gvizTestBit(placed, i), state->dec[i].disp[0],
  //          state->dec[i].disp[i + 1]);
  // }
  //
  return 0;
}
