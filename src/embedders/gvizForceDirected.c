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
                            double *acc) {
  gvizVecAccFRRepForce((size_t)n, vPos, uPos, k, acc);
}

void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos, double k,
                            double *acc) {
  gvizVecAccFRAttForce((size_t)n, vPos, uPos, k, acc);
}
