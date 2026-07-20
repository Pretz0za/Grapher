#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "embedders/gvizGRIPInternal.h"
#include "utils/graphs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROUNDS_PER_LAYER 60
#define EQUILIBRIUM_DISP 0.5

typedef struct {
  const char *name;
  size_t placementK;
  size_t refinementK;
  gvizGRIPKPolicy policy;
} KPolicyCase;

typedef struct {
  double worstLayerFinalDisp;
  double sumLayerFinalDisp;
  size_t layersNotConverged;
  size_t oscillationEvents;
  int valid;
} BenchScore;

static const KPolicyCase POLICIES[] = {
    {"const_256", 256, 256, GVIZ_GRIP_K_CONSTANT},
    {"const_128", 128, 128, GVIZ_GRIP_K_CONSTANT},
    {"const_64", 64, 64, GVIZ_GRIP_K_CONSTANT},
    {"const_32", 32, 32, GVIZ_GRIP_K_CONSTANT},
    {"decay_256", 256, 256, GVIZ_GRIP_K_LAYER_DECAY},
    {"decay_128", 128, 128, GVIZ_GRIP_K_LAYER_DECAY},
    {"decay_64", 64, 64, GVIZ_GRIP_K_LAYER_DECAY},
    {"grow_256", 256, 256, GVIZ_GRIP_K_LAYER_GROW},
    {"grow_128", 128, 128, GVIZ_GRIP_K_LAYER_GROW},
    {"grow_64", 64, 64, GVIZ_GRIP_K_LAYER_GROW},
    {"place_decay_ref_128", 128, 128, GVIZ_GRIP_K_PLACEMENT_DECAY},
    {"place_decay_ref_256", 256, 256, GVIZ_GRIP_K_PLACEMENT_DECAY},
    {"budget_64_32", 64, 32, GVIZ_GRIP_K_BUDGET},
    {"budget_128_64", 128, 64, GVIZ_GRIP_K_BUDGET},
};

static BenchScore runPolicy(const gvizGraph *graph, size_t dim,
                            const KPolicyCase *policy) {
  BenchScore score = {0, 0, 0, 0, 1};

  gvizSubgraph sg = gvizSubgraphCreateFull(graph);
  gvizGRIPState state;
  if (gvizGRIPEmbedderInit(&state, sg, 0, dim) < 0) {
    score.valid = 0;
    return score;
  }

  gvizGRIPEmbedderConfigureK(&state, policy->placementK, policy->refinementK,
                             policy->policy);
  gvizThreadPoolDestroy(state.pool);
  state.pool = NULL;

  gvizGRIPEmbedderBegin(&state);

  while (state.currLayer > 0) {
    double prevDisp = 1e300;
    size_t layerOsc = 0;
    double lastDisp = 0.0;

    for (size_t r = 0; r < ROUNDS_PER_LAYER; r++) {
      gvizGRIPEmbedderRefineRound(&state);
      gvizGRIPRoundStats stats = gvizGRIPEmbedderLastRoundStats(&state);
      if (stats.maxDisplacement > prevDisp * 1.05)
        layerOsc++;
      prevDisp = stats.maxDisplacement;
      lastDisp = stats.maxDisplacement;
    }

    if (lastDisp > score.worstLayerFinalDisp)
      score.worstLayerFinalDisp = lastDisp;
    score.sumLayerFinalDisp += lastDisp;
    score.oscillationEvents += layerOsc;
    if (lastDisp > EQUILIBRIUM_DISP)
      score.layersNotConverged++;

    gvizGRIPEmbedderNextStage(&state);
  }

  for (size_t r = 0; r < ROUNDS_PER_LAYER; r++)
    gvizGRIPEmbedderRefineRound(&state);
  {
    gvizGRIPRoundStats stats = gvizGRIPEmbedderLastRoundStats(&state);
    if (stats.maxDisplacement > score.worstLayerFinalDisp)
      score.worstLayerFinalDisp = stats.maxDisplacement;
    score.sumLayerFinalDisp += stats.maxDisplacement;
    if (stats.maxDisplacement > EQUILIBRIUM_DISP)
      score.layersNotConverged++;
  }

  gvizGRIPEmbedderRelease(&state);
  return score;
}

static void runSuite(const char *graphName, gvizGraph graph, size_t dim) {
  gvizGraphBuildLayout(&graph);

  printf("\n=== %s: %zu vertices, dim=%zu, layers benchmark ===\n", graphName,
         graph.vertices.count, dim);
  printf("%-22s %8s %8s %6s %6s\n", "policy", "worst", "avg_fin", "noconv",
         "osc");
  printf("%-22s %8s %8s %6s %6s\n", "", "disp", "disp", "layers", "cnt");

  BenchScore best = {1e300, 1e300, SIZE_MAX, SIZE_MAX, 0};
  const char *bestName = NULL;

  for (size_t i = 0; i < sizeof(POLICIES) / sizeof(POLICIES[0]); i++) {
    BenchScore s = runPolicy(&graph, dim, &POLICIES[i]);
    if (!s.valid)
      continue;

    size_t layerCount = 1;
    {
      gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
      gvizGRIPState tmp;
      gvizGRIPEmbedderInit(&tmp, sg, 0, dim);
      gvizThreadPoolDestroy(tmp.pool);
      tmp.pool = NULL;
      layerCount = createMISFiltration(&tmp);
      gvizGRIPEmbedderRelease(&tmp);
    }

    double avgFinal = s.sumLayerFinalDisp / (double)(layerCount + 1);

    printf("%-22s %8.3f %8.3f %6zu %6zu\n", POLICIES[i].name,
           s.worstLayerFinalDisp, avgFinal, s.layersNotConverged,
           s.oscillationEvents);

    double rank = s.worstLayerFinalDisp + 0.25 * avgFinal +
                  2.0 * s.layersNotConverged + 0.01 * s.oscillationEvents;
    double bestRank =
        best.worstLayerFinalDisp + 0.25 * best.sumLayerFinalDisp +
        2.0 * best.layersNotConverged + 0.01 * best.oscillationEvents;
    if (rank < bestRank) {
      best = s;
      best.sumLayerFinalDisp = avgFinal;
      bestName = POLICIES[i].name;
    }
  }

  printf("-> best: %s\n", bestName ? bestName : "(none)");
  gvizGraphRelease(&graph);
}

int main(int argc, char **argv) {
  int depth = argc > 1 ? atoi(argv[1]) : 6;
  int onlyTetra = argc > 2;

  if (!onlyTetra) {
    gvizGraph mesh = build_rect_mesh(200, 200);
    runSuite("rect_200x200", mesh, 2);
  }

  gvizGraph tetra = createSierpinskiTetrahedron(depth, NULL);
  char name[64];
  snprintf(name, sizeof(name), "sierpinski_tetra_d%d", depth);
  runSuite(name, tetra, 3);

  if (!onlyTetra && depth >= 5) {
    gvizGraph tetra5 = createSierpinskiTetrahedron(5, NULL);
    runSuite("sierpinski_tetra_d5", tetra5, 3);
  }

  return 0;
}
