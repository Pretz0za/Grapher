#include "embedders/gvizForceDirected.h"
#include "core/gvizVec.h"

static const double FR_SCALE_FACTOR = 0.05;

void gvizPairwiseKKForce(int n, double *vPos, double *uPos, size_t gDist,
                         double edgeLength, double *acc) {
  gvizVecAccKKForce((size_t)n, vPos, uPos, (double)gDist * edgeLength, acc);
}

void gvizPairwiseGRIPFRRepForce(int n, double *vPos, double *uPos,
                                double edgeLength, double *acc) {
  gvizVecAccGRIPFRRepForce((size_t)n, vPos, uPos, edgeLength, acc);
}

void gvizPairwiseGRIPFRAttForce(int n, double *vPos, double *uPos,
                                double edgeLength, double *acc) {
  gvizVecAccGRIPFRAttForce((size_t)n, vPos, uPos, edgeLength, FR_SCALE_FACTOR,
                           acc);
}

void gvizPairwiseFRRepForce(int n, double *vPos, double *uPos, double k,
                            double radiusSum, double overlapConstant,
                            double *acc) {
  gvizVecAccFRRepForce((size_t)n, vPos, uPos, k, radiusSum, overlapConstant,
                       acc);
}

void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos, double k,
                            double *acc) {
  gvizVecAccFRAttForce((size_t)n, vPos, uPos, k, acc);
}

void gvizPairwiseFRRepForceWeighted(int n, double *vPos, double *comPos,
                                    size_t mass, double k, double radiusSum,
                                    double overlapConstant, double *acc) {
  double scratch[n];
  gvizVecZero((size_t)n, scratch);
  gvizPairwiseFRRepForce(n, vPos, comPos, k, radiusSum, overlapConstant,
                         scratch);
  gvizVecAxpy((size_t)n, (double)mass, scratch, acc);
}

void gvizPairwiseLinLogAttForce(int n, double *vPos, double *uPos,
                                double *acc) {
  gvizVecAccLinLogAttForce((size_t)n, vPos, uPos, acc);
}

void gvizPairwiseLinLogRepForce(int n, double *vPos, double *uPos,
                                double vMass, double otherMass,
                                double radiusSum, double overlapConstant,
                                double *acc) {
  gvizVecAccLinLogRepForce((size_t)n, vPos, uPos, vMass, otherMass, radiusSum,
                           overlapConstant, acc);
}

void gvizPairwiseGravityForce(int n, double *vPos, double magnitude,
                              double *acc) {
  gvizVecAccGravityForce((size_t)n, vPos, magnitude, acc);
}
