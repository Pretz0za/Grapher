#include "EmbeddedGraph.hpp"
#include "Graph.hpp"
#include "Graphs.hpp"
#include "Planar.hpp"
#include "Subgraph.hpp"
#include "Tutte.hpp"

#include <cstdio>
#include <exception>
#include <limits>

using namespace gviz;
using namespace gviz::layout;

namespace {

bool SubgraphsMatchFace(const Subgraph &a, const Subgraph &b) {
  if (a.VertexCount() != b.VertexCount())
    return false;

  for (size_t u : a)
    if (!b.HasVertex(u))
      return false;

  for (size_t u : a) {
    for (size_t v : a.Neighbors(u)) {
      if (!a.HasEdge(u, v))
        continue;
      if (!b.HasEdge(u, v) && !b.HasEdge(v, u))
        return false;
    }
  }
  return true;
}

} // namespace

int main() {
  try {
    size_t rows = 6, cols = 6;
    Graph graph = graphs::BuildRectMesh(rows, cols);
    graph.BuildLayout();

    Tutte tutte(graph, Subgraph::CreateFull(graph), 2);
    tutte.Begin();
    tutte.Run(5000);

    EmbeddedGraph &eg = tutte;

    std::printf("planar=%d converged=%d\n", eg.IsPlanarEmbedded(),
                tutte.Converged());

    int interiorMiss = 0;
    int interiorWrong = 0;
    int outsideMiss = 0;

    for (size_t i = 1; i + 1 < rows; i++) {
      for (size_t j = 1; j + 1 < cols; j++) {
        size_t c[4] = {i * cols + j, i * cols + j + 1,
                        (i + 1) * cols + j + 1, (i + 1) * cols + j};
        const double *a = eg.GetVPosition(c[0]);
        const double *b = eg.GetVPosition(c[1]);
        const double *d = eg.GetVPosition(c[2]);
        const double *e = eg.GetVPosition(c[3]);
        double wx = (a[0] + b[0] + d[0] + e[0]) * 0.25;
        double wy = (a[1] + b[1] + d[1] + e[1]) * 0.25;

        Subgraph expected = Subgraph::CreateEmpty(graph);
        for (size_t t = 0; t < 4; t++)
          expected.ShowVertex(c[t]);
        expected.ShowEdge(c[0], c[1]);
        expected.ShowEdge(c[1], c[2]);
        expected.ShowEdge(c[2], c[3]);
        expected.ShowEdge(c[3], c[0]);

        std::optional<Subgraph> picked = FaceSubgraphAt(graph, eg, wx, wy);
        if (!picked) {
          interiorMiss++;
          continue;
        }

        if (!SubgraphsMatchFace(*picked, expected))
          interiorWrong++;
      }
    }

    double bminX = std::numeric_limits<double>::infinity();
    double bminY = std::numeric_limits<double>::infinity();
    for (size_t u = 0; u < graph.Size(); u++) {
      const double *p = eg.GetVPosition(u);
      if (p[0] < bminX)
        bminX = p[0];
      if (p[1] < bminY)
        bminY = p[1];
    }

    std::optional<Subgraph> outside =
        FaceSubgraphAt(graph, eg, bminX - 10.0, bminY - 10.0);
    if (!outside)
      outsideMiss = 1;

    std::printf("Summary: interiorMiss=%d interiorWrong=%d outsideMiss=%d\n",
                interiorMiss, interiorWrong, outsideMiss);

    return (interiorMiss || interiorWrong || outsideMiss) ? 1 : 0;
  } catch (const std::exception &e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
}
