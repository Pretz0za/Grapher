#include "gvizGRIPEmbedderInternal.h"

#include "algorithms/search/gvizBreadthFirst.h"
#include "core/alloc.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "utils/graphs.h"
#include "utils/helpers.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/resource.h>
#endif

static size_t gripMisBorderAt(const gvizGRIPState *state, size_t i) {
  return *(const size_t *)gvizArrayAtIndex(&state->misBorder, i);
}

static size_t *gripMisBorderMut(gvizGRIPState *state, size_t i) {
  return (size_t *)gvizArrayAtIndex(&state->misBorder, i);
}

typedef struct {
  size_t *misFiltration;
  size_t *misBorder;
  size_t misBorderCount;
  size_t layerIndex;
  GVIZ_BIT_ARRAY curr;
  size_t nvertices;
} FiltrationCheckpoint;

static int migrateOneToFinalLayer_legacy(gvizGRIPState *state,
                                         GVIZ_BIT_ARRAY finalLayer,
                                         size_t count) {
  (void)finalLayer;
  const gvizSubgraph *sg = &((gvizEmbeddedGraph *)state)->subgraph;
  size_t nvertices = gvizGraphSize(sg->g);
  size_t spanLen = gripMisBorderAt(state, count - 2);
  if (spanLen <= gripMisBorderAt(state, count - 1))
    return -1;
  size_t *distances = GVIZ_ALLOC(nvertices * sizeof(size_t));
  size_t *borderSpan = GVIZ_ALLOC(spanLen * sizeof(size_t));
  if (!distances || !borderSpan) {
    GVIZ_DEALLOC(distances);
    GVIZ_DEALLOC(borderSpan);
    return -1;
  }
  for (size_t j = 0; j < spanLen; j++)
    borderSpan[j] = SIZE_MAX;

  gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg->g);

  for (size_t i = 0; i < gripMisBorderAt(state, count - 1); i++) {
    gvizSubgraphRelease(&bfs);
    bfs = gvizSubgraphCreateEmpty(sg->g);
    gvizSearchBreadthFirst(sg, &bfs, state->misFiltration[i], 0, distances);

    for (size_t j = gripMisBorderAt(state, count - 1); j < spanLen; j++) {
      size_t dist = distances[state->misFiltration[j]];
      borderSpan[j] = borderSpan[j] > dist ? dist : borderSpan[j];
    }
  }

  gvizSubgraphRelease(&bfs);

  size_t *maxDistVertex = &state->misFiltration[gripMisBorderAt(state, count - 1)];
  size_t maxDist = 0;
  for (size_t j = gripMisBorderAt(state, count - 1); j < spanLen; j++) {
    if (maxDist < borderSpan[j]) {
      maxDist = borderSpan[j];
      maxDistVertex = &state->misFiltration[j];
    }
  }

  xorSwap(maxDistVertex,
          &state->misFiltration[gripMisBorderAt(state, count - 1)]);
  (*gripMisBorderMut(state, count - 1))++;

  GVIZ_DEALLOC(distances);
  GVIZ_DEALLOC(borderSpan);
  return 0;
}

typedef int (*MigrateFn)(gvizGRIPState *, GVIZ_BIT_ARRAY, size_t);

static size_t currentRssKb(void) {
#if defined(__APPLE__)
  struct task_basic_info info;
  mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &count) !=
      KERN_SUCCESS)
    return 0;
  return info.resident_size / 1024;
#elif defined(__linux__)
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) != 0)
    return 0;
  return (size_t)ru.ru_maxrss;
#else
  return 0;
#endif
}

