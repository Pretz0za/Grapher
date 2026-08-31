#include "KamadaKawai.hpp"

#include "BreadthFirst.hpp"
#include "ConnectedComponents.hpp"
#include "Error.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "Vec.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace gviz::layout {

namespace {

size_t RequireMinDimension(size_t dimension) {
  if (dimension < 1)
    throw DimensionError("gviz::layout::KamadaKawai: dimension must be >= 1");
  return dimension;
}

/**
 * Solves the dim x dim linear system A * x = b in place via Gaussian
 * elimination with partial pivoting (dim is always small -- 2 to 4 in
 * practice). @p A is row-major and destroyed; on success @p b is
 * overwritten with the solution x. Returns false (leaving @p b unusable)
 * if a pivot is too close to zero.
 */
bool SolveLinearSystem(size_t dim, double *A, double *b) {
  constexpr double kPivotEpsilon = 1e-12;

  for (size_t col = 0; col < dim; col++) {
    size_t pivotRow = col;
    double pivotMag = std::fabs(A[col * dim + col]);
    for (size_t row = col + 1; row < dim; row++) {
      double mag = std::fabs(A[row * dim + col]);
      if (mag > pivotMag) {
        pivotMag = mag;
        pivotRow = row;
      }
    }
    if (pivotMag < kPivotEpsilon)
      return false;

    if (pivotRow != col) {
      for (size_t k = 0; k < dim; k++)
        std::swap(A[col * dim + k], A[pivotRow * dim + k]);
      std::swap(b[col], b[pivotRow]);
    }

    double pivot = A[col * dim + col];
    for (size_t row = col + 1; row < dim; row++) {
      double factor = A[row * dim + col] / pivot;
      if (factor == 0.0)
        continue;
      for (size_t k = col; k < dim; k++)
        A[row * dim + k] -= factor * A[col * dim + k];
      b[row] -= factor * b[col];
    }
  }

  for (size_t i = dim; i-- > 0;) {
    double sum = b[i];
    for (size_t k = i + 1; k < dim; k++)
      sum -= A[i * dim + k] * b[k];
    b[i] = sum / A[i * dim + i];
  }
  return true;
}

constexpr size_t kMaxNewtonIterationsPerVertex = 100;
constexpr double kCoincidentGuard = 1e-9;

} // namespace

template <GraphLike G>
void KamadaKawai<G>::ActionStep(EmbeddedGraph &embedding, void *userData,
                                 const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &kk = static_cast<KamadaKawai<G> &>(embedding);
  if (!kk.begun_)
    return;
  kk.Step();
}

template <GraphLike G>
KamadaKawai<G>::KamadaKawai(G structure, size_t dimension, double edgeLength,
                             double epsilon)
    : EmbeddedGraph(GraphLikeVertexCount(structure), RequireMinDimension(dimension)),
      structure_(std::move(structure)), index_(structure_), edgeLength_(edgeLength),
      epsilon_(epsilon) {
  AddAction("kamadaKawai.step", &KamadaKawai<G>::ActionStep);
  AddStatSeries("kamadaKawai.maxGradient", StatChartKind::LineLog);
}

template <GraphLike G>
void KamadaKawai<G>::Begin() {
  search::Components components = search::ConnectedComponents(structure_);
  if (components.count > 1)
    throw NotConnectedError();

  n_ = index_.Size();
  distances_.assign(n_ * n_, std::numeric_limits<size_t>::max());

  // `dist` comes back raw/capacity-addressed; translate into the local
  // n_*n_ table below.
  std::vector<size_t> dist;
  for (size_t u : structure_) {
    search::BreadthFirst(structure_, u, dist);
    size_t uLocal = index_.ToLocal(u);
    for (size_t v : structure_)
      distances_[uLocal * n_ + index_.ToLocal(v)] = dist[v];
  }

  size_t count = n_;
  if (Dim() == 2 && count > 0) {
    double radius = edgeLength_ * static_cast<double>(count) / (2.0 * M_PI);
    if (radius < 1.0)
      radius = 1.0;
    size_t idx = 0;
    for (size_t u : structure_) {
      double angle =
          2.0 * M_PI * static_cast<double>(idx) / static_cast<double>(count);
      double pos[2] = {radius * std::cos(angle), radius * std::sin(angle)};
      SetVPosition(u, pos);
      idx++;
    }
  } else if (count > 0) {
    double boxExtent = edgeLength_ * std::sqrt(static_cast<double>(n_));
    RandomizePositions(boxExtent, /*seed=*/1);
  }

  iteration_ = 0;
  lastMaxGradient_ = 0.0;
  converged_ = false;
  begun_ = true;
}

