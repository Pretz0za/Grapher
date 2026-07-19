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

static void bhAccumulateRepulsion(gvizForceEmbedderState *state,
                                  const gvizQuadtreeNode *node, size_t selfIdx,
                                  double *vPos, double *acc) {
  if (!node || node->mass == 0)
    return;

  if (gvizQuadtreeNodeIsLeaf(node)) {
    size_t count = gvizQuadtreeNodePointCount(node);
    for (size_t i = 0; i < count; i++) {
      size_t idx = gvizQuadtreeNodePointAt(node, i);
      if (idx == selfIdx)
        continue;
      double *uPos = state->positionsScratch + idx * 2;
      state->model->repulsive(2, vPos, uPos, state->mass[selfIdx],
                              state->mass[idx], state->edgeLength, acc);
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
                            state->edgeLength, acc);
    return;
  }

  for (int q = 0; q < GVIZ_QUAD_COUNT; q++)
    bhAccumulateRepulsion(state, gvizQuadtreeNodeChild(node, (gvizQuadrant)q),
                          selfIdx, vPos, acc);
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

    if (state->barnesHutEnabled) {
      bhAccumulateRepulsion(state, root, i, vPos, repF);
    } else {
      for (size_t j = 0; j < state->vertexCount; j++) {
        if (j == i)
          continue;
        double *otherPos = state->positionsScratch + j * 2;
        state->model->repulsive(2, vPos, otherPos, state->mass[i],
                                state->mass[j], state->edgeLength, repF);
      }
    }

    gvizVecAxpy(2, 1.0, attF, f);
    gvizVecAxpy(2, 1.0, repF, f);
    state->attForceMag[i] = gvizVecNorm2(2, attF);
    state->repForceMag[i] = gvizVecNorm2(2, repF);

    double gravityMag = state->gravityK * (double)(state->degree[i] + 1);
    gvizPairwiseGravityForce(2, vPos, gravityMag, f);
  }
}

// Rigid translation is a zero mode of the force model (every pairwise force
// is radial, so the total sums to zero under a uniform shift), but the heat
// model rewards a vertex whose displacement direction stays consistent round
// to round -- so any residual net force (e.g. from Barnes-Hut approximation
// error) gets amplified by heat instead of averaging out, and the whole
// layout drifts indefinitely. Subtracting the mean displacement pins the
// centroid without altering any vertex's motion relative to the others.
static void removeNetTranslation(gvizForceEmbedderState *state) {
  if (state->vertexCount == 0)
    return;

  double mean[2] = {0.0, 0.0};
  for (size_t i = 0; i < state->vertexCount; i++)
    gvizVecAxpy(2, 1.0, state->disp + i * 2, mean);
  gvizVecScale(2, 1.0 / (double)state->vertexCount, mean);

  for (size_t i = 0; i < state->vertexCount; i++)
    gvizVecAxpy(2, -1.0, mean, state->disp + i * 2);
}

