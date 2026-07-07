#include "algorithms/search/gvizConnectedComponents.h"
#include "algorithms/search/gvizKNearest.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "utils/graphLoader.h"
#include "utils/graphs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double monotonicSeconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static size_t gripMisBorderAt(const gvizGRIPState *state, size_t i) {
  return *(const size_t *)gvizArrayAtIndex(&state->misBorder, i);
}

static void printGraphStats(const char *label, const gvizGraph *g) {
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
  printf("[%s] %zu vertices, %zu edges, degree min=%zu max=%zu avg=%.2f\n",
         label, n, gvizGraphEdgeCount(g), minDeg, maxDeg,
         (double)degSum / (double)n);
}

static int largestComponentSubgraph(const gvizGraph *graph, gvizSubgraph *out) {
  size_t n = gvizGraphSize(graph);
  gvizSubgraph full = gvizSubgraphCreateFull(graph);

  size_t *labels = malloc(n * sizeof(size_t));
  if (!labels) {
    gvizSubgraphRelease(&full);
    return -1;
  }

  size_t count = 0;
  if (gvizConnectedComponents(&full, labels, &count) < 0) {
    free(labels);
    gvizSubgraphRelease(&full);
    return -1;
  }
  gvizSubgraphRelease(&full);

  if (count == 0) {
    free(labels);
    return -1;
  }

  size_t *sizes = calloc(count, sizeof(size_t));
  if (!sizes) {
    free(labels);
    return -1;
  }
  gvizConnectedComponentSizes(labels, n, count, sizes);

  size_t largest = 0;
  for (size_t c = 1; c < count; c++) {
    if (sizes[c] > sizes[largest])
      largest = c;
  }

  printf("components=%zu largest=%zu (%.1f%% of vertices)\n", count,
         sizes[largest], 100.0 * (double)sizes[largest] / (double)n);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(graph);
  if (!vs) {
    free(labels);
    free(sizes);
    return -1;
  }

  for (size_t v = 0; v < n; v++) {
    if (labels[v] == largest)
      gvizVertexSubsetShowVertex(vs, v);
  }

  free(labels);
  free(sizes);

  *out = gvizSubgraphCreateVertexInduced(graph, vs);
  return 0;
}

static int loadEdgesLargestCC(const char *path, gvizGraph *graph,
                              gvizSubgraph *sg) {
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);

  printf("loading %s...\n", path);
  double t0 = monotonicSeconds();
  if (gvizGraphLoadFromEdgesFile(path, &opts, graph) < 0)
    return -1;
  printf("load %.2fs\n", monotonicSeconds() - t0);

  gvizGraphBuildLayout(graph);
  printGraphStats("full", graph);

  t0 = monotonicSeconds();
  if (largestComponentSubgraph(graph, sg) < 0)
    return -1;
  printf("largest CC %.2fs\n", monotonicSeconds() - t0);
  return 0;
}

static int runGripStages(const char *label, gvizSubgraph sg, size_t dim,
                         size_t diameter, size_t maxStages) {
  gvizGRIPState state;
  if (gvizGRIPEmbedderInit(&state, sg, diameter, dim) < 0) {
    fprintf(stderr, "%s: init failed\n", label);
    return -1;
  }
  gvizGRIPEmbedderConfigureK(&state, 64, 64, 64, GVIZ_GRIP_K_BUDGET);

  double t0 = monotonicSeconds();
  gvizGRIPEmbedderBegin(&state);
  double beginSec = monotonicSeconds() - t0;

  printf("\n=== %s ===\n", label);
  printf("layers=%zu currLayer=%zu (coarsest active)\n", state.layerCount,
         state.currLayer);
  printf("Begin (filtration+simplex+initial KNN): %.3fs\n", beginSec);

  for (size_t stage = 0; stage < maxStages && state.currLayer > 0; stage++) {
    size_t layerBefore = state.currLayer;
    size_t placeCount =
        gripMisBorderAt(&state, layerBefore) -
        gripMisBorderAt(&state, layerBefore + 1);
    size_t visibleBefore =
        gripMisBorderAt(&state, layerBefore + 1);

    t0 = monotonicSeconds();
    if (getenv("GVIZ_KNN_PROFILE"))
      gvizKNNProfileReset();
    beginNewStage(&state);
    double stageSec = monotonicSeconds() - t0;

    unsigned long long knnQueries = 0, knnVisited = 0, knnMaxVisited = 0;
    if (getenv("GVIZ_KNN_PROFILE"))
      gvizKNNProfileSnapshot(&knnQueries, &knnVisited, &knnMaxVisited);

    printf("stage %zu: layer %zu -> %zu | place %zu verts | visible before %zu "
           "| %.3fs (%.3fus/vert)",
           stage, layerBefore, state.currLayer, placeCount, visibleBefore,
           stageSec, placeCount ? (stageSec * 1e6) / (double)placeCount : 0.0);
    if (getenv("GVIZ_KNN_PROFILE")) {
      double avgVisited =
          knnQueries ? (double)knnVisited / (double)knnQueries : 0.0;
      printf(" | knn q=%llu avgVisit=%.0f maxVisit=%llu", knnQueries,
             avgVisited, knnMaxVisited);
    }
    printf("\n");
  }

  gvizGRIPEmbedderRelease(&state);
  return 0;
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "sierpinski";
  size_t dim = argc > 2 ? (size_t)atoi(argv[2]) : 3;
  size_t maxStages = argc > 3 ? (size_t)atoi(argv[3]) : 2;

  if (!strcmp(mode, "sierpinski")) {
    int depth = argc > 4 ? atoi(argv[4]) : 9;
    gvizGraph graph = createSierpinskiTetrahedron(depth, NULL);
    gvizGraphBuildLayout(&graph);
    printGraphStats("sierpinski", &graph);
    gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
    size_t diameter = (size_t)(1ULL << depth) + 64;
    int rc = runGripStages("sierpinski-tetra", sg, dim, diameter, maxStages);
    gvizGraphRelease(&graph);
    return rc < 0 ? 1 : 0;
  }

  if (!strcmp(mode, "edges")) {
    const char *path = argc > 4 ? argv[4]
                                : "data/human-jung-2015/data.edges";
    size_t diameter = argc > 5 ? (size_t)atoi(argv[5]) : 64;
    gvizGraph graph;
    gvizSubgraph sg;
    if (loadEdgesLargestCC(path, &graph, &sg) < 0)
      return 1;
    printGraphStats("largest CC", sg.g);
    int rc = runGripStages(path, sg, dim, diameter, maxStages);
    gvizSubgraphRelease(&sg);
    gvizGraphRelease(&graph);
    return rc < 0 ? 1 : 0;
  }

  fprintf(stderr, "usage: %s sierpinski [dim] [maxStages] [depth]\n", argv[0]);
  fprintf(stderr, "       %s edges [dim] [maxStages] [path] [diameter]\n",
          argv[0]);
  return 1;
}
