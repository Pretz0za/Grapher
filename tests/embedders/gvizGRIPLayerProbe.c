#include "algorithms/search/gvizBreadthFirst.h"
#include "cblas.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "utils/graphs.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  double com[3];
  double drift;
  double meanDisp;
  double coherence;
  double gyration;
  double rotCoherence;
} RoundMetrics;

static size_t activeCount(const gvizGRIPState *s) {
  return s->misBorder[s->currLayer];
}

static void computeCOM(gvizGRIPState *s, double *com) {
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)s;
  size_t dim = eg->embedding.dim;
  size_t n = activeCount(s);
  memset(com, 0, sizeof(double) * dim);
  for (size_t i = 0; i < n; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, s->misFiltration[i]);
    for (size_t d = 0; d < dim; d++)
      com[d] += p[d];
  }
  for (size_t d = 0; d < dim; d++)
    com[d] /= (double)n;
}

static RoundMetrics measureRound(gvizGRIPState *s) {
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)s;
  size_t dim = eg->embedding.dim;
  size_t n = activeCount(s);
  RoundMetrics m;
  memset(&m, 0, sizeof(m));

  double meanDispVec[3] = {0, 0, 0};
  double sumDispNorm = 0.0;
  for (size_t i = 0; i < n; i++) {
    double *d = s->dec[s->misFiltration[i]].disp;
    for (size_t k = 0; k < dim; k++)
      meanDispVec[k] += d[k];
    sumDispNorm += cblas_dnrm2((int)dim, d, 1);
  }
  for (size_t k = 0; k < dim; k++)
    meanDispVec[k] /= (double)n;

  m.drift = cblas_dnrm2((int)dim, meanDispVec, 1);
  m.meanDisp = sumDispNorm / (double)n;
  m.coherence = m.meanDisp > 1e-12 ? m.drift / m.meanDisp : 0.0;

  computeCOM(s, m.com);

  double sumR2 = 0.0;
  double angMom[3] = {0, 0, 0};
  double sumRD = 0.0;
  for (size_t i = 0; i < n; i++) {
    size_t v = s->misFiltration[i];
    double *p = gvizEmbeddedGraphGetVPosition(eg, v);
    double *d = s->dec[v].disp;
    double r[3] = {0, 0, 0};
    for (size_t k = 0; k < dim; k++)
      r[k] = p[k] - m.com[k];
    double rn = cblas_dnrm2((int)dim, r, 1);
    double dn = cblas_dnrm2((int)dim, d, 1);
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
  m.gyration = sqrt(sumR2 / (double)n);
  double L = cblas_dnrm2(3, angMom, 1);
  m.rotCoherence = sumRD > 1e-12 ? L / sumRD : 0.0;
  return m;
}

/**
 * Folding metric: BFS from a few active vertices, then for active vertices at
 * graph distance >= minGd compare Euclidean distance to gd * EDGE_LENGTH.
 * A healthy unfolded embedding keeps the ratio near a constant; folding shows
 * up as the min (and mean) ratio collapsing toward 0.
 */
static void foldMetric(gvizGRIPState *s, size_t minGd, double *meanRatio,
                       double *minRatio) {
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)s;
  const gvizSubgraph *sg = &eg->subgraph;
  size_t dim = eg->embedding.dim;
  size_t n = activeCount(s);
  size_t nvertices = gvizGraphSize(sg->g);
  static size_t *distances = NULL;
  if (!distances)
    distances = malloc(sizeof(size_t) * nvertices);

  double sum = 0.0, mn = 1e300;
  size_t cnt = 0;
  size_t sources = n < 6 ? n : 6;
  for (size_t si = 0; si < sources; si++) {
    size_t src = s->misFiltration[(si * 2654435761u) % n];
    gvizSubgraph bfs = gvizSubgraphCreateEmpty(sg->g);
    gvizSearchBreadthFirst(sg, &bfs, src, 0, distances);
    gvizSubgraphRelease(&bfs);
    double *ps = gvizEmbeddedGraphGetVPosition(eg, src);
    for (size_t i = 0; i < n; i++) {
      size_t v = s->misFiltration[i];
      size_t gd = distances[v];
      if (gd == SIZE_MAX || gd < minGd)
        continue;
      double *pv = gvizEmbeddedGraphGetVPosition(eg, v);
      double d2 = 0.0;
      for (size_t k = 0; k < dim; k++) {
        double dd = pv[k] - ps[k];
        d2 += dd * dd;
      }
      double ratio = sqrt(d2) / ((double)gd * 10.0);
      sum += ratio;
      if (ratio < mn)
        mn = ratio;
      cnt++;
    }
  }
  *meanRatio = cnt ? sum / (double)cnt : 0.0;
  *minRatio = cnt ? mn : 0.0;
}

