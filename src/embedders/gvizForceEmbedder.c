#include "embedders/gvizForceEmbedder.h"
#include "core/alloc.h"
#include "core/gvizVec.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizForceDirected.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FORCE_EMBEDDER_PARALLEL_GRAIN 1

static double defaultBoxExtent(size_t vertexCount, double edgeLength) {
  if (vertexCount == 0)
    return edgeLength;
  double side = sqrt((double)vertexCount);
  return 0.5 * side * edgeLength;
}

static void gatherPositions(gvizForceEmbedderState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  for (size_t i = 0; i < state->vertexCount; i++) {
    double *vPos = gvizEmbeddedGraphGetVPosition(embedding, state->vertices[i]);
    state->positionsScratch[2 * i] = vPos[0];
    state->positionsScratch[2 * i + 1] = vPos[1];
  }
}

static double vertexRadiusIfEnabled(const gvizForceEmbedderState *state,
                                    size_t idx) {
  if (!state->preventOverlap)
    return 0.0;
  return gvizForceEmbedderVertexRadius(state, idx);
}

static void bhAccumulateRepulsion(gvizForceEmbedderState *state,
                                  const gvizQuadtreeNode *node, size_t selfIdx,
                                  double vRadius, double *vPos, double *acc) {
  if (!node || node->mass == 0)
    return;

  if (gvizQuadtreeNodeIsLeaf(node)) {
    size_t count = gvizQuadtreeNodePointCount(node);
    for (size_t i = 0; i < count; i++) {
      size_t idx = gvizQuadtreeNodePointAt(node, i);
      if (idx == selfIdx)
        continue;
      double *uPos = state->positionsScratch + idx * 2;
      double otherRadius = vertexRadiusIfEnabled(state, idx);
      state->model->repulsive(2, vPos, uPos, state->mass[selfIdx],
                              state->mass[idx], vRadius, otherRadius,
                              state->overlapConstant, state->edgeLength, acc);
    }
    return;
  }

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(node, &comX, &comY);
  double dx = comX - vPos[0];
  double dy = comY - vPos[1];
  double dist = sqrt(dx * dx + dy * dy);
  double ratio = (2.0 * node->halfSize) / dist;

  if (ratio < state->theta) {
    double com[2] = {comX, comY};
    state->model->repulsive(2, vPos, com, state->mass[selfIdx], node->mass,
                            vRadius, 0.0, state->overlapConstant,
                            state->edgeLength, acc);
    return;
  }

  for (int q = 0; q < GVIZ_QUAD_COUNT; q++)
    bhAccumulateRepulsion(state, gvizQuadtreeNodeChild(node, (gvizQuadrant)q),
                          selfIdx, vRadius, vPos, acc);
}

static void computeForceRange(void *ctx, size_t begin, size_t end) {
  gvizForceEmbedderState *state = ctx;
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  const gvizSubgraph *sg = &embedding->subgraph;
  const gvizQuadtreeNode *root =
      state->barnesHutEnabled ? gvizQuadtreeRoot(&state->quadtree) : NULL;

  for (size_t i = begin; i < end; i++) {
    size_t v = state->vertices[i];
    double *f = state->disp + i * 2;
    gvizVecZero(2, f);
    double attF[2];
    double repF[2];
    gvizVecZero(2, attF);
    gvizVecZero(2, repF);
    double *vPos = state->positionsScratch + i * 2;

    gvizSubgraphNeighborIterator nit =
        gvizSubgraphNeighborIteratorCreate(sg, v);
    size_t u;
    while (gvizSubgraphNeighborIterate(&nit, &u)) {
      double *uPos = gvizEmbeddedGraphGetVPosition(embedding, u);
      state->model->attractive(2, vPos, uPos, state->edgeLength, attF);
    }

    double vRadius = vertexRadiusIfEnabled(state, i);
    if (state->barnesHutEnabled) {
      bhAccumulateRepulsion(state, root, i, vRadius, vPos, repF);
    } else {
      for (size_t j = 0; j < state->vertexCount; j++) {
        if (j == i)
          continue;
        double *otherPos = state->positionsScratch + j * 2;
        double otherRadius = vertexRadiusIfEnabled(state, j);
        state->model->repulsive(2, vPos, otherPos, state->mass[i],
                                state->mass[j], vRadius, otherRadius,
                                state->overlapConstant, state->edgeLength,
                                repF);
      }
    }

    gvizVecAxpy(2, 1.0, attF, f);
    gvizVecAxpy(2, 1.0, repF, f);
    state->attForceMag[i] = gvizVecNorm2(2, attF);
    state->repForceMag[i] = gvizVecNorm2(2, repF);
    gvizVecCopy(2, f, state->structForce + i * 2);

    double gravityMag = state->gravityK * (double)(state->degree[i] + 1);
    gvizPairwiseGravityForce(2, vPos, gravityMag, f);
  }
}

