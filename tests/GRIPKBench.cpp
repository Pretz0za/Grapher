// C++ port of tests/embedders/gvizGRIPKBench.c: sweeps GRIP's KNN
// policy/parameter combinations over a couple of benchmark graphs and prints
// a convergence-quality table per policy. Not a Unity test (no RUN_TEST/
// UNITY_BEGIN) -- a standalone driver program, same as the old C tool (see
// README.md's "Benchmarks and debug tools" section).
//
// Port notes:
//   - gvizGRIPState::pool was destroyed right after Init in the old C, for a
//     reproducible single-threaded run; GRIP<Subgraph>::DebugDisableThreadPool() is
//     the narrow accessor added for exactly this (see GRIP.hpp's DEBUG /
//     INTROSPECTION section).
//   - The old C's per-policy `layerCount` (used only to normalize
//     avgFinal) came from a throwaway state's createMISFiltration() call.
//     Filtration doesn't depend on the K policy at all, so the throwaway
//     here just runs the ordinary public Begin() and reads LayerCount() --
//     no new accessor needed for that step.
//   - Manual gvizGRIPEmbedderInit/-Release error checking is replaced by
//     RAII: a GRIP that fails to construct (DimensionError/
//     InsufficientVerticesError) throws, caught once per policy so a single
//     bad case doesn't abort the whole sweep, matching the old code's
//     `score.valid` skip-on-init-failure behavior.

#include "GRIP.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Graphs.hpp"
#include "Subgraph.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using gviz::Graph;
using gviz::LayoutError;
using gviz::Subgraph;
using gviz::graphs::BuildRectMesh;
using gviz::graphs::CreateSierpinskiTetrahedron;
using gviz::layout::GRIP;