int main(int argc, char **argv) {
  int depth = argc > 1 ? atoi(argv[1]) : 7;
  size_t dim = argc > 2 ? (size_t)atoi(argv[2]) : 3;
  const char *policyName = argc > 3 ? argv[3] : "constant";
  size_t placementK = argc > 4 ? (size_t)atoi(argv[4]) : 128;
  size_t refinementK = argc > 5 ? (size_t)atoi(argv[5]) : 128;
  size_t rounds = argc > 6 ? (size_t)atoi(argv[6]) : 60;
  size_t minLayer = argc > 7 ? (size_t)atoi(argv[7]) : 0;

  gvizGRIPKPolicy policy = GVIZ_GRIP_K_CONSTANT;
  if (!strcmp(policyName, "decay"))
    policy = GVIZ_GRIP_K_LAYER_DECAY;
  else if (!strcmp(policyName, "grow"))
    policy = GVIZ_GRIP_K_LAYER_GROW;
  else if (!strcmp(policyName, "pdecay"))
    policy = GVIZ_GRIP_K_PLACEMENT_DECAY;
  else if (!strcmp(policyName, "budget"))
    policy = GVIZ_GRIP_K_BUDGET;

  gvizGraph graph = createSierpinskiTetrahedron(depth, NULL);
  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);

  gvizGRIPState state;
  if (gvizGRIPEmbedderInit(&state, sg, (size_t)pow(2, depth) + 64, dim) < 0) {
    fprintf(stderr, "init failed\n");
    return 1;
  }
  gvizGRIPEmbedderConfigureK(&state, placementK, refinementK, 256, policy);
  gvizGRIPEmbedderBegin(&state);

  printf("layers=%zu policy=%s pK=%zu rK=%zu rounds=%zu minLayer=%zu\n",
         state.layerCount, policyName, placementK, refinementK, rounds,
         minLayer);
  printf("%5s %5s %8s | %10s %10s %6s | %10s %8s | %10s | %7s %7s\n", "layer",
         "round", "active", "drift", "meanDisp", "coher", "gyration", "rotCoh",
         "cumDrift", "foldAvg", "foldMin");

  while (1) {
    size_t layer = state.currLayer;
    double com0[3];
    computeCOM(&state, com0);

    for (size_t r = 0; r < rounds; r++) {
      runRefinementRound(&state);
      RoundMetrics m = measureRound(&state);
      if (r < 3 || (r + 1) % 10 == 0 || r == rounds - 1) {
        double dcom[3] = {m.com[0] - com0[0], m.com[1] - com0[1],
                          m.com[2] - com0[2]};
        double cum = cblas_dnrm2(3, dcom, 1);
        double foldAvg, foldMin;
        foldMetric(&state, 4, &foldAvg, &foldMin);
        printf("%5zu %5zu %8zu | %10.4f %10.4f %6.3f | %10.2f %8.3f | %10.2f "
               "| %7.3f %7.3f\n",
               layer, r, activeCount(&state), m.drift, m.meanDisp, m.coherence,
               m.gyration, m.rotCoherence, cum, foldAvg, foldMin);
      }
    }

    if (layer <= minLayer || state.currLayer == 0)
      break;
    beginNewStage(&state);
  }

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
  return 0;
}
