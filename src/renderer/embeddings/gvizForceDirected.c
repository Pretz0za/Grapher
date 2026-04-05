#include "renderer/embeddings/gvizForceDirected.h"
#include "cblas.h"
#include <math.h>

static const double FR_SCALE_FACTOR = 0.05;

// Kamada Kawai Force Vector calculation
void gvizPairwiseKKForce(int n, double *vPos, double *uPos, size_t gDist,
                         double edgeLength, double *acc) {
  double tmp[n];

  // tmp = uPos
  cblas_dcopy(n, uPos, 1, tmp, 1);

  // tmp = uPos - vPos
  cblas_daxpy(n, -1, vPos, 1, tmp, 1);

  // dist = ||uPos - vPos|| = distance in Rn!
  double dist = cblas_dnrm2(n, tmp, 1);

  double L = gDist * edgeLength; //* edgeLength;
  if (L < 1e-9)
    return; // guard

  double scalar = dist / L - 1.0;

  // tmp = Kamada-Kawai Force vector
  cblas_dscal(n, scalar, tmp, 1);

  // acc = acc + tmp
  cblas_daxpy(n, 1, tmp, 1, acc, 1);
}

// Fruchterman Reingold Attraction Force Vector calculation. Use on neighbors.
void gvizPairwiseFRRepForce(int n, double *vPos, double *uPos,
                            double edgeLength, double *acc) {
  double tmp[n];

  // tmp = uPos
  cblas_dcopy(n, uPos, 1, tmp, 1);

  // tmp = uPos - vPos
  cblas_daxpy(n, -1, vPos, 1, tmp, 1);

  double dist = cblas_dnrm2(n, tmp, 1);
  if (dist < 1e-9)
    return;

  double scalar = (dist * dist) / (edgeLength * edgeLength);

  // tmp = Final force vector
  cblas_dscal(n, scalar, tmp, 1);

  // accumulate
  cblas_daxpy(n, 1, tmp, 1, acc, 1);
}

// Fruchterman Reingold Repulsive Force Vector calculation. Use on KNN
void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos,
                            double edgeLength, double *acc) {
  double tmp[n];

  // tmp = vPos
  cblas_dcopy(n, vPos, 1, tmp, 1);

  // tmp = vPos - uPos
  cblas_daxpy(n, -1, uPos, 1, tmp, 1);

  double dist = cblas_dnrm2(n, tmp, 1);
  if (dist < 1e-9)
    return;

  double scalar = FR_SCALE_FACTOR * (pow(edgeLength, 2) / pow(dist, 2));

  // tmp = final force vector
  cblas_dscal(n, scalar, tmp, 1);

  // accumulate
  cblas_daxpy(n, 1, tmp, 1, acc, 1);
}
