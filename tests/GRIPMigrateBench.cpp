// C++ port of tests/embedders/gvizGRIPMigrateBench.c: benchmarks the
// coarsest-layer "top-off" migration step (pulling extra vertices into the
// final MIS layer when it has fewer than dim+1 members) two ways -- the
// production bounded-BFS algorithm (GRIP::DebugMigrateOneToFinalLayer) vs. a
// historical O(V) full-BFS-per-candidate baseline reimplemented here -- from
// the identical checkpointed starting state, and reports wall time and RSS
// delta. Not a Unity test -- a standalone benchmark driver, same as the old
// C tool (see README.md's "Benchmarks and debug tools" section).
//
// Port notes:
//   - This is the one tool in the M6 GRIP-tooling batch that needs more than
//     read-only introspection: comparing two migration *algorithms* from the
//     same starting point requires checkpoint/restore of the filtration
//     order and layer borders, plus a way to apply either algorithm's pick.
//     GRIP.hpp's DEBUG / INTROSPECTION section calls this out explicitly as
//     broader surface than any other embedder's debug accessors in this
//     port (DebugBuildFiltrationPreMigrate/DebugSnapshotFiltration/
//     DebugSnapshotBorders/DebugRestoreFiltration/DebugMigrateOneToFinalLayer/
//     DebugApplyMigration) -- flagged there and again here.
//   - The old C's MigrateFn took an unused `GVIZ_BIT_ARRAY finalLayer`
//     parameter (both the production and legacy implementations `(void)`'d
//     it); dropped here since it never carried any information. Checkpoint/
//     restore in the old code also copied that same unused BitSet -- dropped
//     for the same reason, leaving only the filtration order and borders to
//     snapshot.
//   - migrateOneToFinalLayer_legacy's raw gvizGRIPState field pokes (misBorder
//     read via gripMisBorderAt, in-place misFiltration swap, misBorder bump)
//     become GRIP::LayerBorder/FiltrationVertexAt (read) and
//     GRIP::DebugApplyMigration (the swap-and-bump primitive), with the BFS
//     itself now a real search::BreadthFirst call instead of the old file-
//     local reimplementation.
//   - Manual gvizGRIPEmbedderInit/Release error checking is replaced by RAII
//     + exceptions, caught per depth/variant in main() so one bad case
//     doesn't abort the whole sweep (matching the old code's per-case
//     "failed at depth %zu" skip).

#include "GRIP.hpp"

#include "BreadthFirst.hpp"
#include "Error.hpp"
#include "Graph.hpp"
#include "Graphs.hpp"
#include "Subgraph.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <optional>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <sys/resource.h>
#endif

using gviz::Graph;
using gviz::LayoutError;
using gviz::Subgraph;
using gviz::graphs::BuildSierpinskiCarpet;
using gviz::layout::GRIP;
namespace search = gviz::search;