static double monotonicSeconds(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static size_t buildFiltrationPreMigrate(gvizGRIPState *state,
                                        gvizVertexSubset *currOut) {
  const gvizSubgraph *sg = &((gvizEmbeddedGraph *)state)->subgraph;
  size_t nvertices = gvizGraphSize(sg->g);
  gvizVertexSubset curr = gvizVertexSubsetCreateEmpty(sg->g);
  if (!curr)
    return 0;

  makeFirstMISPartition(state, curr);

  size_t i = 2;
  while (iterMISFiltration(state, i, curr))
    i++;

  size_t k = 0;
  gvizBitArrayIterator it = gvizVertexSubsetIteratorCreate(curr, nvertices);
  size_t vtx;
  while (gvizBitArrayIterate(&it, &vtx))
    state->misFiltration[k++] = vtx;

  *currOut = curr;
  return i;
}

static int checkpointFiltration(const gvizGRIPState *state, gvizVertexSubset curr,
                                size_t layerIndex, FiltrationCheckpoint *cp) {
  const gvizSubgraph *sg = &((const gvizEmbeddedGraph *)state)->subgraph;
  size_t nvertices = gvizGraphSize(sg->g);
  size_t borderBytes = state->misBorder.count * sizeof(size_t);
  size_t currUnits = GVIZ_ARRAY_UNITS(nvertices);

  cp->nvertices = nvertices;
  cp->layerIndex = layerIndex;
  cp->misBorderCount = state->misBorder.count;
  cp->misFiltration = GVIZ_ALLOC(nvertices * sizeof(size_t));
  cp->misBorder = GVIZ_ALLOC(borderBytes);
  cp->curr = GVIZ_ALLOC(currUnits * sizeof(GVIZ_BIT_UNIT));
  if (!cp->misFiltration || !cp->misBorder || !cp->curr)
    return -1;

  memcpy(cp->misFiltration, state->misFiltration, nvertices * sizeof(size_t));
  memcpy(cp->misBorder, state->misBorder.arr, borderBytes);
  memcpy(cp->curr, curr, currUnits * sizeof(GVIZ_BIT_UNIT));
  return 0;
}

static void restoreCheckpoint(gvizGRIPState *state,
                              const FiltrationCheckpoint *cp,
                              gvizVertexSubset *currOut) {
  memcpy(state->misFiltration, cp->misFiltration,
         cp->nvertices * sizeof(size_t));
  memcpy(state->misBorder.arr, cp->misBorder, cp->misBorderCount * sizeof(size_t));
  state->misBorder.count = cp->misBorderCount;

  if (*currOut)
    gvizVertexSubsetRelease(*currOut);
  size_t currUnits = GVIZ_ARRAY_UNITS(cp->nvertices);
  *currOut = GVIZ_ALLOC(currUnits * sizeof(GVIZ_BIT_UNIT));
  if (*currOut)
    memcpy(*currOut, cp->curr, currUnits * sizeof(GVIZ_BIT_UNIT));
}

static void releaseCheckpoint(FiltrationCheckpoint *cp) {
  GVIZ_DEALLOC(cp->misFiltration);
  GVIZ_DEALLOC(cp->misBorder);
  GVIZ_DEALLOC(cp->curr);
  memset(cp, 0, sizeof(*cp));
}

typedef struct {
  double preMigrateSec;
  double migrateSec;
  size_t migrateCalls;
  size_t rssBeforeKb;
  size_t rssAfterKb;
  size_t rssDeltaKb;
} BenchRow;

static BenchRow runMigratePhase(gvizGRIPState *state, gvizVertexSubset curr,
                                size_t layerIndex, MigrateFn migrate) {
  size_t dim = ((gvizEmbeddedGraph *)state)->embedding.dim;
  BenchRow row = {0};

  row.rssBeforeKb = currentRssKb();
  double t0 = monotonicSeconds();

  while (gripMisBorderAt(state, layerIndex) < dim + 1) {
    if (migrate(state, curr, layerIndex + 1) < 0)
      break;
    row.migrateCalls++;
  }

  row.migrateSec = monotonicSeconds() - t0;
  row.rssAfterKb = currentRssKb();
  if (row.rssAfterKb >= row.rssBeforeKb)
    row.rssDeltaKb = row.rssAfterKb - row.rssBeforeKb;
  return row;
}

static int runCase(size_t depth, int legacy, BenchRow *row) {
  gvizGraph graph = build_sierpinski_carpet(depth);
  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);

  gvizGRIPState grip;
  size_t diameter = (size_t)pow(3, depth) + 64;
  if (gvizGRIPEmbedderInit(&grip, sg, diameter, 2) < 0) {
    gvizGraphRelease(&graph);
    return -1;
  }

  gvizVertexSubset curr = NULL;
  double t0 = monotonicSeconds();
  size_t layerIndex = buildFiltrationPreMigrate(&grip, &curr);
  row->preMigrateSec = monotonicSeconds() - t0;
  if (!layerIndex || !curr) {
    gvizGRIPEmbedderRelease(&grip);
    gvizGraphRelease(&graph);
    return -1;
  }

  FiltrationCheckpoint cp;
  if (checkpointFiltration(&grip, curr, layerIndex, &cp) < 0) {
    gvizVertexSubsetRelease(curr);
    gvizGRIPEmbedderRelease(&grip);
    gvizGraphRelease(&graph);
    return -1;
  }

  restoreCheckpoint(&grip, &cp, &curr);
  MigrateFn migrate =
      legacy ? migrateOneToFinalLayer_legacy : migrateOneToFinalLayer;
  BenchRow migrateRow = runMigratePhase(&grip, curr, layerIndex, migrate);

  row->migrateSec = migrateRow.migrateSec;
  row->migrateCalls = migrateRow.migrateCalls;
  row->rssBeforeKb = migrateRow.rssBeforeKb;
  row->rssAfterKb = migrateRow.rssAfterKb;
  row->rssDeltaKb = migrateRow.rssDeltaKb;

  releaseCheckpoint(&cp);
  gvizVertexSubsetRelease(curr);
  gvizGRIPEmbedderRelease(&grip);
  gvizGraphRelease(&graph);
  return 0;
}

