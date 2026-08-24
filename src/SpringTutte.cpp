#include "SpringTutte.hpp"

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
    throw DimensionError("gviz::layout::SpringTutte: dimension must be 2");
  return dimension;
}

} // namespace

template <GraphLike G>
void SpringTutte<G>::ActionStep(EmbeddedGraph &embedding, void *userData,
                                 const ActionPayload &payload) {
  (void)userData;
  auto &st = static_cast<SpringTutte<G> &>(embedding);
  if (!st.begun_ || st.boundary_.empty())
    return;
  double dt = payload.deltaTime > 0.0 ? payload.deltaTime : 1.0 / 60.0;
  st.Step(dt);
}

template <GraphLike G>
SpringTutte<G>::SpringTutte(G structure, size_t dimension, double epsilon)
    : EmbeddedGraph(GraphLikeVertexCount(structure), RequireDim2(dimension)),
      structure_(std::move(structure)), index_(structure_),
      isBoundary_(index_.Size()), scratch_(index_.Size() * dimension, 0.0),
      velocity_(index_.Size() * dimension, 0.0), epsilon_(epsilon) {
  AddAction("springTutte.step", &SpringTutte<G>::ActionStep);
  AddStatSeries("springTutte.maxDelta", StatChartKind::LineLog);
}

template <GraphLike G>
bool SpringTutte<G>::SetBoundary(std::span<const size_t> boundary,
                                  std::span<const double> polygonPositions) {
  if (boundary.size() < 3)
    return false;

  for (size_t idx : boundary)
    if (!structure_.HasVertex(idx))
      return false;

  isBoundary_.ClearAll();
  boundary_.assign(boundary.begin(), boundary.end());

  size_t d = Dim();
  double zero[2] = {0.0, 0.0};
  for (size_t i = 0; i < boundary_.size(); i++) {
    size_t uLocal = index_.ToLocal(boundary_[i]);
    isBoundary_.Set(uLocal);
    SetVPosition(boundary_[i], polygonPositions.data() + i * d);
    VecCopy(d, zero, velocity_.data() + uLocal * d);
  }

  begun_ = true;
  return true;
}

template <GraphLike G>
void SpringTutte<G>::SeedInterior() {
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

  VecZero(velocity_.size(), velocity_.data());

  iteration_ = 0;
  lastMaxDelta_ = 0.0;
  converged_ = false;
}

template <GraphLike G>
bool SpringTutte<G>::FixConvexPolygon(std::span<const size_t> boundary, double radius) {
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
void SpringTutte<G>::SnapshotInterior() {
  size_t n = index_.Size();
  size_t d = Dim();
  for (size_t uLocal = 0; uLocal < n; uLocal++)
    if (!isBoundary_.Test(uLocal))
      VecCopy(d, EmbeddedGraph::GetVPosition(uLocal), scratch_.data() + uLocal * d);
}

template <GraphLike G>
const double *SpringTutte<G>::NeighborReadPos(size_t vLocal) const noexcept {
  if (!isBoundary_.Test(vLocal))
    return scratch_.data() + vLocal * Dim();
  return EmbeddedGraph::GetVPosition(vLocal);
}

template <GraphLike G>
void SpringTutte<G>::ComputeBarycenter(size_t uLocal, double *out) const {
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
double SpringTutte<G>::SpringVertex(size_t uLocal, double dt) {
  if (structure_.Degree(index_.ToRaw(uLocal)) == 0)
    return 0.0;

  double bary[2];
  ComputeBarycenter(uLocal, bary);

  size_t d = Dim();
  double *old = EmbeddedGraph::GetVPosition(uLocal);
  double *v = velocity_.data() + uLocal * d;
  double newp[2];
  double delta = 0.0;
  for (size_t k = 0; k < d; k++) {
    double accel = stiffness_ * (bary[k] - old[k]) - damping_ * v[k];
    v[k] += accel * dt;
    newp[k] = old[k] + v[k] * dt;
    double diff = newp[k] - old[k];
    delta += diff * diff;
  }

  EmbeddedGraph::SetVPosition(uLocal, newp);
  return std::sqrt(delta);
}

template <GraphLike G>
double SpringTutte<G>::Step(double dt) {
  size_t n = index_.Size();
  if (dt <= 0.0)
    return 0.0;

  SnapshotInterior();

  double maxDelta = 0.0;
  for (size_t uLocal = 0; uLocal < n; uLocal++) {
    if (isBoundary_.Test(uLocal))
      continue;
    double d = SpringVertex(uLocal, dt);
    if (d > maxDelta)
      maxDelta = d;
  }

  iteration_++;
  lastMaxDelta_ = maxDelta;
  if (maxDelta < epsilon_)
    converged_ = true;

  StatAppend("springTutte.maxDelta", maxDelta);

  return maxDelta;
}

template <GraphLike G>
size_t SpringTutte<G>::Run(size_t maxIters, double dt) {
  if (boundary_.empty())
    throw std::logic_error(
        "gviz::layout::SpringTutte::Run: no boundary set (call SetBoundary() "
        "or FixConvexPolygon() first)");

  while (!converged_ && iteration_ < maxIters)
    Step(dt);

  return iteration_;
}

template <GraphLike G>
void SpringTutte<G>::Configure(double stiffness, double damping) noexcept {
  if (stiffness > 0.0)
    stiffness_ = stiffness;
  if (damping > 0.0)
    damping_ = damping;
}

// Explicit instantiation for the GraphLike types this codebase uses.
template class SpringTutte<Graph>;
template class SpringTutte<Subgraph>;

} // namespace gviz::layout
