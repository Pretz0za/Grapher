// C++ port of tests/embedders/gvizGRIPFiltrationDebug.c: drives GRIP's MIS
// filtration one layer at a time (instead of through Begin()) and prints
// per-layer counts, then -- if the filtration plateaus (a layer that adds no
// new vertices) -- runs a small BFS-based analysis of why. The companion
// driver for GVIZ_GRIP_DEBUG_FILTRATION (see README.md's "Debug and
// profiling environment variables" section): run with that variable set to
// also get the [grip-filt]/[grip-bfs] lines GRIP.cpp emits internally while
// DebugMakeFirstMISPartition/DebugIterMISFiltration run. Not a Unity test --
// a standalone driver, same as the old C tool.
//
// Port notes:
//   - makeFirstMISPartition/iterMISFiltration (gvizGRIPInternal.h) become
//     GRIP<Subgraph>::DebugMakeFirstMISPartition/DebugIterMISFiltration, thin
//     passthroughs to the same private methods Begin() itself calls (see
//     GRIP.hpp's DEBUG / INTROSPECTION section) -- this tool's whole point
//     is driving those two steps manually instead of letting Begin() run
//     them to completion.
//   - gvizVertexSubset (a raw GVIZ_BIT_ARRAY in the old C) is BitSet here;
//     gvizVertexSubsetCount/Iterator become BitSet::Popcount()/range-for.
//   - state->misFiltration[i] reads become GRIP<Subgraph>::FiltrationVertexAt(i).

#include "GRIP.hpp"

#include "BitSet.hpp"
#include "BreadthFirst.hpp"
#include "Graph.hpp"
#include "GraphLoader.hpp"
#include "Subgraph.hpp"

#include <cstdio>
#include <cstdlib>
#include <vector>

using gviz::BitSet;
using gviz::Graph;
using gviz::Subgraph;
using gviz::io::EdgesFileOptions;
using gviz::io::LoadFromEdgesFile;
using gviz::layout::GRIP;
namespace search = gviz::search;

namespace {

void PrintGraphStats(const Graph &g) {
  size_t n = g.Size();
  size_t minDeg = SIZE_MAX, maxDeg = 0;
  unsigned long long degSum = 0;
  for (size_t i = 0; i < n; i++) {
    size_t d = g.Degree(i);
    if (d < minDeg)
      minDeg = d;
    if (d > maxDeg)
      maxDeg = d;
    degSum += d;
  }
  std::printf("graph: %zu vertices, layout edges %zu, directed=%d\n", n, g.EdgeCount(),
              g.IsDirected());
  std::printf("degree: min=%zu max=%zu avg=%.2f\n", minDeg, maxDeg,
              static_cast<double>(degSum) / static_cast<double>(n));
}

void ProbeBfsReach(GRIP<Subgraph> &grip, const Graph &graph, size_t src, size_t maxDepth) {
  size_t n = graph.Size();
  Subgraph bfs = Subgraph::CreateEmpty(graph);
  search::BreadthFirstTree(grip.Structure(), bfs, src, maxDepth, nullptr);
  size_t reached = bfs.VertexCount();
  std::printf("  BFS from vtx %zu depth %zu: reached %zu vertices (%.4f%%)\n", src,
              maxDepth, reached, 100.0 * static_cast<double>(reached) / static_cast<double>(n));
}

} // namespace

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : "data/human-jung-2015/data.edges";
  size_t dim = argc > 2 ? static_cast<size_t>(std::atoi(argv[2])) : 3;

  std::printf("loading %s...\n", path);
  Graph graph = LoadFromEdgesFile(path, EdgesFileOptions{});
  std::printf("loaded %zu vertices\n", graph.Size());

  graph.BuildLayout();
  PrintGraphStats(graph);

  Subgraph sg = Subgraph::CreateFull(graph);
  std::printf("subgraph vertices: %zu\n", sg.VertexCount());

  size_t diameter = 64;
  GRIP grip(std::move(sg), diameter, dim);

  BitSet curr(grip.Structure().VertexCapacity());
  grip.DebugMakeFirstMISPartition(curr);
  size_t nvertices = graph.Size();
  size_t prevCount = curr.Popcount();
  std::printf("layer 1: %zu vertices\n", prevCount);
  (void)nvertices;

  size_t plateauAt = 0;
  size_t i = 2;
  while (true) {
    size_t radius = size_t{1} << (i - 1);
    size_t beforeCount = curr.Popcount();

    bool cont = grip.DebugIterMISFiltration(i, curr);
    size_t afterCount = curr.Popcount();

    std::printf("layer %zu: radius=%zu count=%zu (delta=%zd) continue=%d\n", i, radius,
                afterCount, static_cast<ptrdiff_t>(afterCount) - static_cast<ptrdiff_t>(beforeCount),
                cont ? 1 : 0);

    if (afterCount == beforeCount && plateauAt == 0)
      plateauAt = i;

    if (!cont)
      break;

    if (i >= 20) {
      std::printf("stopping debug at layer 20\n");
      break;
    }
    i++;
  }

  if (plateauAt) {
    size_t layerEnd = curr.Popcount();
    std::printf("\n=== plateau analysis at layer %zu (%zu vertices in curr) ===\n",
                plateauAt, layerEnd);

    std::vector<size_t> layerVerts;
    layerVerts.reserve(layerEnd);
    for (size_t v : curr)
      layerVerts.push_back(v);
    size_t lv = layerVerts.size();

    size_t mpd = SIZE_MAX;
    for (size_t si = 0; si < lv && si < 12; si++) {
      std::vector<size_t> dist;
      Subgraph bfs = Subgraph::CreateEmpty(graph);
      search::BreadthFirstTree(grip.Structure(), bfs, layerVerts[si], 0, &dist);
      for (size_t ti = si + 1; ti < lv; ti++) {
        if (dist[layerVerts[ti]] < mpd)
          mpd = dist[layerVerts[ti]];
      }
    }
    if (mpd == SIZE_MAX)
      std::printf("min pairwise distance (first 12 of %zu): unreachable\n", lv);
    else
      std::printf("min pairwise distance (first 12 of %zu): %zu\n", lv, mpd);

    if (lv >= 2) {
      std::vector<size_t> dist;
      Subgraph bfs = Subgraph::CreateEmpty(graph);
      search::BreadthFirstTree(grip.Structure(), bfs, layerVerts[0], 0, &dist);
      size_t within128 = 0;
      for (size_t ti = 1; ti < lv; ti++)
        if (dist[layerVerts[ti]] <= 128)
          within128++;
      std::printf("vertices within dist 128 of layerVerts[0]: %zu / %zu\n", within128,
                  lv - 1);
    }

    for (size_t si = 0; si < lv && si < 4; si++) {
      ProbeBfsReach(grip, graph, layerVerts[si], 128);
      ProbeBfsReach(grip, graph, layerVerts[si], 256);
    }
  }

  return 0;
}