namespace {

constexpr size_t kRoundsPerLayer = 60;
constexpr double kEquilibriumDisp = 0.5;

struct KPolicyCase {
  const char *name;
  size_t placementK;
  size_t refinementK;
  GRIP<Subgraph>::KPolicy policy;
};

const KPolicyCase kPolicies[] = {
    {"const_256", 256, 256, GRIP<Subgraph>::KPolicy::Constant},
    {"const_128", 128, 128, GRIP<Subgraph>::KPolicy::Constant},
    {"const_64", 64, 64, GRIP<Subgraph>::KPolicy::Constant},
    {"const_32", 32, 32, GRIP<Subgraph>::KPolicy::Constant},
    {"decay_256", 256, 256, GRIP<Subgraph>::KPolicy::LayerDecay},
    {"decay_128", 128, 128, GRIP<Subgraph>::KPolicy::LayerDecay},
    {"decay_64", 64, 64, GRIP<Subgraph>::KPolicy::LayerDecay},
    {"grow_256", 256, 256, GRIP<Subgraph>::KPolicy::LayerGrow},
    {"grow_128", 128, 128, GRIP<Subgraph>::KPolicy::LayerGrow},
    {"grow_64", 64, 64, GRIP<Subgraph>::KPolicy::LayerGrow},
    {"place_decay_ref_128", 128, 128, GRIP<Subgraph>::KPolicy::PlacementDecay},
    {"place_decay_ref_256", 256, 256, GRIP<Subgraph>::KPolicy::PlacementDecay},
    {"budget_64_32", 64, 32, GRIP<Subgraph>::KPolicy::Budget},
    {"budget_128_64", 128, 64, GRIP<Subgraph>::KPolicy::Budget},
};

struct BenchScore {
  double worstLayerFinalDisp = 0.0;
  double sumLayerFinalDisp = 0.0;
  size_t layersNotConverged = 0;
  size_t oscillationEvents = 0;
  bool valid = true;
};

BenchScore RunPolicy(const Graph &graph, size_t dim, const KPolicyCase &policy) {
  BenchScore score;

  GRIP grip(Subgraph::CreateFull(graph), 0, dim);
  grip.ConfigureK(policy.placementK, policy.refinementK, policy.policy);
  grip.DebugDisableThreadPool();

  grip.Begin();

  while (grip.CurrentLayer() > 0) {
    double prevDisp = 1e300;
    size_t layerOsc = 0;
    double lastDisp = 0.0;

    for (size_t r = 0; r < kRoundsPerLayer; r++) {
      grip.RefineRound();
      GRIP<Subgraph>::RoundStats stats = grip.LastRoundStats();
      if (stats.maxDisplacement > prevDisp * 1.05)
        layerOsc++;
      prevDisp = stats.maxDisplacement;
      lastDisp = stats.maxDisplacement;
    }

    if (lastDisp > score.worstLayerFinalDisp)
      score.worstLayerFinalDisp = lastDisp;
    score.sumLayerFinalDisp += lastDisp;
    score.oscillationEvents += layerOsc;
    if (lastDisp > kEquilibriumDisp)
      score.layersNotConverged++;

    grip.NextStage();
  }

  for (size_t r = 0; r < kRoundsPerLayer; r++)
    grip.RefineRound();
  {
    GRIP<Subgraph>::RoundStats stats = grip.LastRoundStats();
    if (stats.maxDisplacement > score.worstLayerFinalDisp)
      score.worstLayerFinalDisp = stats.maxDisplacement;
    score.sumLayerFinalDisp += stats.maxDisplacement;
    if (stats.maxDisplacement > kEquilibriumDisp)
      score.layersNotConverged++;
  }

  return score;
}

void RunSuite(const char *graphName, Graph graph, size_t dim) {
  graph.BuildLayout();

  std::printf("\n=== %s: %zu vertices, dim=%zu, layers benchmark ===\n", graphName,
              graph.Size(), dim);
  std::printf("%-22s %8s %8s %6s %6s\n", "policy", "worst", "avg_fin", "noconv", "osc");
  std::printf("%-22s %8s %8s %6s %6s\n", "", "disp", "disp", "layers", "cnt");

  BenchScore best;
  best.worstLayerFinalDisp = 1e300;
  best.sumLayerFinalDisp = 1e300;
  best.layersNotConverged = SIZE_MAX;
  best.oscillationEvents = SIZE_MAX;
  best.valid = false;
  const char *bestName = nullptr;

  for (const KPolicyCase &policy : kPolicies) {
    BenchScore s;
    try {
      s = RunPolicy(graph, dim, policy);
    } catch (const LayoutError &) {
      continue;
    }
    if (!s.valid)
      continue;

    size_t layerCount = 1;
    {
      GRIP tmp(Subgraph::CreateFull(graph), 0, dim);
      tmp.DebugDisableThreadPool();
      tmp.Begin();
      layerCount = tmp.LayerCount();
    }

    double avgFinal = s.sumLayerFinalDisp / static_cast<double>(layerCount + 1);

    std::printf("%-22s %8.3f %8.3f %6zu %6zu\n", policy.name, s.worstLayerFinalDisp,
                avgFinal, s.layersNotConverged, s.oscillationEvents);

    double rank = s.worstLayerFinalDisp + 0.25 * avgFinal +
                  2.0 * static_cast<double>(s.layersNotConverged) +
                  0.01 * static_cast<double>(s.oscillationEvents);
    double bestRank = best.worstLayerFinalDisp + 0.25 * best.sumLayerFinalDisp +
                       2.0 * static_cast<double>(best.layersNotConverged) +
                       0.01 * static_cast<double>(best.oscillationEvents);
    if (rank < bestRank) {
      best = s;
      best.sumLayerFinalDisp = avgFinal;
      bestName = policy.name;
    }
  }

  std::printf("-> best: %s\n", bestName ? bestName : "(none)");
}

} // namespace

int main(int argc, char **argv) {
  int depth = argc > 1 ? std::atoi(argv[1]) : 6;
  bool onlyTetra = argc > 2;

  if (!onlyTetra) {
    Graph mesh = BuildRectMesh(200, 200);
    RunSuite("rect_200x200", std::move(mesh), 2);
  }

  Graph tetra = CreateSierpinskiTetrahedron(depth);
  char name[64];
  std::snprintf(name, sizeof(name), "sierpinski_tetra_d%d", depth);
  RunSuite(name, std::move(tetra), 3);

  if (!onlyTetra && depth >= 5) {
    Graph tetra5 = CreateSierpinskiTetrahedron(5);
    RunSuite("sierpinski_tetra_d5", std::move(tetra5), 3);
  }

  return 0;
}
