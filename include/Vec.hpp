#ifndef GVIZ_VEC_HPP
#define GVIZ_VEC_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gviz {

/** Largest dimension with an explicit unrolled fast path in this header. */
constexpr size_t VecUnrolledMax = 4;

inline void VecZero(size_t n, double *v) {
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

inline void VecCopy(size_t n, const double *src, double *dst) {
  memcpy(dst, src, sizeof(double) * n);
}

inline double VecDot(size_t n, const double *a, const double *b) {
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

inline double VecNorm2Sq(size_t n, const double *v) { return VecDot(n, v, v); }

inline double VecNorm2(size_t n, const double *v) {
  return sqrt(VecNorm2Sq(n, v));
}

inline void VecScale(size_t n, double s, double *v) {
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

inline void VecAxpy(size_t n, double alpha, const double *x, double *y) {
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

inline void VecAccScaledDiff(size_t n, double alpha, const double *a,
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

inline void VecAccKKForce(size_t n, const double *vPos, const double *uPos,
                           double L, double *acc) {
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

// GRIP's variant of the FR "repulsive" op: NOT the repulsive force from the
// original FR paper (that grows as d^2, i.e. it pulls together like an
// attraction). GRIP applies this along graph edges; see VecAccFRRepForce
// below for the paper-accurate repulsive force (k^2/d, pairwise embedder only).
inline void VecAccGRIPFRRepForce(size_t n, const double *vPos,
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

// GRIP's variant of the FR "attractive" op: NOT the attractive force from the
// original FR paper (that grows as d^2 and pulls together; this one decays as
// 1/d^2, i.e. it pushes apart like a repulsion). GRIP applies this to k-nearest
// non-edge vertices; see VecAccFRAttForce below for the paper-accurate
// attractive force (d/k, pairwise embedder only).
inline void VecAccGRIPFRAttForce(size_t n, const double *vPos,
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

// Attractive force from the original Fruchterman-Reingold paper:
// f_a(d) = d^2 / k, applied along graph edges, pulling v toward u.
inline void VecAccFRAttForce(size_t n, const double *vPos, const double *uPos,
                              double k, double *acc) {
  switch (n) {
  case 2: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dist = sqrt(dx * dx + dy * dy);
    if (dist < 1e-9)
      return;
    double s = dist / k;
    acc[0] += s * dx;
    acc[1] += s * dy;
    return;
  }
  case 3: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 1e-9)
      return;
    double s = dist / k;
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
    double dist = sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
    if (dist < 1e-9)
      return;
    double s = dist / k;
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
    double dist = sqrt(dist_sq);
    if (dist < 1e-9)
      return;
    double s = dist / k;
    for (size_t i = 0; i < n; i++)
      acc[i] += s * (uPos[i] - vPos[i]);
  }
  }
}

/* Deterministic pseudo-random unit direction for two exactly-coincident
 * points, where dx/dy give no direction to normalize. Hashes the two
 * points' buffer addresses (not their -- identical -- values) so distinct
 * coincident pairs still separate in distinct directions instead of all
 * being pushed the same way, which would leave them coincident relative to
 * each other. */
inline void VecCoincidentFallbackDir2D(const double *a, const double *b,
                                        double *outX, double *outY) {
  uint64_t bits = (uint64_t)(uintptr_t)a ^
                  ((uint64_t)(uintptr_t)b * 0x9E3779B97F4A7C15ULL);
  bits ^= bits >> 33;
  bits *= 0xff51afd7ed558ccdULL;
  bits ^= bits >> 33;
  double angle = (double)(bits % 1000000u) / 1000000.0 * 2.0 * M_PI;
  *outX = cos(angle);
  *outY = sin(angle);
}

// Below this fraction of the model's characteristic length (k for FR,
// edgeLength for LinLog), repulsion's 1/gap term is floored instead of left
// to diverge. radiusSum/overlapConstant only bound repulsion once vertices'
// *configured* radii touch (preventOverlap must be on); without it, gap
// tracks raw center distance all the way to 0, so two vertices that land
// arbitrarily close (random initial placement, or two positions briefly
// crossing mid-step) would otherwise produce an unbounded one-round force --
// the vertex is flung far away, and since attraction (FR: ~d^2, LinLog:
// ~log(d)) grows much slower than the repulsion that just fired, it can take
// many rounds to pull back in, especially for LinLog's logarithmic
// attraction. This floor is a numerical-stability guard, independent of
// preventOverlap.
constexpr double VecMinDistFraction = 0.01;

// Repulsive force from the original Fruchterman-Reingold paper:
// f_r(d) = k^2 / d, applied between every vertex pair, pushing v away from u.
// radiusSum (0 to disable) shifts the effective distance from raw center
// distance to the surface-to-surface gap between the two vertices' rendered
// radii. Once that gap is <= 0 (the circles touch or overlap), the magnitude
// stops growing and holds flat at kSq * overlapConstant (Gephi ForceAtlas2's
// "Prevent Overlap" constant) instead of diverging -- bounded by
// construction, since that branch never divides by the vanishing gap.
inline void VecAccFRRepForce(size_t n, const double *vPos, const double *uPos,
                              double k, double radiusSum,
                              double overlapConstant, double *acc) {
  double kSq = k * k;

  switch (n) {
  case 2: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dist_sq = dx * dx + dy * dy;
    double dist = sqrt(dist_sq);
    double safeDist = fmax(dist, k * VecMinDistFraction);
    double gap = safeDist - radiusSum;
    double mag = gap > 0.0 ? kSq / gap : kSq * overlapConstant;
    double ux, uy;
    if (dist > 0.0) {
      ux = dx / dist;
      uy = dy / dist;
    } else {
      VecCoincidentFallbackDir2D(vPos, uPos, &ux, &uy);
    }
    acc[0] -= mag * ux;
    acc[1] -= mag * uy;
    return;
  }
  /* n=3/4/default: only n=2 is exercised in practice (the pairwise embedder
   * hard-locks to 2D); a coincidence fallback for higher dimensions can be
   * added if a future caller needs it. For now these keep the old
   * skip-guard: no force when the two points exactly coincide. */
  case 3: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dist_sq = dx * dx + dy * dy + dz * dz;
    double dist = sqrt(dist_sq);
    if (dist <= 0.0)
      return;
    double gap = dist - radiusSum;
    double mag = gap > 0.0 ? kSq / gap : kSq * overlapConstant;
    double s = mag / dist;
    acc[0] -= s * dx;
    acc[1] -= s * dy;
    acc[2] -= s * dz;
    return;
  }
  case 4: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dw = uPos[3] - vPos[3];
    double dist_sq = dx * dx + dy * dy + dz * dz + dw * dw;
    double dist = sqrt(dist_sq);
    if (dist <= 0.0)
      return;
    double gap = dist - radiusSum;
    double mag = gap > 0.0 ? kSq / gap : kSq * overlapConstant;
    double s = mag / dist;
    acc[0] -= s * dx;
    acc[1] -= s * dy;
    acc[2] -= s * dz;
    acc[3] -= s * dw;
    return;
  }
  default: {
    double dist_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
      double d = uPos[i] - vPos[i];
      dist_sq += d * d;
    }
    double dist = sqrt(dist_sq);
    if (dist <= 0.0)
      return;
    double gap = dist - radiusSum;
    double mag = gap > 0.0 ? kSq / gap : kSq * overlapConstant;
    double s = mag / dist;
    for (size_t i = 0; i < n; i++)
      acc[i] -= s * (uPos[i] - vPos[i]);
  }
  }
}

// FR repulsive force from a pseudo-body of the given mass, collapsed at
// comPos -- the Barnes-Hut approximation for a quadtree node far enough away
// (per its opening-angle test) to be treated as one point instead of visiting
// each aggregated body individually. radiusSum here is vPos's own radius only
// (0 to disable): a pseudo-body has no single radius of its own, and the
// opening-angle test already guarantees it's far enough away that overlap
// with any individual body it aggregates is moot.
inline void VecAccFRRepForceWeighted(size_t n, const double *vPos,
                                      const double *comPos, size_t mass,
                                      double k, double radiusSum,
                                      double overlapConstant, double *acc) {
  double stackScratch[VecUnrolledMax] = {0.0, 0.0, 0.0, 0.0};
  std::vector<double> heapScratch;
  double *scratch = stackScratch;
  if (n > VecUnrolledMax) {
    heapScratch.assign(n, 0.0);
    scratch = heapScratch.data();
  }
  VecAccFRRepForce(n, vPos, comPos, k, radiusSum, overlapConstant, scratch);
  VecAxpy(n, static_cast<double>(mass), scratch, acc);
}

// LinLog attractive force: magnitude log(1 + dist), pulling v toward u --
// the textbook Noack LinLog energy model's attractive term. Grows slower
// than FR's linear d/k (keeps the LinLog "weak long-range attraction"
// character that reveals cluster structure), and slower still than
// sqrt(dist) (the equilibrium gap against a mass-scaled repulsive term
// grows as roughly mass/log(mass) here, vs. mass^(2/3) under sqrt(dist)
// growth -- so high-degree hubs stretch their incident edges out further
// than under the sqrt(dist) variant).
inline void VecAccLinLogAttForce(size_t n, const double *vPos,
                                  const double *uPos, double *acc) {
  switch (n) {
  case 2: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dist = sqrt(dx * dx + dy * dy);
    if (dist < 1e-9)
      return;
    double s = log1p(dist) / dist;
    acc[0] += s * dx;
    acc[1] += s * dy;
    return;
  }
  case 3: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dist = sqrt(dx * dx + dy * dy + dz * dz);
    if (dist < 1e-9)
      return;
    double s = log1p(dist) / dist;
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
    double dist = sqrt(dx * dx + dy * dy + dz * dz + dw * dw);
    if (dist < 1e-9)
      return;
    double s = log1p(dist) / dist;
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
    double dist = sqrt(dist_sq);
    if (dist < 1e-9)
      return;
    double s = log1p(dist) / dist;
    for (size_t i = 0; i < n; i++)
      acc[i] += s * (uPos[i] - vPos[i]);
  }
  }
}

