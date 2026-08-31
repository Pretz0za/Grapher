// C++ port of tests/embedders/gvizGRIPPlaceBench.c: benchmarks GRIP's
// per-stage placement + KNN-refresh timing (NextStage) on either a
// Sierpinski tetrahedron or the largest connected component of a loaded
// .edges graph. Not a Unity test -- a standalone benchmark driver, same as
// the old C tool (see README.md's "Benchmarks and debug tools" section).
// Pair with GVIZ_GRIP_STAGE_TIMING (splits each NextStage into
// placeLayerVertices/updateKNNs) and GVIZ_KNN_PROFILE (per-query KNN BFS
// visit counts) -- both documented in README.md's "Debug and profiling
// environment variables" section and, unlike GRIPKBench/LayerProbe/
// MigrateBench/FiltrationDebug, needed NO new GRIP.hpp accessors beyond
// LayerBorder (shared with the other tools): this tool drives GRIP entirely
// through Begin()/NextStage()/ConfigureK(), so the env-var logging added to
// GRIP<Subgraph>::NextStage/IterMISFiltration/VerticesWithinRadius (see GRIP.cpp) and
// the already-ported gviz::search::KnnProfileReset/KnnProfileSnapshot
// (KNearest.hpp) fire automatically.
//
// Port note: largestComponentSubgraph/loadEdgesLargestCC take the resulting
// Graph and Subgraph as out-parameters (Graph& and std::optional<Subgraph>&)
// rather than returning them bundled together -- Subgraph holds a `const
// Graph&` to its parent that can never be reseated (see Subgraph.hpp), so a
// struct pairing a Graph value with a Subgraph that references it is not
// safe to return by value (nothing guarantees the Graph sub-object doesn't
// move to a new address on the way out). The old C sidestepped this
// entirely with raw output pointers into the caller's already-allocated
// gvizGraph/gvizSubgraph; this is the same shape, via a caller-owned Graph
// local plus std::optional<Subgraph>::emplace() (the pattern Subgraph.hpp
// itself recommends for a "slot that outlives its own construction").

#include "GRIP.hpp"

#include "ConnectedComponents.hpp"
#include "Error.hpp"
#include "Graph.hpp"
#include "GraphLoader.hpp"
#include "Graphs.hpp"
#include "KNearest.hpp"
#include "Subgraph.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using gviz::Graph;
using gviz::LayoutError;
using gviz::Subgraph;
using gviz::graphs::CreateSierpinskiTetrahedron;
using gviz::io::EdgesFileOptions;
using gviz::io::LoadFromEdgesFile;
using gviz::layout::GRIP;
namespace search = gviz::search;

