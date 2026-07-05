#ifndef GVIZ_VEC_H
#define GVIZ_VEC_H

#include <math.h>
#include <stddef.h>
#include <string.h>

/** Largest dimension with an explicit unrolled fast path in this header. */
#define GVIZ_VEC_UNROLLED_MAX 4

static inline void gvizVecZero(size_t n, double *v) {
  switch (n) {
  case 2:
    v[0] = 0.0;
    v[1] = 0.0;
    return;
  case 3:
    v[0] = 0.0;
    v[1] = 0.0;
    v[2] = 0.0;
    return;
  case 4:
    v[0] = 0.0;
    v[1] = 0.0;
    v[2] = 0.0;
    v[3] = 0.0;
    return;
  default:
    memset(v, 0, n * sizeof(double));
  }
}

static inline void gvizVecCopy(size_t n, const double *src, double *dst) {
  switch (n) {
  case 2:
    dst[0] = src[0];
    dst[1] = src[1];
    return;
  case 3:
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    return;
  case 4:
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    return;
  default:
    for (size_t i = 0; i < n; i++)
      dst[i] = src[i];
  }
}

static inline double gvizVecDot(size_t n, const double *a, const double *b) {
  switch (n) {
  case 2:
    return a[0] * b[0] + a[1] * b[1];
  case 3:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  case 4:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  default: {
    double sum = 0.0;
    for (size_t i = 0; i < n; i++)
      sum += a[i] * b[i];
    return sum;
  }
  }
}

static inline double gvizVecNorm2Sq(size_t n, const double *v) {
  return gvizVecDot(n, v, v);
}

static inline double gvizVecNorm2(size_t n, const double *v) {
  return sqrt(gvizVecNorm2Sq(n, v));
}

static inline void gvizVecScale(size_t n, double s, double *v) {
  switch (n) {
  case 2:
    v[0] *= s;
    v[1] *= s;
    return;
  case 3:
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
    return;
  case 4:
    v[0] *= s;
    v[1] *= s;
    v[2] *= s;
    v[3] *= s;
    return;
  default:
    for (size_t i = 0; i < n; i++)
      v[i] *= s;
  }
}

static inline void gvizVecAxpy(size_t n, double alpha, const double *x,
                               double *y) {
  switch (n) {
  case 2:
    y[0] += alpha * x[0];
    y[1] += alpha * x[1];
    return;
  case 3:
    y[0] += alpha * x[0];
    y[1] += alpha * x[1];
    y[2] += alpha * x[2];
    return;
  case 4:
    y[0] += alpha * x[0];
    y[1] += alpha * x[1];
    y[2] += alpha * x[2];
    y[3] += alpha * x[3];
    return;
  default:
    for (size_t i = 0; i < n; i++)
      y[i] += alpha * x[i];
  }
}

static inline void gvizVecAccScaledDiff(size_t n, double alpha, const double *a,
                                        const double *b, double *acc) {
  switch (n) {
  case 2:
    acc[0] += alpha * (a[0] - b[0]);
    acc[1] += alpha * (a[1] - b[1]);
    return;
  case 3:
    acc[0] += alpha * (a[0] - b[0]);
    acc[1] += alpha * (a[1] - b[1]);
    acc[2] += alpha * (a[2] - b[2]);
    return;
  case 4:
    acc[0] += alpha * (a[0] - b[0]);
    acc[1] += alpha * (a[1] - b[1]);
    acc[2] += alpha * (a[2] - b[2]);
    acc[3] += alpha * (a[3] - b[3]);
    return;
  default:
    for (size_t i = 0; i < n; i++)
      acc[i] += alpha * (a[i] - b[i]);
  }
}

