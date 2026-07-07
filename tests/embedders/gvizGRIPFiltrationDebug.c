#include "algorithms/search/gvizBreadthFirst.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "gvizGRIPEmbedderInternal.h"
#include "utils/graphLoader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t gripMisBorderAt(const gvizGRIPState *state, size_t i) {
  return *(const size_t *)gvizArrayAtIndex(&state->misBorder, i);
}

static size_t countLayerVertices(const gvizGRIPState *state, size_t layer,
                                 size_t nvertices) {
  size_t end = gripMisBorderAt(state, layer);
  size_t cnt = 0;
  for (size_t i = 0; i < end; i++) {
    if (state->misFiltration[i] < nvertices)
      cnt++;
  }
  return cnt;
}

static void printGraphStats(const gvizGraph *g) {
  size_t n = gvizGraphSize(g);
  size_t minDeg = SIZE_MAX, maxDeg = 0;
  unsigned long long degSum = 0;
  for (size_t i = 0; i < n; i++) {
    size_t d = gvizGraphGetVertexNeighbors(g, i)->count;
    if (d < minDeg)
      minDeg = d;
    if (d > maxDeg)
      maxDeg = d;
    degSum += d;
  }
  printf("graph: %zu vertices, layout edges %zu, directed=%d\n", n,
         gvizGraphEdgeCount(g), gvizGraphIsDirected(g));
  printf("degree: min=%zu max=%zu avg=%.2f\n", minDeg, maxDeg,
         (double)degSum / (double)n);
}

static void probeBfsReach(const gvizSubgraph *sg, size_t src, size_t maxDepth) {
  size_t n = gvizGraphSize(sg->g);
  size_t *dist = malloc(n * sizeof(size_t));
  if (!dist)
    return;
  gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg->g);
  gvizSearchBreadthFirst(sg, &bfs, src, maxDepth, dist);
  size_t reached = gvizSubgraphVertexCount(&bfs);
  gvizSubgraphRelease(&bfs);
  printf("  BFS from vtx %zu depth %zu: reached %zu vertices (%.4f%%)\n", src,
         maxDepth, reached, 100.0 * (double)reached / (double)n);
  free(dist);
}

static size_t minPairwiseDistance(const gvizSubgraph *sg,
                                  const gvizGRIPState *state, size_t layerEnd,
                                  size_t maxSamples) {
  size_t n = gvizGraphSize(sg->g);
  size_t *dist = malloc(n * sizeof(size_t));
  if (!dist)
    return SIZE_MAX;

  size_t minDist = SIZE_MAX;
  size_t samples = layerEnd < maxSamples ? layerEnd : maxSamples;

  for (size_t si = 0; si < samples; si++) {
    size_t src = state->misFiltration[si];
    gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg->g);
    gvizSearchBreadthFirst(sg, &bfs, src, 0, dist);
    gvizSubgraphRelease(&bfs);

    for (size_t ti = si + 1; ti < layerEnd; ti++) {
      size_t dst = state->misFiltration[ti];
      size_t d = dist[dst];
      if (d < minDist)
        minDist = d;
    }
  }
  free(dist);
  return minDist;
}

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1]
                              : "data/human-jung-2015/data.edges";
  size_t dim = argc > 2 ? (size_t)atoi(argv[2]) : 3;

  gvizGraph graph;
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);

  printf("loading %s...\n", path);
  if (gvizGraphLoadFromEdgesFile(path, &opts, &graph) < 0) {
    fprintf(stderr, "load failed\n");
    return 1;
  }
  printf("loaded %zu vertices\n", gvizGraphSize(&graph));

  gvizGraphBuildLayout(&graph);
  printGraphStats(&graph);

  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  printf("subgraph vertices: %zu\n", gvizSubgraphVertexCount(&sg));

  gvizGRIPState state;
  size_t diameter = 64;
  if (gvizGRIPEmbedderInit(&state, sg, diameter, dim) < 0) {
    fprintf(stderr, "init failed\n");
    return 1;
  }

  gvizVertexSubset curr = gvizVertexSubsetCreateEmpty(sg.g);
  makeFirstMISPartition(&state, curr);
  size_t nvertices = gvizGraphSize(sg.g);
  size_t prevCount = gvizVertexSubsetCount(curr, nvertices);
  printf("layer 1: %zu vertices\n", prevCount);

  size_t plateauAt = 0;
  size_t i = 2;
  while (1) {
    size_t radius = (size_t)(1ULL << (i - 1));
    size_t beforeCount = gvizVertexSubsetCount(curr, nvertices);

    int cont = iterMISFiltration(&state, i, curr);
    size_t afterCount = gvizVertexSubsetCount(curr, nvertices);

    printf("layer %zu: radius=%zu count=%zu (delta=%zd) continue=%d\n", i,
           radius, afterCount, (ssize_t)(afterCount - beforeCount), cont);

    if (afterCount == beforeCount && plateauAt == 0)
      plateauAt = i;

    if (!cont)
      break;

    if (i >= 20) {
      printf("stopping debug at layer 20\n");
      break;
    }
    i++;
  }

  if (plateauAt) {
    size_t layerEnd = gvizVertexSubsetCount(curr, nvertices);
    printf("\n=== plateau analysis at layer %zu (%zu vertices in curr) ===\n",
           plateauAt, layerEnd);

    size_t *layerVerts = malloc(layerEnd * sizeof(size_t));
    size_t lv = 0;
    gvizBitArrayIterator vit =
        gvizVertexSubsetIteratorCreate(curr, nvertices);
    size_t vtx;
    while (gvizBitArrayIterate(&vit, &vtx))
      layerVerts[lv++] = vtx;

    size_t mpd = SIZE_MAX;
    for (size_t si = 0; si < lv && si < 12; si++) {
      size_t *dist = malloc(nvertices * sizeof(size_t));
      gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg.g);
      gvizSearchBreadthFirst(&sg, &bfs, layerVerts[si], 0, dist);
      gvizSubgraphRelease(&bfs);
      for (size_t ti = si + 1; ti < lv; ti++) {
        if (dist[layerVerts[ti]] < mpd)
          mpd = dist[layerVerts[ti]];
      }
      free(dist);
    }
    if (mpd == SIZE_MAX)
      printf("min pairwise distance (first 12 of %zu): unreachable\n", lv);
    else
      printf("min pairwise distance (first 12 of %zu): %zu\n", lv, mpd);

    if (lv >= 2) {
      size_t *dist = malloc(nvertices * sizeof(size_t));
      gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg.g);
      gvizSearchBreadthFirst(&sg, &bfs, layerVerts[0], 0, dist);
      gvizSubgraphRelease(&bfs);
      size_t within128 = 0;
      for (size_t ti = 1; ti < lv; ti++)
        if (dist[layerVerts[ti]] <= 128)
          within128++;
      printf("vertices within dist 128 of layerVerts[0]: %zu / %zu\n", within128,
             lv - 1);
      free(dist);
    }

    for (size_t si = 0; si < lv && si < 4; si++) {
      probeBfsReach(&sg, layerVerts[si], 128);
      probeBfsReach(&sg, layerVerts[si], 256);
    }
    free(layerVerts);
  }

  gvizVertexSubsetRelease(curr);
  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
  return 0;
}