static void printRow(const char *label, size_t depth, size_t vertices,
                     const BenchRow *row) {
  printf("%-10s depth=%zu verts=%8zu  pre=%8.3fs  migrate=%8.3fs  "
         "calls=%zu  rss_before=%6zuMB  rss_after=%6zuMB  rss_delta=%4zuMB\n",
         label, depth, vertices, row->preMigrateSec, row->migrateSec,
         row->migrateCalls, row->rssBeforeKb / 1024, row->rssAfterKb / 1024,
         row->rssDeltaKb / 1024);
}

int main(int argc, char **argv) {
  size_t depths[] = {4, 5, 6, 7};
  size_t depthCount = sizeof(depths) / sizeof(depths[0]);

  if (argc > 1) {
    depthCount = (size_t)(argc - 1);
    for (size_t i = 0; i < depthCount; i++)
      depths[i] = (size_t)atoi(argv[i + 1]);
  }

  printf("GRIP final-layer migration benchmark (2D sierpinski carpet)\n");
  printf("Same pre-migrate checkpoint for legacy vs bounded on each row.\n");
  printf("process RSS from macOS task_info / Linux getrusage maxrss\n\n");
  printf("%-10s %-14s %8s %10s %7s %12s %11s %11s\n", "impl", "graph",
         "pre(s)", "migrate(s)", "calls", "rss0(MB)", "rss1(MB)", "delta(MB)");
  printf("%s\n", "--------------------------------------------------------------------------");

  for (size_t d = 0; d < depthCount; d++) {
    size_t depth = depths[d];
    size_t vertices = 1;
    for (size_t k = 0; k < depth; k++)
      vertices *= 8;

    BenchRow legacy = {0};
    BenchRow bounded = {0};

    if (runCase(depth, 1, &legacy) < 0) {
      fprintf(stderr, "legacy case failed at depth %zu\n", depth);
      continue;
    }
    if (runCase(depth, 0, &bounded) < 0) {
      fprintf(stderr, "bounded case failed at depth %zu\n", depth);
      continue;
    }

    printRow("legacy", depth, vertices, &legacy);
    printRow("bounded", depth, vertices, &bounded);

    if (legacy.migrateSec > 0.0) {
      printf("  speedup migrate: %.1fx\n",
             legacy.migrateSec / bounded.migrateSec);
    }
    printf("\n");
  }

  printf("peak process RSS at exit: %zu MB\n", currentRssKb() / 1024);
  return 0;
}