namespace {

double MonotonicSeconds() {
  using Clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
}

void PrintGraphStats(const char *label, const Graph &g) {
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
  std::printf("[%s] %zu vertices, %zu edges, degree min=%zu max=%zu avg=%.2f\n", label, n,
              g.EdgeCount(), minDeg, maxDeg, static_cast<double>(degSum) / static_cast<double>(n));
}

Subgraph LargestComponentSubgraph(const Graph &graph) {
  Subgraph full = Subgraph::CreateFull(graph);
  search::Components comps = search::ConnectedComponents(full);
  if (comps.count == 0)
    throw std::runtime_error("graph has no components");

  std::vector<size_t> sizes = search::ConnectedComponentSizes(comps.labels, comps.count);
  size_t largest = 0;
  for (size_t c = 1; c < comps.count; c++)
    if (sizes[c] > sizes[largest])
      largest = c;

  size_t n = graph.Size();
  std::printf("components=%zu largest=%zu (%.1f%% of vertices)\n", comps.count, sizes[largest],
              100.0 * static_cast<double>(sizes[largest]) / static_cast<double>(n));

  Subgraph vs = Subgraph::CreateVertexInduced(graph);
  for (size_t v = 0; v < n; v++)
    if (comps.labels[v] == largest)
      vs.ShowVertex(v);
  return vs;
}

// Fills @p graph (assigned in place -- move-assignment keeps its address
// stable, which is what lets @p sgOut safely reference it afterward) and
// emplaces the largest-connected-component subgraph into @p sgOut.
void LoadEdgesLargestCC(const std::string &path, Graph &graph, std::optional<Subgraph> &sgOut) {
  std::printf("loading %s...\n", path.c_str());
  double t0 = MonotonicSeconds();
  graph = LoadFromEdgesFile(path, EdgesFileOptions{});
  std::printf("load %.2fs\n", MonotonicSeconds() - t0);

  graph.BuildLayout();
  PrintGraphStats("full", graph);

  t0 = MonotonicSeconds();
  sgOut.emplace(LargestComponentSubgraph(graph));
  std::printf("largest CC %.2fs\n", MonotonicSeconds() - t0);
}

int RunGripStages(const std::string &label, Subgraph sg, size_t dim, size_t diameter,
                   size_t maxStages) {
  std::unique_ptr<GRIP<Subgraph>> gripPtr;
  try {
    gripPtr = std::make_unique<GRIP<Subgraph>>(std::move(sg), diameter, dim);
  } catch (const LayoutError &) {
    std::fprintf(stderr, "%s: init failed\n", label.c_str());
    return -1;
  }
  GRIP<Subgraph> &grip = *gripPtr;
  grip.ConfigureK(64, 64, GRIP<Subgraph>::KPolicy::Budget);

  double t0 = MonotonicSeconds();
  grip.Begin();
  double beginSec = MonotonicSeconds() - t0;

  std::printf("\n=== %s ===\n", label.c_str());
  std::printf("layers=%zu currLayer=%zu (coarsest active)\n", grip.LayerCount(),
              grip.CurrentLayer());
  std::printf("Begin (filtration+simplex+initial KNN): %.3fs\n", beginSec);

  bool knnProfile = std::getenv("GVIZ_KNN_PROFILE") != nullptr;

  for (size_t stage = 0; stage < maxStages && grip.CurrentLayer() > 0; stage++) {
    size_t layerBefore = grip.CurrentLayer();
    size_t placeCount = grip.LayerBorder(layerBefore) - grip.LayerBorder(layerBefore + 1);
    size_t visibleBefore = grip.LayerBorder(layerBefore + 1);

    t0 = MonotonicSeconds();
    if (knnProfile)
      search::KnnProfileReset();
    grip.NextStage();
    double stageSec = MonotonicSeconds() - t0;

    unsigned long long knnQueries = 0, knnVisited = 0, knnMaxVisited = 0;
    if (knnProfile)
      search::KnnProfileSnapshot(&knnQueries, &knnVisited, &knnMaxVisited);

    std::printf("stage %zu: layer %zu -> %zu | place %zu verts | visible before %zu "
                "| %.3fs (%.3fus/vert)",
                stage, layerBefore, grip.CurrentLayer(), placeCount, visibleBefore, stageSec,
                placeCount ? (stageSec * 1e6) / static_cast<double>(placeCount) : 0.0);
    if (knnProfile) {
      double avgVisited = knnQueries ? static_cast<double>(knnVisited) / static_cast<double>(knnQueries) : 0.0;
      std::printf(" | knn q=%llu avgVisit=%.0f maxVisit=%llu", knnQueries, avgVisited, knnMaxVisited);
    }
    std::printf("\n");
  }

  return 0;
}

} // namespace

int main(int argc, char **argv) {
  std::string mode = argc > 1 ? argv[1] : "sierpinski";
  size_t dim = argc > 2 ? static_cast<size_t>(std::atoi(argv[2])) : 3;
  size_t maxStages = argc > 3 ? static_cast<size_t>(std::atoi(argv[3])) : 2;

  if (mode == "sierpinski") {
    int depth = argc > 4 ? std::atoi(argv[4]) : 9;
    Graph graph = CreateSierpinskiTetrahedron(depth);
    graph.BuildLayout();
    PrintGraphStats("sierpinski", graph);
    Subgraph sg = Subgraph::CreateFull(graph);
    size_t diameter = (size_t{1} << depth) + 64;
    int rc = RunGripStages("sierpinski-tetra", std::move(sg), dim, diameter, maxStages);
    return rc < 0 ? 1 : 0;
  }

  if (mode == "edges") {
    std::string path = argc > 4 ? argv[4] : "data/human-jung-2015/data.edges";
    size_t diameter = argc > 5 ? static_cast<size_t>(std::atoi(argv[5])) : 64;

    Graph graph(false, 1);
    std::optional<Subgraph> sg;
    try {
      LoadEdgesLargestCC(path, graph, sg);
    } catch (const std::exception &e) {
      std::fprintf(stderr, "load failed: %s\n", e.what());
      return 1;
    }

    // Matches the old C tool's own call here (printGraphStats("largest CC",
    // sg.g)): sg.g was always just the same parent graph pointer, so this
    // reprints the FULL graph's degree stats under the "largest CC" label,
    // not stats scoped to the induced subgraph -- preserved as-is rather
    // than "fixed", since it's the old tool's actual (if misleadingly
    // labeled) behavior.
    PrintGraphStats("largest CC", graph);

    int rc = RunGripStages(path, std::move(*sg), dim, diameter, maxStages);
    return rc < 0 ? 1 : 0;
  }

  std::fprintf(stderr, "usage: %s sierpinski [dim] [maxStages] [depth]\n", argv[0]);
  std::fprintf(stderr, "       %s edges [dim] [maxStages] [path] [diameter]\n", argv[0]);
  return 1;
}