static void computeSwingTractionRange(void *ctx, size_t begin, size_t end) {
  gvizForceEmbedderState *state = ctx;
  for (size_t i = begin; i < end; i++) {
    double *f = state->structForce + i * 2;
    double *old = state->oldStructForce + i * 2;
    double diff[2] = {f[0] - old[0], f[1] - old[1]};
    double sum[2] = {f[0] + old[0], f[1] + old[1]};
    state->swinging[i] = state->mass[i] * gvizVecNorm2(2, diff);
    state->traction[i] = state->mass[i] * 0.5 * gvizVecNorm2(2, sum);
  }
}

static void updateGlobalSpeed(gvizForceEmbedderState *state) {
  double totalSwinging = 0.0, totalEffectiveTraction = 0.0;
  for (size_t i = 0; i < state->vertexCount; i++) {
    totalSwinging += state->swinging[i];
    totalEffectiveTraction += state->traction[i];
  }

  if (totalEffectiveTraction < gvizNumericEpsilon)
    return;

  double n = (double)state->vertexCount;
  double estimatedOptimalJT = 0.05 * sqrt(n);
  double minJT = sqrt(estimatedOptimalJT);
  double maxJT = 10.0;
  double jt = state->jitterTolerance *
              fmax(minJT, fmin(maxJT, estimatedOptimalJT *
                                          totalEffectiveTraction / (n * n)));

  if (totalSwinging / totalEffectiveTraction > 2.0) {
    if (state->speedEfficiency > GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_MIN)
      state->speedEfficiency *= 0.5;
    jt = fmax(jt, state->jitterTolerance);
  }

  double targetSpeed =
      totalSwinging > gvizNumericEpsilon
          ? jt * state->speedEfficiency * totalEffectiveTraction /
                totalSwinging
          : state->globalSpeed;

  if (totalSwinging > jt * totalEffectiveTraction) {
    if (state->speedEfficiency > GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_MIN)
      state->speedEfficiency *= 0.7;
  } else if (state->globalSpeed < 1000.0) {
    state->speedEfficiency *= 1.3;
  }

  if (state->speedEfficiency < GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_MIN)
    state->speedEfficiency = GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_MIN;
  else if (state->speedEfficiency > GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_MAX)
    state->speedEfficiency = GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_MAX;

  state->globalSpeed +=
      fmin(targetSpeed - state->globalSpeed,
          GVIZ_FORCE_EMBEDDER_SPEED_MAX_RISE * state->globalSpeed);
}

static void applySpeedRange(void *ctx, size_t begin, size_t end) {
  gvizForceEmbedderState *state = ctx;
  for (size_t i = begin; i < end; i++) {
    double factor = state->globalSpeed /
                    (1.0 + sqrt(state->globalSpeed * state->swinging[i]));
    double *f = state->disp + i * 2;
    double *out = state->appliedDisp + i * 2;
    out[0] = f[0] * factor;
    out[1] = f[1] * factor;
  }
}