// LinLog repulsive force: magnitude (vMass*otherMass)/dist, pushing v away
// from u. radiusSum (0 to disable) shifts the effective distance from raw
// center distance to the surface-to-surface gap between the two vertices'
// rendered radii. Once that gap is <= 0 (the circles touch or overlap), the
// magnitude stops growing and holds flat at massProduct * overlapConstant
// (Gephi ForceAtlas2's "Prevent Overlap" constant) instead of diverging --
// bounded by construction, since that branch never divides by the vanishing
// gap. Independent of radiusSum, the raw center distance itself is floored
// to VecMinDistFraction * edgeLength before computing gap, so two vertices
// landing arbitrarily close together (radiusSum or not) can't spike this
// into an unbounded one-round impulse.
inline void VecAccLinLogRepForce(size_t n, const double *vPos,
                                  const double *uPos, double vMass,
                                  double otherMass, double radiusSum,
                                  double overlapConstant, double edgeLength,
                                  double *acc) {
  double massProduct = vMass * otherMass;

  switch (n) {
  case 2: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dist_sq = dx * dx + dy * dy;
    double dist = sqrt(dist_sq);
    double safeDist = fmax(dist, edgeLength * VecMinDistFraction);
    double gap = safeDist - radiusSum;
    double mag =
        gap > 0.0 ? massProduct / gap : massProduct * overlapConstant;
    double ux, uy;
    if (dist > 0.0) {
      ux = dx / dist;
      uy = dy / dist;
    } else {
      VecCoincidentFallbackDir2D(vPos, uPos, &ux, &uy);
    }
    acc[0] -= mag * ux;
    acc[1] -= mag * uy;
    return;
  }
  /* n=3/4/default: only n=2 is exercised in practice (the pairwise embedder
   * hard-locks to 2D); a coincidence fallback for higher dimensions can be
   * added if a future caller needs it. For now these keep the old
   * skip-guard: no force when the two points exactly coincide. */
  case 3: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dist_sq = dx * dx + dy * dy + dz * dz;
    double dist = sqrt(dist_sq);
    if (dist <= 0.0)
      return;
    double gap = dist - radiusSum;
    double mag =
        gap > 0.0 ? massProduct / gap : massProduct * overlapConstant;
    double s = mag / dist;
    acc[0] -= s * dx;
    acc[1] -= s * dy;
    acc[2] -= s * dz;
    return;
  }
  case 4: {
    double dx = uPos[0] - vPos[0];
    double dy = uPos[1] - vPos[1];
    double dz = uPos[2] - vPos[2];
    double dw = uPos[3] - vPos[3];
    double dist_sq = dx * dx + dy * dy + dz * dz + dw * dw;
    double dist = sqrt(dist_sq);
    if (dist <= 0.0)
      return;
    double gap = dist - radiusSum;
    double mag =
        gap > 0.0 ? massProduct / gap : massProduct * overlapConstant;
    double s = mag / dist;
    acc[0] -= s * dx;
    acc[1] -= s * dy;
    acc[2] -= s * dz;
    acc[3] -= s * dw;
    return;
  }
  default: {
    double dist_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
      double d = uPos[i] - vPos[i];
      dist_sq += d * d;
    }
    double dist = sqrt(dist_sq);
    if (dist <= 0.0)
      return;
    double gap = dist - radiusSum;
    double mag =
        gap > 0.0 ? massProduct / gap : massProduct * overlapConstant;
    double s = mag / dist;
    for (size_t i = 0; i < n; i++)
      acc[i] -= s * (uPos[i] - vPos[i]);
  }
  }
}