template <GraphLike G>
double KamadaKawai<G>::ComputeGradientAndHessian(size_t m, double *grad,
                                                  double *hessian) const {
  size_t d = Dim();
  VecZero(d, grad);
  std::fill(hessian, hessian + d * d, 0.0);

  const double *pm = GetVPosition(m);
  size_t mLocal = index_.ToLocal(m);
  double diffStack[VecUnrolledMax];
  std::vector<double> diffHeap;
  double *diff = diffStack;
  if (d > VecUnrolledMax) {
    diffHeap.assign(d, 0.0);
    diff = diffHeap.data();
  }

  for (size_t i : structure_) {
    if (i == m)
      continue;
    size_t hop = distances_[mLocal * n_ + index_.ToLocal(i)];
    if (hop == 0 || hop == std::numeric_limits<size_t>::max())
      continue;

    double l = edgeLength_ * static_cast<double>(hop);
    double k = stiffness_ / (static_cast<double>(hop) * static_cast<double>(hop));

    const double *pi = GetVPosition(i);
    for (size_t c = 0; c < d; c++)
      diff[c] = pm[c] - pi[c];
    double dist = VecNorm2(d, diff);
    if (dist < kCoincidentGuard)
      continue;

    double invDist = 1.0 / dist;
    double oneMinusLOverDist = 1.0 - l * invDist;
    VecAxpy(d, k * oneMinusLOverDist, diff, grad);

    double lOverDist3 = l * invDist * invDist * invDist;
    for (size_t r = 0; r < d; r++) {
      for (size_t c = 0; c < d; c++) {
        double delta = (r == c) ? 1.0 : 0.0;
        hessian[r * d + c] +=
            k * (oneMinusLOverDist * delta + lOverDist3 * diff[r] * diff[c]);
      }
    }
  }

  return VecNorm2(d, grad);
}

template <GraphLike G>
double KamadaKawai<G>::Step() {
  size_t d = Dim();
  std::vector<double> grad(d), hessian(d * d);

  size_t best = std::numeric_limits<size_t>::max();
  double bestNorm = -1.0;
  for (size_t u : structure_) {
    double norm = ComputeGradientAndHessian(u, grad.data(), hessian.data());
    if (norm > bestNorm) {
      bestNorm = norm;
      best = u;
    }
  }

  if (best == std::numeric_limits<size_t>::max()) {
    iteration_++;
    lastMaxGradient_ = 0.0;
    converged_ = true;
    StatAppend("kamadaKawai.maxGradient", 0.0);
    return 0.0;
  }

  if (bestNorm < epsilon_) {
    converged_ = true;
  } else {
    converged_ = false;
    std::vector<double> delta(d);
    double norm = bestNorm;
    size_t iters = 0;
    while (norm >= epsilon_ && iters < kMaxNewtonIterationsPerVertex) {
      norm = ComputeGradientAndHessian(best, grad.data(), hessian.data());
      if (norm < epsilon_)
        break;
      for (size_t c = 0; c < d; c++)
        delta[c] = -grad[c];
      if (!SolveLinearSystem(d, hessian.data(), delta.data()))
        break;
      double *pm = GetVPosition(best);
      for (size_t c = 0; c < d; c++)
        pm[c] += delta[c];
      iters++;
    }
  }

  iteration_++;
  lastMaxGradient_ = bestNorm;
  StatAppend("kamadaKawai.maxGradient", bestNorm);
  return bestNorm;
}

template <GraphLike G>
size_t KamadaKawai<G>::Run(size_t maxIters) {
  if (!begun_)
    throw std::logic_error(
        "gviz::layout::KamadaKawai::Run: Begin() has not been called");

  while (!converged_ && iteration_ < maxIters)
    Step();

  return iteration_;
}

template <GraphLike G>
size_t KamadaKawai<G>::GraphDistance(size_t u, size_t v) const noexcept {
  if (!begun_ || !structure_.HasVertex(u) || !structure_.HasVertex(v))
    return std::numeric_limits<size_t>::max();
  return distances_[index_.ToLocal(u) * n_ + index_.ToLocal(v)];
}

// Explicit instantiation for the GraphLike types this codebase uses.
template class KamadaKawai<Graph>;
template class KamadaKawai<Subgraph>;

} // namespace gviz::layout