static void forceEmbedderActionStep(gvizEmbeddedGraph *embedding,
                                    void *userData,
                                    const gvizActionPayload *payload) {
  (void)userData;
  (void)payload;
  gvizForceEmbedderState *state = (gvizForceEmbedderState *)embedding;
  if (!state->begun)
    return;
  gvizForceEmbedderStep(state);
}

static void forceEmbedderActionToggleOverlapPrevention(
    gvizEmbeddedGraph *embedding, void *userData,
    const gvizActionPayload *payload) {
  (void)userData;
  (void)payload;
  gvizForceEmbedderState *state = (gvizForceEmbedderState *)embedding;
  state->preventOverlap = !state->preventOverlap;
}

int gvizForceEmbedderInit(gvizForceEmbedderState *state, gvizSubgraph subgraph,
                          size_t dimension, gvizForceModelKind model) {
  memset(state, 0, sizeof(*state));

  if (dimension != 2)
    return -2;

  int res =
      gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, subgraph, dimension);
  if (res < 0)
    return res;

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  state->vertexCount = gvizSubgraphVertexCount(&embedding->subgraph);
  state->model = gvizForceModelGet(model);

  state->vertices = GVIZ_ALLOC(sizeof(size_t) * state->vertexCount);
  if (!state->vertices)
    return -1;

  size_t k = 0;
  gvizSubgraphVertexIterator vit =
      gvizSubgraphVertexIteratorCreate(&embedding->subgraph);
  size_t u;
  while (gvizSubgraphVertexIterate(&vit, &u))
    state->vertices[k++] = u;

  state->degree = GVIZ_ALLOC(sizeof(size_t) * state->vertexCount);
  state->mass = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  if (!state->degree || !state->mass)
    return -1;

  double sumDegPlusOne = 0.0;
  for (size_t i = 0; i < state->vertexCount; i++) {
    state->degree[i] =
        gvizSubgraphDegree(&embedding->subgraph, state->vertices[i]);
    state->mass[i] = state->model->vertexMass(state->degree[i]);
    sumDegPlusOne += (double)(state->degree[i] + 1);
  }
  state->meanDegreePlusOne =
      state->vertexCount ? sumDegPlusOne / (double)state->vertexCount : 0.0;

  state->disp = GVIZ_ALLOC(sizeof(double) * state->vertexCount * 2);
  if (!state->disp)
    return -1;

  state->positionsScratch = GVIZ_ALLOC(sizeof(double) * state->vertexCount * 2);
  if (!state->positionsScratch)
    return -1;

  state->attForceMag = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  state->repForceMag = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  if (!state->attForceMag || !state->repForceMag)
    return -1;

  state->appliedDisp = GVIZ_ALLOC(sizeof(double) * state->vertexCount * 2);
  state->swinging = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  state->traction = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  state->structForce = GVIZ_ALLOC(sizeof(double) * state->vertexCount * 2);
  state->oldStructForce = GVIZ_ALLOC(sizeof(double) * state->vertexCount * 2);
  if (!state->appliedDisp || !state->swinging || !state->traction ||
      !state->structForce || !state->oldStructForce)
    return -1;

  state->edgeLength = GVIZ_FORCE_EMBEDDER_EDGE_LENGTH_DEFAULT;
  state->boxExtent = defaultBoxExtent(state->vertexCount, state->edgeLength);
  state->jitterTolerance = GVIZ_FORCE_EMBEDDER_JITTER_TOLERANCE_DEFAULT;
  state->theta = GVIZ_FORCE_EMBEDDER_THETA_DEFAULT;
  state->nodesPerCell = GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT;
  state->gravityK = 0.0;
  state->radiusBase = 0.0;
  state->radiusPerDegree = 0.0;
  state->overlapConstant = GVIZ_FORCE_EMBEDDER_OVERLAP_CONSTANT_DEFAULT;
  state->preventOverlap = 0;
  state->barnesHutEnabled = 1;
  // NULL pool is fine: parallel phases fall back to running serially
  state->pool = gvizThreadPoolCreate(0);
  state->quadtreeReady = 0;

  if (gvizEmbeddedGraphAddAction(embedding, "forceEmbedder.step",
                                 forceEmbedderActionStep, NULL) < 0)
    return -1;

  if (gvizEmbeddedGraphAddAction(embedding,
                                 "forceEmbedder.toggleOverlapPrevention",
                                 forceEmbedderActionToggleOverlapPrevention,
                                 NULL) < 0)
    return -1;

  if (!gvizEmbeddedGraphAddStatSeries(embedding, "forceEmbedder.maxDisp",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "forceEmbedder.speed",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(
          embedding, "forceEmbedder.attractiveForce", GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "forceEmbedder.repulsiveForce",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "forceEmbedder.gravityForce",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;

  return 0;
}

void gvizForceEmbedderRelease(gvizForceEmbedderState *state) {
  gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)state);
  if (state->vertices)
    GVIZ_DEALLOC(state->vertices);
  if (state->mass)
    GVIZ_DEALLOC(state->mass);
  if (state->degree)
    GVIZ_DEALLOC(state->degree);
  if (state->disp)
    GVIZ_DEALLOC(state->disp);
  if (state->positionsScratch)
    GVIZ_DEALLOC(state->positionsScratch);
  if (state->attForceMag)
    GVIZ_DEALLOC(state->attForceMag);
  if (state->repForceMag)
    GVIZ_DEALLOC(state->repForceMag);
  if (state->appliedDisp)
    GVIZ_DEALLOC(state->appliedDisp);
  if (state->swinging)
    GVIZ_DEALLOC(state->swinging);
  if (state->traction)
    GVIZ_DEALLOC(state->traction);
  if (state->structForce)
    GVIZ_DEALLOC(state->structForce);
  if (state->oldStructForce)
    GVIZ_DEALLOC(state->oldStructForce);
  gvizQuadtreeRelease(&state->quadtree);
  gvizThreadPoolDestroy(state->pool);
}

