#include "Tutte.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "Vec.hpp"

#include <cmath>
#include <stdexcept>

namespace gviz::layout {

namespace {

size_t RequireDim2(size_t dimension) {
  if (dimension != 2)
    throw DimensionError("gviz::layout::Tutte: dimension must be 2");
  return dimension;
}

} // namespace

template <GraphLike G>
void Tutte<G>::ActionStep(EmbeddedGraph &embedding, void *userData,
                           const ActionPayload &payload) {
  (void)userData;
  auto &t = static_cast<Tutte<G> &>(embedding);
  if (!t.begun_ || t.boundary_.empty())
    return;
  double dt = payload.deltaTime > 0.0 ? payload.deltaTime : 1.0 / 60.0;
  t.Step(dt);
}

template <GraphLike G>
Tutte<G>::Tutte(G structure, size_t dimension, double epsilon)
    : EmbeddedGraph(GraphLikeVertexCount(structure), RequireDim2(dimension)),
      structure_(std::move(structure)), index_(structure_),
      isBoundary_(index_.Size()), scratch_(index_.Size() * dimension, 0.0),
      epsilon_(epsilon) {
  AddAction("tutte.step", &Tutte<G>::ActionStep);
  AddStatSeries("tutte.maxDelta", StatChartKind::LineLog);
}

template <GraphLike G>
bool Tutte<G>::SetBoundary(std::span<const size_t> boundary,
                            std::span<const double> polygonPositions) {
  if (boundary.size() < 3)
    return false;

  for (size_t idx : boundary)
    if (!structure_.HasVertex(idx))
      return false;

  isBoundary_.ClearAll();
  boundary_.assign(boundary.begin(), boundary.end());

  size_t d = Dim();
  for (size_t i = 0; i < boundary_.size(); i++) {
    isBoundary_.Set(index_.ToLocal(boundary_[i]));
    SetVPosition(boundary_[i], polygonPositions.data() + i * d);
  }

  begun_ = true;
  return true;
}

template <GraphLike G>
void Tutte<G>::SeedInterior() {
  size_t n = index_.Size();
  size_t d = Dim();

  double centroid[2] = {0.0, 0.0};
  for (size_t b : boundary_) {
    const double *p = GetVPosition(b);
    for (size_t k = 0; k < d; k++)
      centroid[k] += p[k];
  }
  if (!boundary_.empty())
    for (size_t k = 0; k < d; k++)
      centroid[k] /= static_cast<double>(boundary_.size());

  for (size_t uLocal = 0; uLocal < n; uLocal++)
    if (!isBoundary_.Test(uLocal))
      EmbeddedGraph::SetVPosition(uLocal, centroid);

  iteration_ = 0;
  lastMaxDelta_ = 0.0;
  converged_ = false;
}

template <GraphLike G>
bool Tutte<G>::FixConvexPolygon(std::span<const size_t> boundary, double radius) {
  size_t d = Dim();
  size_t count = boundary.size();
  std::vector<double> positions(count * d, 0.0);

  for (size_t k = 0; k < count; k++) {
    double angle = 2.0 * M_PI * static_cast<double>(k) / static_cast<double>(count);
    positions[k * d + 0] = radius * std::cos(angle);
    if (d > 1)
      positions[k * d + 1] = radius * std::sin(angle);
  }

  return SetBoundary(boundary, positions);
}

template <GraphLike G>
void Tutte<G>::SnapshotInterior() {
  size_t n = index_.Size();
  size_t d = Dim();
  for (size_t uLocal = 0; uLocal < n; uLocal++)
    if (!isBoundary_.Test(uLocal))
      VecCopy(d, EmbeddedGraph::GetVPosition(uLocal), scratch_.data() + uLocal * d);
}

template <GraphLike G>
const double *Tutte<G>::NeighborReadPos(size_t vLocal) const noexcept {
  if (!useGaussSeidel_ && !isBoundary_.Test(vLocal))
    return scratch_.data() + vLocal * Dim();
  return EmbeddedGraph::GetVPosition(vLocal);
}

template <GraphLike G>
void Tutte<G>::ComputeBarycenter(size_t uLocal, double *out) const {
  size_t d = Dim();
  size_t count = 0;
  VecZero(d, out);

  for (size_t v : structure_.Neighbors(index_.ToRaw(uLocal))) {
    const double *vp = NeighborReadPos(index_.ToLocal(v));
    for (size_t k = 0; k < d; k++)
      out[k] += vp[k];
    count++;
  }
  if (count == 0)
    return;
  for (size_t k = 0; k < d; k++)
    out[k] /= static_cast<double>(count);
}

template <GraphLike G>
double Tutte<G>::RelaxVertex(size_t uLocal, double alpha) {
  if (structure_.Degree(index_.ToRaw(uLocal)) == 0)
    return 0.0;

  double bary[2];
  ComputeBarycenter(uLocal, bary);

  size_t d = Dim();
  double *old = EmbeddedGraph::GetVPosition(uLocal);
  double newp[2];
  double delta = 0.0;
  for (size_t k = 0; k < d; k++) {
    newp[k] = old[k] + alpha * (bary[k] - old[k]);
    double diff = newp[k] - old[k];
    delta += diff * diff;
  }

  EmbeddedGraph::SetVPosition(uLocal, newp);
  return std::sqrt(delta);
}

template <GraphLike G>
double Tutte<G>::Step(double dt) {
  size_t n = index_.Size();

  double alpha = relaxationRate_ * dt;
  if (alpha <= 0.0)
    return 0.0;
  if (alpha > 1.0)
    alpha = 1.0;

  if (!useGaussSeidel_)
    SnapshotInterior();

  double maxDelta = 0.0;
  for (size_t uLocal = 0; uLocal < n; uLocal++) {
    if (isBoundary_.Test(uLocal))
      continue;
    double d = RelaxVertex(uLocal, alpha);
    if (d > maxDelta)
      maxDelta = d;
  }

  iteration_++;
  lastMaxDelta_ = maxDelta;
  if (maxDelta < epsilon_)
    converged_ = true;

  StatAppend("tutte.maxDelta", maxDelta);

  return maxDelta;
}

template <GraphLike G>
size_t Tutte<G>::Run(size_t maxIters) {
  if (boundary_.empty())
    throw std::logic_error(
        "gviz::layout::Tutte::Run: no boundary set (call SetBoundary() or "
        "FixConvexPolygon() first)");

  while (!converged_ && iteration_ < maxIters)
    Step(1.0);

  return iteration_;
}

// Explicit instantiation for the GraphLike types this codebase uses.
template class Tutte<Graph>;
template class Tutte<Subgraph>;

} // namespace gviz::layout