static inline void gvizVecAccKKForce(size_t n, const double *vPos,
                                     const double *uPos, double L,
                                     double *acc) {
  if (L < 1e-9)
    return;

  switch (n) {
  case 2: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double s = sqrt(dx * dx + dy * dy) / L - 1.0;
    acc[0] += s * dx;
    acc[1] += s * dy;
    return;
  }
  case 3: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double s = sqrt(dx * dx + dy * dy + dz * dz) / L - 1.0;
    acc[0] += s * dx;
    acc[1] += s * dy;
    acc[2] += s * dz;
    return;
  }
  case 4: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dw = uPos[3] - vPos[3];
    double s = sqrt(dx * dx + dy * dy + dz * dz + dw * dw) / L - 1.0;
    acc[0] += s * dx;
    acc[1] += s * dy;
    acc[2] += s * dz;
    acc[3] += s * dw;
    return;
  }
  default: {
    double dist_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
      double d = uPos[i] - vPos[i];
      dist_sq += d * d;
    }
    double s = sqrt(dist_sq) / L - 1.0;
    for (size_t i = 0; i < n; i++)
      acc[i] += s * (uPos[i] - vPos[i]);
  }
  }
}

static inline void gvizVecAccFRRepForce(size_t n, const double *vPos,
                                        const double *uPos, double edgeLength,
                                        double *acc) {
  double edgeLenSq = edgeLength * edgeLength;

  switch (n) {
  case 2: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dist_sq = dx * dx + dy * dy;
    if (dist_sq < 1e-18)
      return;
    double s = dist_sq / edgeLenSq;
    acc[0] += s * dx;
    acc[1] += s * dy;
    return;
  }
  case 3: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dist_sq = dx * dx + dy * dy + dz * dz;
    if (dist_sq < 1e-18)
      return;
    double s = dist_sq / edgeLenSq;
    acc[0] += s * dx;
    acc[1] += s * dy;
    acc[2] += s * dz;
    return;
  }
  case 4: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dw = uPos[3] - vPos[3];
    double dist_sq = dx * dx + dy * dy + dz * dz + dw * dw;
    if (dist_sq < 1e-18)
      return;
    double s = dist_sq / edgeLenSq;
    acc[0] += s * dx;
    acc[1] += s * dy;
    acc[2] += s * dz;
    acc[3] += s * dw;
    return;
  }
  default: {
    double dist_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
      double d = uPos[i] - vPos[i];
      dist_sq += d * d;
    }
    if (dist_sq < 1e-18)
      return;
    double s = dist_sq / edgeLenSq;
    for (size_t i = 0; i < n; i++)
      acc[i] += s * (uPos[i] - vPos[i]);
  }
  }
}

static inline void gvizVecAccFRAttForce(size_t n, const double *vPos,
                                        const double *uPos, double edgeLength,
                                        double frScale, double *acc) {
  double edgeLenSq = edgeLength * edgeLength;

  switch (n) {
  case 2: {
    double dx = vPos[0] - uPos[0];
    double dy = vPos[1] - uPos[1];
    double dist_sq = dx * dx + dy * dy;
    if (dist_sq < 1e-18)
      return;
    double s = frScale * edgeLenSq / dist_sq;
    acc[0] += s * dx;
    acc[1] += s * dy;
    return;
  }
  case 3: {
    double dx = vPos[0] - uPos[0];
    double dy = vPos[1] - uPos[1];
    double dz = vPos[2] - uPos[2];
    double dist_sq = dx * dx + dy * dy + dz * dz;
    if (dist_sq < 1e-18)
      return;
    double s = frScale * edgeLenSq / dist_sq;
    acc[0] += s * dx;
    acc[1] += s * dy;
    acc[2] += s * dz;
    return;
  }
  case 4: {
    double dx = vPos[0] - uPos[0];
    double dy = vPos[1] - uPos[1];
    double dz = vPos[2] - uPos[2];
    double dw = vPos[3] - uPos[3];
    double dist_sq = dx * dx + dy * dy + dz * dz + dw * dw;
    if (dist_sq < 1e-18)
      return;
    double s = frScale * edgeLenSq / dist_sq;
    acc[0] += s * dx;
    acc[1] += s * dy;
    acc[2] += s * dz;
    acc[3] += s * dw;
    return;
  }
  default: {
    double dist_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
      double d = vPos[i] - uPos[i];
      dist_sq += d * d;
    }
    if (dist_sq < 1e-18)
      return;
    double s = frScale * edgeLenSq / dist_sq;
    for (size_t i = 0; i < n; i++)
      acc[i] += s * (vPos[i] - uPos[i]);
  }
  }
}

#endif