// Gravity force: constant magnitude pulling v toward the origin.
inline void VecAccGravityForce(size_t n, const double *vPos, double magnitude,
                                double *acc) {
  switch (n) {
  case 2: {
    double dist = sqrt(vPos[0] * vPos[0] + vPos[1] * vPos[1]);
    if (dist < 1e-9)
      return;
    double s = magnitude / dist;
    acc[0] -= s * vPos[0];
    acc[1] -= s * vPos[1];
    return;
  }
  case 3: {
    double dist =
        sqrt(vPos[0] * vPos[0] + vPos[1] * vPos[1] + vPos[2] * vPos[2]);
    if (dist < 1e-9)
      return;
    double s = magnitude / dist;
    acc[0] -= s * vPos[0];
    acc[1] -= s * vPos[1];
    acc[2] -= s * vPos[2];
    return;
  }
  case 4: {
    double dist = sqrt(vPos[0] * vPos[0] + vPos[1] * vPos[1] +
                        vPos[2] * vPos[2] + vPos[3] * vPos[3]);
    if (dist < 1e-9)
      return;
    double s = magnitude / dist;
    acc[0] -= s * vPos[0];
    acc[1] -= s * vPos[1];
    acc[2] -= s * vPos[2];
    acc[3] -= s * vPos[3];
    return;
  }
  default: {
    double dist_sq = 0.0;
    for (size_t i = 0; i < n; i++)
      dist_sq += vPos[i] * vPos[i];
    double dist = sqrt(dist_sq);
    if (dist < 1e-9)
      return;
    double s = magnitude / dist;
    for (size_t i = 0; i < n; i++)
      acc[i] -= s * vPos[i];
  }
  }
}

} // namespace gviz

#endif