void gvizForceEmbedderConfigure(gvizForceEmbedderState *state,
                                double edgeLength, double boxExtent) {
  if (edgeLength > 0.0)
    state->edgeLength = edgeLength;
  if (boxExtent > 0.0)
    state->boxExtent = boxExtent;
}

void gvizForceEmbedderConfigureSpeed(gvizForceEmbedderState *state,
                                     double tolerance) {
  if (tolerance > 0.0)
    state->jitterTolerance = tolerance;
}

void gvizForceEmbedderConfigureBarnesHut(gvizForceEmbedderState *state,
                                         double theta, size_t nodesPerCell) {
  if (theta > 0.0)
    state->theta = theta;
  if (nodesPerCell > 0)
    state->nodesPerCell = nodesPerCell;
}

void gvizForceEmbedderSetBarnesHutEnabled(gvizForceEmbedderState *state,
                                          int enabled) {
  state->barnesHutEnabled = enabled;
}

void gvizForceEmbedderConfigureGravity(gvizForceEmbedderState *state,
                                       double k) {
  state->gravityK = k;
}

double gvizForceEmbedderVertexRadius(const gvizForceEmbedderState *state,
                                     size_t index) {
  return state->radiusBase *
         (1.0 + state->radiusPerDegree * sqrt((double)state->degree[index]));
}

void gvizForceEmbedderConfigureRadius(gvizForceEmbedderState *state,
                                      double base, double perDegree) {
  state->radiusBase = base;
  state->radiusPerDegree = perDegree;
}

void gvizForceEmbedderConfigureOverlapPrevention(gvizForceEmbedderState *state,
                                                 double constant) {
  if (constant > 0.0)
    state->overlapConstant = constant;
}