namespace {

size_t CurrentRssKb() {
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
  return static_cast<size_t>(ru.ru_maxrss);
#else
  return 0;
#endif
}

double MonotonicSeconds() {
  using Clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

/**
 * Historical O(V) baseline: for every vertex already in the final layer, one
 * full BFS over the whole subgraph, tracking each still-shallower candidate's
 * minimum distance to any final-layer vertex; the candidate with the largest
 * such minimum distance migrates in. Direct port of the old C
 * migrateOneToFinalLayer_legacy, which lived in this same driver file (not
 * in gvizGRIPEmbedder.c) -- it was never production code, only a comparison
 * baseline, so it stays here rather than moving into GRIP.cpp alongside the
 * production DebugMigrateOneToFinalLayer/MigrateOneToFinalLayer it's
 * benchmarked against. Unlike the production algorithm (which walks further
 * down the layer stack when the immediately shallower layer's pool is
 * empty), this fails outright in that case -- a real behavioral difference
 * from the bounded algorithm, preserved intentionally since it's part of
 * what the benchmark is comparing.
 */
bool MigrateOneToFinalLayerLegacy(GRIP &grip, const Graph &graph, size_t layerIndex) {
  if (layerIndex == 0)
    return false;
  size_t finalEnd = grip.LayerBorder(layerIndex);
  size_t spanLen = grip.LayerBorder(layerIndex - 1);
  if (spanLen <= finalEnd)
    return false;

  std::vector<size_t> borderSpan(spanLen, SIZE_MAX);

  for (size_t i = 0; i < finalEnd; i++) {
    std::vector<size_t> dist;
    Subgraph bfs = Subgraph::CreateEmpty(graph);
    search::BreadthFirst(grip.Structure(), bfs, grip.FiltrationVertexAt(i), 0, &dist);

    for (size_t j = finalEnd; j < spanLen; j++) {
      size_t d = dist[grip.FiltrationVertexAt(j)];
      if (d < borderSpan[j])
        borderSpan[j] = d;
    }
  }

  size_t pick = finalEnd;
  size_t maxDist = 0;
  for (size_t j = finalEnd; j < spanLen; j++) {
    if (borderSpan[j] > maxDist) {
      maxDist = borderSpan[j];
      pick = j;
    }
  }

  grip.DebugApplyMigration(layerIndex, pick);
  return true;
}

using MigrateFn = std::function<bool(GRIP &, size_t)>;

struct BenchRow {
  double preMigrateSec = 0.0;
  double migrateSec = 0.0;
  size_t migrateCalls = 0;
  size_t rssBeforeKb = 0;
  size_t rssAfterKb = 0;
  size_t rssDeltaKb = 0;
};

BenchRow RunMigratePhase(GRIP &grip, size_t layerIndex, const MigrateFn &migrate) {
  size_t dim = grip.Dim();
  BenchRow row;

  row.rssBeforeKb = CurrentRssKb();
  double t0 = MonotonicSeconds();

  while (grip.LayerBorder(layerIndex) < dim + 1) {
    if (!migrate(grip, layerIndex))
      break;
    row.migrateCalls++;
  }

  row.migrateSec = MonotonicSeconds() - t0;
  row.rssAfterKb = CurrentRssKb();
  if (row.rssAfterKb >= row.rssBeforeKb)
    row.rssDeltaKb = row.rssAfterKb - row.rssBeforeKb;
  return row;
}

std::optional<BenchRow> RunCase(size_t depth, bool legacy) {
  Graph graph = BuildSierpinskiCarpet(depth);
  graph.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(graph);

  size_t diameter = static_cast<size_t>(std::pow(3, depth)) + 64;
  GRIP grip(std::move(sg), diameter, 2);

  BenchRow row;
  double t0 = MonotonicSeconds();
  size_t layerIndex = grip.DebugBuildFiltrationPreMigrate();
  row.preMigrateSec = MonotonicSeconds() - t0;
  if (layerIndex == 0)
    return std::nullopt;

  // Checkpoint immediately followed by restore: a faithful port of the old
  // C's identical round-trip (see this file's top-of-file port notes) --
  // functionally a no-op here (nothing mutates the filtration in between),
  // kept for fidelity to the original driver's structure.
  std::vector<size_t> filtrationSnapshot = grip.DebugSnapshotFiltration();
  std::vector<size_t> borderSnapshot = grip.DebugSnapshotBorders();
  grip.DebugRestoreFiltration(filtrationSnapshot, borderSnapshot);

  MigrateFn migrate;
  if (legacy) {
    migrate = [&graph](GRIP &g, size_t li) { return MigrateOneToFinalLayerLegacy(g, graph, li); };
  } else {
    migrate = [](GRIP &g, size_t li) { return g.DebugMigrateOneToFinalLayer(li); };
  }
  BenchRow migrateRow = RunMigratePhase(grip, layerIndex, migrate);

  row.migrateSec = migrateRow.migrateSec;
  row.migrateCalls = migrateRow.migrateCalls;
  row.rssBeforeKb = migrateRow.rssBeforeKb;
  row.rssAfterKb = migrateRow.rssAfterKb;
  row.rssDeltaKb = migrateRow.rssDeltaKb;

  return row;
}

void PrintRow(const char *label, size_t depth, size_t vertices, const BenchRow &row) {
  std::printf("%-10s depth=%zu verts=%8zu  pre=%8.3fs  migrate=%8.3fs  "
              "calls=%zu  rss_before=%6zuMB  rss_after=%6zuMB  rss_delta=%4zuMB\n",
              label, depth, vertices, row.preMigrateSec, row.migrateSec, row.migrateCalls,
              row.rssBeforeKb / 1024, row.rssAfterKb / 1024, row.rssDeltaKb / 1024);
}

} // namespace

int main(int argc, char **argv) {
  std::vector<size_t> depths = {4, 5, 6, 7};

  if (argc > 1) {
    depths.clear();
    for (int i = 1; i < argc; i++)
      depths.push_back(static_cast<size_t>(std::atoi(argv[i])));
  }

  std::printf("GRIP final-layer migration benchmark (2D sierpinski carpet)\n");
  std::printf("Same pre-migrate checkpoint for legacy vs bounded on each row.\n");
  std::printf("process RSS from macOS task_info / Linux getrusage maxrss\n\n");
  std::printf("%-10s %-14s %8s %10s %7s %12s %11s %11s\n", "impl", "graph", "pre(s)",
              "migrate(s)", "calls", "rss0(MB)", "rss1(MB)", "delta(MB)");
  std::printf("%s\n",
              "--------------------------------------------------------------------------");

  for (size_t depth : depths) {
    size_t vertices = 1;
    for (size_t k = 0; k < depth; k++)
      vertices *= 8;

    std::optional<BenchRow> legacy, bounded;
    try {
      legacy = RunCase(depth, true);
    } catch (const LayoutError &) {
      legacy.reset();
    }
    if (!legacy) {
      std::fprintf(stderr, "legacy case failed at depth %zu\n", depth);
      continue;
    }

    try {
      bounded = RunCase(depth, false);
    } catch (const LayoutError &) {
      bounded.reset();
    }
    if (!bounded) {
      std::fprintf(stderr, "bounded case failed at depth %zu\n", depth);
      continue;
    }

    PrintRow("legacy", depth, vertices, *legacy);
    PrintRow("bounded", depth, vertices, *bounded);

    if (legacy->migrateSec > 0.0) {
      std::printf("  speedup migrate: %.1fx\n", legacy->migrateSec / bounded->migrateSec);
    }
    std::printf("\n");
  }

  std::printf("peak process RSS at exit: %zu MB\n", CurrentRssKb() / 1024);
  return 0;
}
