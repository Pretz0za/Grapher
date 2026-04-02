#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "helpers.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "utils/serializers.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int gvizGRIPEmbeddingInit(gvizGRIPState *state, gvizGraph *graph,
                          size_t diameter) {
  int res;
  size_t N = graph->vertices.count;

  res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, graph);
  if (res < 0)
    return res;

  size_t tmp = 1024;
  if (diameter) {
    tmp = (size_t)log2(diameter) + 1;
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

  return 0;
}

void gvizGRIPEmbeddingRelease(gvizGRIPState *state) {
  gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)state);
  if (state->misBorder)
    GVIZ_DEALLOC(state->misBorder);
  if (state->misFiltration)
    GVIZ_DEALLOC(state->misFiltration);
  if (state->dec)
    GVIZ_DEALLOC(state->dec);
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
  gvizArrayInit(out, sizeof(size_t));

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

int iterMISFiltration(gvizGRIPState *state, size_t i, GVIZ_BIT_ARRAY vertices,
                      gvizArray *verticesArr) {

  assert(i != 1); // special case

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizGraph bfs;
  GVIZ_BIT_UNIT newVertices[GVIZ_ARRAY_UNITS(embedding->graph->vertices.count)];
  GVIZ_BIT_UNIT
  newMisStates[GVIZ_ARRAY_UNITS(embedding->graph->vertices.count)];
  size_t curr;
  memset(newVertices, 0, sizeof(newVertices));
  memset(newMisStates, 0, sizeof(newMisStates));

  printf("creating new layer %zu\n", i);

  // NOTE: may be able to optimize by reusing bfs buffer to avoid allocation
  // and free overhead.
  for (size_t j = 0; j < verticesArr->count; j++) {
    curr = *(size_t *)gvizArrayAtIndex(verticesArr, j);

    // already chosen or too close to a prev choice
    if (gvizTestBit(newMisStates, curr))
      continue;

    // chosen for next layer
    gvizSetBit(newMisStates, curr);
    gvizSetBit(newVertices, curr);

    // bfs at each vertex at the previous layer, max depth = 2^(prev layer idx)
    // vertices that are reached are too close to be chosen for next layer
    gvizGraphBFSTree(((gvizEmbeddedGraph *)state)->graph, &bfs, curr,
                     (1ULL << (i - 1)) + 1, 0);

    // from bfs results, only keep vertices that are also in the previous layer
    for (size_t k = 0; k < bfs.vertices.count; k++) {
      // Set as non-choosable
      gvizSetBit(newMisStates, bfs.map[k]);
    }

    // destory bfs tree immediately after use. this is defining of GRIP
    gvizGraphRelease(&bfs);
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

  while (state->misBorder[i] < 3) {
    printf("MIGRATING ONE\n");
    migrateOneToFinalLayer(state, newVertices, i + 1);
  }

  printf("filtered layer. new layer size: %zu\n", state->misBorder[i]);

  if (state->misBorder[i] == 3) {
    for (int i = 0; i < verticesArr->count; i++) {
      state->misFiltration[i] = *(size_t *)gvizArrayAtIndex(verticesArr, i);
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

  return i + 1;
}

int gvizGRIPEmbeddingEmbed(gvizGRIPState *state);