void gvizForceEmbedderSetPreventOverlapEnabled(gvizForceEmbedderState *state,
                                               int enabled) {
  state->preventOverlap = enabled;
}

int gvizForceEmbedderBegin(gvizForceEmbedderState *state, unsigned int seed) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizEmbeddedGraphRandomizePositions(embedding, state->boxExtent, seed);

  state->globalSpeed = GVIZ_FORCE_EMBEDDER_SPEED_INITIAL;
  state->speedEfficiency = GVIZ_FORCE_EMBEDDER_SPEED_EFFICIENCY_INITIAL;
  memset(state->disp, 0, sizeof(double) * state->vertexCount * 2);
  memset(state->structForce, 0, sizeof(double) * state->vertexCount * 2);
  memset(state->oldStructForce, 0, sizeof(double) * state->vertexCount * 2);

  state->iteration = 0;
  state->lastMaxDisplacement = 0.0;
  state->begun = 1;

  gatherPositions(state);

  int res = 0;
  if (state->barnesHutEnabled) {
    if (!state->quadtreeReady) {
      res = gvizQuadtreeInit(&state->quadtree, state->positionsScratch,
                             state->mass, state->vertexCount,
                             state->nodesPerCell);
      if (res == 0)
        state->quadtreeReady = 1;
    } else {
      res = gvizQuadtreeRebuild(&state->quadtree, state->positionsScratch,
                                state->mass, state->vertexCount);
    }
  }

  return res;
}

double gvizForceEmbedderStep(gvizForceEmbedderState *state) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  gatherPositions(state);
  gvizVecCopy(state->vertexCount * 2, state->structForce,
             state->oldStructForce);
  if (state->barnesHutEnabled)
    gvizQuadtreeRebuild(&state->quadtree, state->positionsScratch,
                        state->mass, state->vertexCount);

  gvizThreadPoolForRange(state->pool, 0, state->vertexCount,
                         FORCE_EMBEDDER_PARALLEL_GRAIN, computeForceRange,
                         state);

  gvizThreadPoolForRange(state->pool, 0, state->vertexCount,
                         FORCE_EMBEDDER_PARALLEL_GRAIN,
                         computeSwingTractionRange, state);
  updateGlobalSpeed(state);
  gvizThreadPoolForRange(state->pool, 0, state->vertexCount,
                         FORCE_EMBEDDER_PARALLEL_GRAIN, applySpeedRange,
                         state);

  double maxDisp = 0.0;
  double sumAtt = 0.0, sumRep = 0.0;
  for (size_t i = 0; i < state->vertexCount; i++) {
    double *f = state->appliedDisp + i * 2;
    double applied = gvizVecNorm2(2, f);
    if (applied > maxDisp)
      maxDisp = applied;
    sumAtt += state->attForceMag[i];
    sumRep += state->repForceMag[i];
    gvizEmbeddedGraphAddVPosition(embedding, state->vertices[i], f);
  }
  double meanAtt = state->vertexCount ? sumAtt / state->vertexCount : 0.0;
  double meanRep = state->vertexCount ? sumRep / state->vertexCount : 0.0;
  double meanGrav = state->gravityK * state->meanDegreePlusOne;

  state->iteration++;
  state->lastMaxDisplacement = maxDisp;
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.maxDisp", maxDisp);
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.speed",
                              state->globalSpeed);
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.attractiveForce",
                              meanAtt);
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.repulsiveForce",
                              meanRep);
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.gravityForce",
                              meanGrav);

  return maxDisp;
}

int gvizForceEmbedderRun(gvizForceEmbedderState *state, size_t maxIters,
                         double epsilon) {
  if (!state->begun)
    return -1;

  size_t rounds = 0;
  while (rounds < maxIters) {
    double maxDisp = gvizForceEmbedderStep(state);
    rounds++;
    if (maxDisp < epsilon)
      break;
  }
  return (int)rounds;
}
