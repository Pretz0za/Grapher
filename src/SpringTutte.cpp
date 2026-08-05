#include "SpringTutte.hpp"

#include "Error.hpp"
#include "Planar.hpp"
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

void SpringTutte::ActionStep(EmbeddedGraph &embedding, void *userData,
                              const ActionPayload &payload) {
  (void)userData;
  auto &st = static_cast<SpringTutte &>(embedding);
  if (!st.begun_ || st.boundary_.empty())
    return;
  double dt = payload.deltaTime > 0.0 ? payload.deltaTime : 1.0 / 60.0;
  st.Step(dt);
}

void SpringTutte::ActionFixOuterFace(EmbeddedGraph &embedding, void *userData,
                                      const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &st = static_cast<SpringTutte &>(embedding);
  st.FixOuterFace();
}

SpringTutte::SpringTutte(Graph &g, Subgraph subgraph, size_t dimension, double epsilon)
    : EmbeddedGraph(std::move(subgraph), RequireDim2(dimension)), graph_(g),
      isBoundary_(g.Size()), scratch_(g.Size() * dimension, 0.0),
      velocity_(g.Size() * dimension, 0.0), epsilon_(epsilon) {
  AddAction("springTutte.step", &SpringTutte::ActionStep);
  AddAction("springTutte.fixOuterFace", &SpringTutte::ActionFixOuterFace);
  AddStatSeries("springTutte.maxDelta", StatChartKind::LineLog);
}

void SpringTutte::Begin() {
  ApplyPlanarRotation(graph_, Structure());
  SetPlanarEmbedded(true);

  std::vector<size_t> boundary = LargestFaceBoundary(graph_, Structure());
  FixConvexPolygon(boundary, 200.0);
  SeedInterior();
  begun_ = true;
}

bool SpringTutte::SetBoundary(std::span<const size_t> boundary,
                               std::span<const double> polygonPositions) {
  if (boundary.size() < 3)
    return false;

  size_t N = graph_.Size();
  for (size_t idx : boundary)
    if (idx >= N)
      return false;

  isBoundary_.ClearAll();
  boundary_.assign(boundary.begin(), boundary.end());

  size_t d = Dim();
  double zero[2] = {0.0, 0.0};
  for (size_t i = 0; i < boundary_.size(); i++) {
    size_t u = boundary_[i];
    isBoundary_.Set(u);
    SetVPosition(u, polygonPositions.data() + i * d);
    VecCopy(d, zero, velocity_.data() + u * d);
  }

  return true;
}

void SpringTutte::SeedInterior() {
  size_t N = graph_.Size();
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

  for (size_t u = 0; u < N; u++)
    if (!isBoundary_.Test(u))
      SetVPosition(u, centroid);

  VecZero(velocity_.size(), velocity_.data());

  iteration_ = 0;
  lastMaxDelta_ = 0.0;
  converged_ = false;
}

bool SpringTutte::FixConvexPolygon(std::span<const size_t> boundary, double radius) {
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

void SpringTutte::SnapshotInterior() {
  size_t N = graph_.Size();
  size_t d = Dim();
  for (size_t u = 0; u < N; u++)
    if (!isBoundary_.Test(u))
      VecCopy(d, GetVPosition(u), scratch_.data() + u * d);
}

const double *SpringTutte::NeighborReadPos(size_t v) const noexcept {
  if (!isBoundary_.Test(v))
    return scratch_.data() + v * Dim();
  return GetVPosition(v);
}

void SpringTutte::ComputeBarycenter(size_t u, double *out) const {
  size_t d = Dim();
  size_t count = 0;
  VecZero(d, out);

  for (size_t v : Structure().Neighbors(u)) {
    const double *vp = NeighborReadPos(v);
    for (size_t k = 0; k < d; k++)
      out[k] += vp[k];
    count++;
  }
  if (count == 0)
    return;
  for (size_t k = 0; k < d; k++)
    out[k] /= static_cast<double>(count);
}

double SpringTutte::SpringVertex(size_t u, double dt) {
  if (Structure().Degree(u) == 0)
    return 0.0;

  double bary[2];
  ComputeBarycenter(u, bary);

  size_t d = Dim();
  double *old = GetVPosition(u);
  double *v = velocity_.data() + u * d;
  double newp[2];
  double delta = 0.0;
  for (size_t k = 0; k < d; k++) {
    double accel = stiffness_ * (bary[k] - old[k]) - damping_ * v[k];
    v[k] += accel * dt;
    newp[k] = old[k] + v[k] * dt;
    double diff = newp[k] - old[k];
    delta += diff * diff;
  }

  SetVPosition(u, newp);
  return std::sqrt(delta);
}

double SpringTutte::Step(double dt) {
  size_t N = graph_.Size();
  if (dt <= 0.0)
    return 0.0;

  SnapshotInterior();

  double maxDelta = 0.0;
  for (size_t u = 0; u < N; u++) {
    if (isBoundary_.Test(u))
      continue;
    double d = SpringVertex(u, dt);
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

size_t SpringTutte::Run(size_t maxIters, double dt) {
  if (boundary_.empty())
    throw std::logic_error(
        "gviz::layout::SpringTutte::Run: no boundary set (call Begin(), "
        "SetBoundary(), or FixConvexPolygon() first)");

  while (!converged_ && iteration_ < maxIters)
    Step(dt);

  return iteration_;
}

bool SpringTutte::FixOuterFace() {
  if (!IsPlanarEmbedded() || !HasHighlight())
    return false;

  const Subgraph &highlight = *GetHighlight();

  HalfEdge start{};
  bool found = false;
  for (size_t u : highlight) {
    for (size_t v : highlight.Neighbors(u)) {
      if (!highlight.HasEdge(u, v))
        continue;
      start = HalfEdge{u, v};
      found = true;
      break;
    }
    if (found)
      break;
  }
  if (!found)
    return false;

  std::vector<size_t> boundary;
  for (size_t v : FaceWalk(graph_, highlight, start))
    boundary.push_back(v);

  if (boundary.size() < 3)
    return false;

  if (!FixConvexPolygon(boundary, 200.0))
    return false;

  iteration_ = 0;
  lastMaxDelta_ = 0.0;
  converged_ = false;
  begun_ = true;
  return true;
}

void SpringTutte::Configure(double stiffness, double damping) noexcept {
  if (stiffness > 0.0)
    stiffness_ = stiffness;
  if (damping > 0.0)
    damping_ = damping;
}

} // namespace gviz::layout
