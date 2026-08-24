// C++ port of tests/embedders/gvizGRIPLayerProbe.c: inspects per-layer MIS
// filtration/refinement state round by round (center-of-mass drift,
// coherence, gyration, rotational coherence, and a graph-distance-vs-
// Euclidean-distance "folding" metric) while driving a Sierpinski
// tetrahedron through GRIP by hand. Not a Unity test -- a standalone probe
// driver, same as the old C tool (see README.md's "Benchmarks and debug
// tools" section).
//
// Port notes:
//   - state->misFiltration[i] / gripMisBorderAt(state, layer) /
//     s->dec[v].disp are read through GRIP's new FiltrationVertexAt/
//     LayerBorder/Displacement accessors (see GRIP.hpp's DEBUG /
//     INTROSPECTION section) -- the exact three fields this tool reads from
//     the old gvizGRIPState/gvizGRIPDecorators, and nothing more.
//   - gvizSearchBreadthFirst(sg, &bfs, src, 0, distances) becomes
//     search::BreadthFirstTree(grip.Structure(), out, src, 0, &distances); `out`
//     is a fresh Subgraph::CreateEmpty(graph) each call, using the driver's
//     own retained Graph (GRIP only holds a reference to it via the moved
//     Subgraph, never hands the parent graph back out -- see Subgraph.hpp).

#include "GRIP.hpp"

#include "BreadthFirst.hpp"
#include "Graph.hpp"
#include "Graphs.hpp"
#include "Subgraph.hpp"
#include "Vec.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using gviz::Graph;
using gviz::Subgraph;
using gviz::VecNorm2;
using gviz::graphs::CreateSierpinskiTetrahedron;
using gviz::layout::GRIP;
namespace search = gviz::search;

namespace {

struct RoundMetrics {
  double com[3] = {0, 0, 0};
  double drift = 0.0;
  double meanDisp = 0.0;
  double coherence = 0.0;
  double gyration = 0.0;
  double rotCoherence = 0.0;
};

size_t ActiveCount(const GRIP<Subgraph> &grip) { return grip.LayerBorder(grip.CurrentLayer()); }

void ComputeCOM(GRIP<Subgraph> &grip, double *com) {
  size_t dim = grip.Dim();
  size_t n = ActiveCount(grip);
  std::memset(com, 0, sizeof(double) * dim);
  for (size_t i = 0; i < n; i++) {
    const double *p = grip.GetVPosition(grip.FiltrationVertexAt(i));
    for (size_t d = 0; d < dim; d++)
      com[d] += p[d];
  }
  for (size_t d = 0; d < dim; d++)
    com[d] /= static_cast<double>(n);
}

RoundMetrics MeasureRound(GRIP<Subgraph> &grip) {
  size_t dim = grip.Dim();
  size_t n = ActiveCount(grip);
  RoundMetrics m;

  double meanDispVec[3] = {0, 0, 0};
  double sumDispNorm = 0.0;
  for (size_t i = 0; i < n; i++) {
    auto d = grip.Displacement(grip.FiltrationVertexAt(i));
    for (size_t k = 0; k < dim; k++)
      meanDispVec[k] += d[k];
    sumDispNorm += VecNorm2(dim, d.data());
  }
  for (size_t k = 0; k < dim; k++)
    meanDispVec[k] /= static_cast<double>(n);

  m.drift = VecNorm2(dim, meanDispVec);
  m.meanDisp = sumDispNorm / static_cast<double>(n);
  m.coherence = m.meanDisp > 1e-12 ? m.drift / m.meanDisp : 0.0;

  ComputeCOM(grip, m.com);

  double sumR2 = 0.0;
  double angMom[3] = {0, 0, 0};
  double sumRD = 0.0;
  for (size_t i = 0; i < n; i++) {
    size_t v = grip.FiltrationVertexAt(i);
    const double *p = grip.GetVPosition(v);
    auto d = grip.Displacement(v);
    double r[3] = {0, 0, 0};
    for (size_t k = 0; k < dim; k++)
      r[k] = p[k] - m.com[k];
    double rn = VecNorm2(dim, r);
    double dn = VecNorm2(dim, d.data());
    sumR2 += rn * rn;
    sumRD += rn * dn;
    if (dim == 3) {
      angMom[0] += r[1] * d[2] - r[2] * d[1];
      angMom[1] += r[2] * d[0] - r[0] * d[2];
      angMom[2] += r[0] * d[1] - r[1] * d[0];
    } else if (dim == 2) {
      angMom[2] += r[0] * d[1] - r[1] * d[0];
    }
  }
  m.gyration = std::sqrt(sumR2 / static_cast<double>(n));
  double L = VecNorm2(3, angMom);
  m.rotCoherence = sumRD > 1e-12 ? L / sumRD : 0.0;
  return m;
}

/**
 * Folding metric: BFS from a few active vertices, then for active vertices
 * at graph distance >= minGd compare Euclidean distance to gd * EDGE_LENGTH.
 * A healthy unfolded embedding keeps the ratio near a constant; folding
 * shows up as the min (and mean) ratio collapsing toward 0.
 */
void FoldMetric(GRIP<Subgraph> &grip, const Graph &graph, size_t minGd, double *meanRatio,
                 double *minRatio) {
  size_t dim = grip.Dim();
  size_t n = ActiveCount(grip);
  size_t nvertices = graph.Size();
  static std::vector<size_t> distances;
  if (distances.empty())
    distances.resize(nvertices);

  double sum = 0.0, mn = 1e300;
  size_t cnt = 0;
  size_t sources = n < 6 ? n : 6;
  for (size_t si = 0; si < sources; si++) {
    size_t src = grip.FiltrationVertexAt((si * 2654435761u) % n);
    Subgraph bfs = Subgraph::CreateEmpty(graph);
    search::BreadthFirstTree(grip.Structure(), bfs, src, 0, &distances);
    const double *ps = grip.GetVPosition(src);
    for (size_t i = 0; i < n; i++) {
      size_t v = grip.FiltrationVertexAt(i);
      size_t gd = distances[v];
      if (gd == SIZE_MAX || gd < minGd)
        continue;
      const double *pv = grip.GetVPosition(v);
      double d2 = 0.0;
      for (size_t k = 0; k < dim; k++) {
        double dd = pv[k] - ps[k];
        d2 += dd * dd;
      }
      double ratio = std::sqrt(d2) / (static_cast<double>(gd) * 10.0);
      sum += ratio;
      if (ratio < mn)
        mn = ratio;
      cnt++;
    }
  }
  *meanRatio = cnt ? sum / static_cast<double>(cnt) : 0.0;
  *minRatio = cnt ? mn : 0.0;
}

} // namespace

