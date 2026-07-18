#include "embedders/gvizFRPairwiseEmbedder.h"
#include "core/alloc.h"
#include "core/gvizVec.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizForceDirected.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FR_PAIRWISE_PARALLEL_GRAIN 1

static double defaultBoxExtent(size_t vertexCount, size_t dim,
                               double edgeLength) {
  if (vertexCount == 0)
    return edgeLength;
  double side = pow((double)vertexCount, 1.0 / (double)dim);
  return 0.5 * side * edgeLength;
}

// Writes only disp[begin..end), attForceMag[begin..end), and
// repForceMag[begin..end) from positions that are only read, so this is safe
// to run concurrently over disjoint ranges once state->pool is set.
static void computeForceRange(void *ctx, size_t begin, size_t end) {
  gvizFRPairwiseState *state = ctx;
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  size_t dim = embedding->embedding.dim;

  for (size_t i = begin; i < end; i++) {
    size_t v = state->vertices[i];
    double *f = state->disp + i * dim;
    gvizVecZero(dim, f);
    double attF[dim];
    double repF[dim];
    gvizVecZero(dim, attF);
    gvizVecZero(dim, repF);
    double *vPos = gvizEmbeddedGraphGetVPosition(embedding, v);

    for (size_t j = 0; j < state->vertexCount; j++) {
      if (j == i)
        continue;
      size_t u = state->vertices[j];
      double *uPos = gvizEmbeddedGraphGetVPosition(embedding, u);
      if (gvizSubgraphHasEdge(sg, v, u))
        gvizPairwiseFRAttForce(dim, vPos, uPos, state->edgeLength, attF);
      else
        gvizPairwiseFRRepForce(dim, vPos, uPos, state->edgeLength, repF);
    }

    gvizVecAxpy(dim, 1.0, attF, f);
    gvizVecAxpy(dim, 1.0, repF, f);
    state->attForceMag[i] = gvizVecNorm2(dim, attF);
    state->repForceMag[i] = gvizVecNorm2(dim, repF);
  }
}

static void frPairwiseActionStep(gvizEmbeddedGraph *embedding, void *userData,
                                 const gvizActionPayload *payload) {
  (void)userData;
  (void)payload;
  gvizFRPairwiseState *state = (gvizFRPairwiseState *)embedding;
  if (!state->begun)
    return;
  gvizFRPairwiseEmbedderStep(state);
}

int gvizFRPairwiseEmbedderInit(gvizFRPairwiseState *state,
                               gvizSubgraph subgraph, size_t dimension) {
  memset(state, 0, sizeof(*state));

  int res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, subgraph,
                                  dimension);
  if (res < 0)
    return res;

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  state->vertexCount = gvizSubgraphVertexCount(&embedding->subgraph);

  state->vertices = GVIZ_ALLOC(sizeof(size_t) * state->vertexCount);
  if (!state->vertices)
    return -1;

  size_t k = 0;
  gvizSubgraphVertexIterator vit =
      gvizSubgraphVertexIteratorCreate(&embedding->subgraph);
  size_t u;
  while (gvizSubgraphVertexIterate(&vit, &u))
    state->vertices[k++] = u;

  state->disp = GVIZ_ALLOC(sizeof(double) * state->vertexCount * dimension);
  if (!state->disp)
    return -1;

  state->attForceMag = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  state->repForceMag = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  if (!state->attForceMag || !state->repForceMag)
    return -1;

  state->edgeLength = GVIZ_FR_PAIRWISE_EDGE_LENGTH_DEFAULT;
  state->boxExtent =
      defaultBoxExtent(state->vertexCount, dimension, state->edgeLength);
  state->coolingFactor = GVIZ_FR_PAIRWISE_COOLING_FACTOR_DEFAULT;
  state->temperature = state->edgeLength * GVIZ_FR_PAIRWISE_INITIAL_TEMP_FACTOR;
  state->pool = NULL;

  if (gvizEmbeddedGraphAddAction(embedding, "frPairwise.step",
                                 frPairwiseActionStep, NULL) < 0)
    return -1;

  if (!gvizEmbeddedGraphAddStatSeries(embedding, "frPairwise.maxDisp",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "frPairwise.temperature",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "frPairwise.attractiveForce",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "frPairwise.repulsiveForce",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;

  return 0;
}

void gvizFRPairwiseEmbedderRelease(gvizFRPairwiseState *state) {
  gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)state);
  if (state->vertices)
    GVIZ_DEALLOC(state->vertices);
  if (state->disp)
    GVIZ_DEALLOC(state->disp);
  if (state->attForceMag)
    GVIZ_DEALLOC(state->attForceMag);
  if (state->repForceMag)
    GVIZ_DEALLOC(state->repForceMag);
  gvizThreadPoolDestroy(state->pool);
}

