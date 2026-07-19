#include "embedders/gvizForceModel.h"
#include "embedders/gvizForceDirected.h"

static double frVertexMass(size_t degree) {
  (void)degree;
  return 1.0;
}

static void frAttractive(int n, double *vPos, double *uPos, double edgeLength,
                         double *acc) {
  gvizPairwiseFRAttForce(n, vPos, uPos, edgeLength, acc);
}

static void frRepulsive(int n, double *vPos, double *otherPos, double vMass,
                        double otherMass, double edgeLength, double *acc) {
  (void)vMass;
  gvizPairwiseFRRepForceWeighted(n, vPos, otherPos, (size_t)otherMass,
                                 edgeLength, acc);
}

static const gvizForceModel gvizForceModelFR = {
    .vertexMass = frVertexMass,
    .attractive = frAttractive,
    .repulsive = frRepulsive,
};

static double linLogVertexMass(size_t degree) { return (double)degree; }

static void linLogAttractive(int n, double *vPos, double *uPos,
                             double edgeLength, double *acc) {
  (void)edgeLength;
  gvizPairwiseLinLogAttForce(n, vPos, uPos, acc);
}

static void linLogRepulsive(int n, double *vPos, double *otherPos,
                            double vMass, double otherMass, double edgeLength,
                            double *acc) {
  (void)edgeLength;
  gvizPairwiseLinLogRepForce(n, vPos, otherPos, vMass, otherMass, acc);
}

static const gvizForceModel gvizForceModelLinLog = {
    .vertexMass = linLogVertexMass,
    .attractive = linLogAttractive,
    .repulsive = linLogRepulsive,
};

const gvizForceModel *gvizForceModelGet(gvizForceModelKind kind) {
  switch (kind) {
  case GVIZ_FORCE_MODEL_LINLOG:
    return &gvizForceModelLinLog;
  case GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD:
  default:
    return &gvizForceModelFR;
  }
}