int main(int argc, char **argv) {
  int depth = argc > 1 ? std::atoi(argv[1]) : 7;
  size_t dim = argc > 2 ? static_cast<size_t>(std::atoi(argv[2])) : 3;
  std::string policyName = argc > 3 ? argv[3] : "constant";
  size_t placementK = argc > 4 ? static_cast<size_t>(std::atoi(argv[4])) : 128;
  size_t refinementK = argc > 5 ? static_cast<size_t>(std::atoi(argv[5])) : 128;
  size_t rounds = argc > 6 ? static_cast<size_t>(std::atoi(argv[6])) : 60;
  size_t minLayer = argc > 7 ? static_cast<size_t>(std::atoi(argv[7])) : 0;

  GRIP<Subgraph>::KPolicy policy = GRIP<Subgraph>::KPolicy::Constant;
  if (policyName == "decay")
    policy = GRIP<Subgraph>::KPolicy::LayerDecay;
  else if (policyName == "grow")
    policy = GRIP<Subgraph>::KPolicy::LayerGrow;
  else if (policyName == "pdecay")
    policy = GRIP<Subgraph>::KPolicy::PlacementDecay;
  else if (policyName == "budget")
    policy = GRIP<Subgraph>::KPolicy::Budget;

  Graph graph = CreateSierpinskiTetrahedron(depth);
  graph.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(graph);

  GRIP grip(std::move(sg), static_cast<size_t>(std::pow(2, depth)) + 64, dim);
  grip.ConfigureK(placementK, refinementK, policy);
  grip.Begin();

  std::printf("layers=%zu policy=%s pK=%zu rK=%zu rounds=%zu minLayer=%zu\n",
              grip.LayerCount(), policyName.c_str(), placementK, refinementK, rounds,
              minLayer);
  std::printf("%5s %5s %8s | %10s %10s %6s | %10s %8s | %10s | %7s %7s\n", "layer",
              "round", "active", "drift", "meanDisp", "coher", "gyration", "rotCoh",
              "cumDrift", "foldAvg", "foldMin");

  while (true) {
    size_t layer = grip.CurrentLayer();
    double com0[3];
    ComputeCOM(grip, com0);

    for (size_t r = 0; r < rounds; r++) {
      grip.RefineRound();
      RoundMetrics m = MeasureRound(grip);
      if (r < 3 || (r + 1) % 10 == 0 || r == rounds - 1) {
        double dcom[3] = {m.com[0] - com0[0], m.com[1] - com0[1], m.com[2] - com0[2]};
        double cum = VecNorm2(3, dcom);
        double foldAvg, foldMin;
        FoldMetric(grip, graph, 4, &foldAvg, &foldMin);
        std::printf("%5zu %5zu %8zu | %10.4f %10.4f %6.3f | %10.2f %8.3f | %10.2f "
                    "| %7.3f %7.3f\n",
                    layer, r, ActiveCount(grip), m.drift, m.meanDisp, m.coherence,
                    m.gyration, m.rotCoherence, cum, foldAvg, foldMin);
      }
    }

    if (layer <= minLayer || grip.CurrentLayer() == 0)
      break;
    grip.NextStage();
  }

  return 0;
}