void gvizFRPairwiseEmbedderConfigure(gvizFRPairwiseState *state,
                                     double edgeLength, double boxExtent) {
  if (edgeLength > 0.0)
    state->edgeLength = edgeLength;
  if (boxExtent > 0.0)
    state->boxExtent = boxExtent;
}

void gvizFRPairwiseEmbedderConfigureCooling(gvizFRPairwiseState *state,
                                            double coolingFactor) {
  if (coolingFactor > 0.0)
    state->coolingFactor = coolingFactor;
}

void gvizFRPairwiseEmbedderBegin(gvizFRPairwiseState *state,
                                 unsigned int seed) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizEmbeddedGraphRandomizePositions(embedding, state->boxExtent, seed);

  state->temperature = state->edgeLength * GVIZ_FR_PAIRWISE_INITIAL_TEMP_FACTOR;
  state->iteration = 0;
  state->lastMaxDisplacement = 0.0;
  state->begun = 1;
}

double gvizFRPairwiseEmbedderStep(gvizFRPairwiseState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t dim = embedding->embedding.dim;

  gvizThreadPoolForRange(state->pool, 0, state->vertexCount,
                         FR_PAIRWISE_PARALLEL_GRAIN, computeForceRange,
                         state);

  // Raw FR forces grow with distance (see the note in computeForceRange), so
  // an unclamped displacement can overshoot equilibrium and diverge; capping
  // each vertex's per-round move at the current temperature is the standard
  // FR temperature limit. Without cooling it further, that cap alone lets two
  // vertices settle into a fixed-amplitude back-and-forth instead of
  // converging, so the temperature is cooled every round.
  double cap = state->temperature;
  double maxDisp = 0.0;
  for (size_t i = 0; i < state->vertexCount; i++) {
    double *f = state->disp + i * dim;
    double nrm = gvizVecNorm2(dim, f);
    if (nrm > cap)
      gvizVecScale(dim, cap / nrm, f);
    double applied = gvizVecNorm2(dim, f);
    if (applied > maxDisp)
      maxDisp = applied;
    gvizEmbeddedGraphAddVPosition(embedding, state->vertices[i], f);
  }

  double sumAtt = 0.0, sumRep = 0.0;
  for (size_t i = 0; i < state->vertexCount; i++) {
    sumAtt += state->attForceMag[i];
    sumRep += state->repForceMag[i];
  }
  double meanAtt = state->vertexCount ? sumAtt / state->vertexCount : 0.0;
  double meanRep = state->vertexCount ? sumRep / state->vertexCount : 0.0;

  double minTemp = state->edgeLength * GVIZ_FR_PAIRWISE_MIN_TEMP_FACTOR;
  state->temperature = fmax(state->temperature * state->coolingFactor, minTemp);

  state->iteration++;
  state->lastMaxDisplacement = maxDisp;
  gvizEmbeddedGraphStatAppend(embedding, "frPairwise.maxDisp", maxDisp);
  gvizEmbeddedGraphStatAppend(embedding, "frPairwise.temperature", cap);
  gvizEmbeddedGraphStatAppend(embedding, "frPairwise.attractiveForce", meanAtt);
  gvizEmbeddedGraphStatAppend(embedding, "frPairwise.repulsiveForce", meanRep);

  return maxDisp;
}

int gvizFRPairwiseEmbedderRun(gvizFRPairwiseState *state, size_t maxIters,
                              double epsilon) {
  if (!state->begun)
    return -1;

  size_t rounds = 0;
  while (rounds < maxIters) {
    double maxDisp = gvizFRPairwiseEmbedderStep(state);
    rounds++;
    if (maxDisp < epsilon)
      break;
  }
  return (int)rounds;
}