static void updateHeatRange(void *ctx, size_t begin, size_t end) {
  gvizForceEmbedderState *state = ctx;
  double heatMin = state->edgeLength * GVIZ_FORCE_EMBEDDER_HEAT_MIN_FACTOR;
  double heatMax = state->edgeLength * GVIZ_FORCE_EMBEDDER_HEAT_MAX_FACTOR;

  for (size_t i = begin; i < end; i++) {
    double *f = state->disp + i * 2;
    double *old = state->oldDisp + i * 2;
    double nrm = gvizVecNorm2(2, f);
    double oldNrm = gvizVecNorm2(2, old);

    if (nrm > gvizNumericEpsilon && oldNrm > gvizNumericEpsilon) {
      double cos = gvizVecDot(2, f, old) / (nrm * oldNrm);
      if (cos > 0.0 && state->oldCos[i] > 0.0)
        state->heat[i] *= (1.0 + cos * state->heatR * state->heatS);
      else
        state->heat[i] *= (1.0 + cos * state->heatR);
      state->oldCos[i] = cos;
    }

    state->heat[i] = fmin(state->heat[i], heatMax);
    state->heat[i] = fmax(state->heat[i], heatMin);

    if (nrm > gvizNumericEpsilon)
      gvizVecScale(2, state->heat[i] / nrm, f);
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

  state->heat = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  state->oldDisp = GVIZ_ALLOC(sizeof(double) * state->vertexCount * 2);
  state->oldCos = GVIZ_ALLOC(sizeof(double) * state->vertexCount);
  if (!state->heat || !state->oldDisp || !state->oldCos)
    return -1;

  state->edgeLength = GVIZ_FORCE_EMBEDDER_EDGE_LENGTH_DEFAULT;
  state->boxExtent = defaultBoxExtent(state->vertexCount, state->edgeLength);
  state->heatR = GVIZ_FORCE_EMBEDDER_HEAT_R_DEFAULT;
  state->heatS = GVIZ_FORCE_EMBEDDER_HEAT_S_DEFAULT;
  state->theta = GVIZ_FORCE_EMBEDDER_THETA_DEFAULT;
  state->nodesPerCell = GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT;
  state->gravityK = 0.0;
  state->barnesHutEnabled = 1;
  state->pool = NULL;
  state->quadtreeReady = 0;

  if (gvizEmbeddedGraphAddAction(embedding, "forceEmbedder.step",
                                 forceEmbedderActionStep, NULL) < 0)
    return -1;

  if (!gvizEmbeddedGraphAddStatSeries(embedding, "forceEmbedder.maxDisp",
                                      GVIZ_STAT_CHART_LINE_LOG))
    return -1;
  if (!gvizEmbeddedGraphAddStatSeries(embedding, "forceEmbedder.heat",
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
  if (state->heat)
    GVIZ_DEALLOC(state->heat);
  if (state->oldDisp)
    GVIZ_DEALLOC(state->oldDisp);
  if (state->oldCos)
    GVIZ_DEALLOC(state->oldCos);
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

void gvizForceEmbedderConfigureHeat(gvizForceEmbedderState *state, double r,
                                    double s) {
  if (r > 0.0)
    state->heatR = r;
  if (s > 0.0)
    state->heatS = s;
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

int gvizForceEmbedderBegin(gvizForceEmbedderState *state, unsigned int seed) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizEmbeddedGraphRandomizePositions(embedding, state->boxExtent, seed);

  double initialHeat =
      state->edgeLength * GVIZ_FORCE_EMBEDDER_HEAT_INITIAL_FACTOR;
  for (size_t i = 0; i < state->vertexCount; i++)
    state->heat[i] = initialHeat;
  memset(state->disp, 0, sizeof(double) * state->vertexCount * 2);
  memset(state->oldDisp, 0, sizeof(double) * state->vertexCount * 2);
  memset(state->oldCos, 0, sizeof(double) * state->vertexCount);

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
  gvizVecCopy(state->vertexCount * 2, state->disp, state->oldDisp);
  if (state->barnesHutEnabled)
    gvizQuadtreeRebuild(&state->quadtree, state->positionsScratch,
                        state->mass, state->vertexCount);

  gvizThreadPoolForRange(state->pool, 0, state->vertexCount,
                         FORCE_EMBEDDER_PARALLEL_GRAIN, computeForceRange,
                         state);

  gvizThreadPoolForRange(state->pool, 0, state->vertexCount,
                         FORCE_EMBEDDER_PARALLEL_GRAIN, updateHeatRange,
                         state);

  removeNetTranslation(state);

  double maxDisp = 0.0;
  double sumHeat = 0.0, sumAtt = 0.0, sumRep = 0.0;
  for (size_t i = 0; i < state->vertexCount; i++) {
    double *f = state->disp + i * 2;
    double applied = gvizVecNorm2(2, f);
    if (applied > maxDisp)
      maxDisp = applied;
    sumHeat += state->heat[i];
    sumAtt += state->attForceMag[i];
    sumRep += state->repForceMag[i];
    gvizEmbeddedGraphAddVPosition(embedding, state->vertices[i], f);
  }
  double meanHeat = state->vertexCount ? sumHeat / state->vertexCount : 0.0;
  double meanAtt = state->vertexCount ? sumAtt / state->vertexCount : 0.0;
  double meanRep = state->vertexCount ? sumRep / state->vertexCount : 0.0;
  double meanGrav = state->gravityK * state->meanDegreePlusOne;

  state->iteration++;
  state->lastMaxDisplacement = maxDisp;
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.maxDisp", maxDisp);
  gvizEmbeddedGraphStatAppend(embedding, "forceEmbedder.heat", meanHeat);
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
