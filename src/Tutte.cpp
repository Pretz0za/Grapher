#include "Tutte.hpp"

#include "Error.hpp"
#include "Planar.hpp"
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

void Tutte::ActionStep(EmbeddedGraph &embedding, void *userData,
                        const ActionPayload &payload) {
  (void)userData;
  auto &t = static_cast<Tutte &>(embedding);
  if (!t.begun_ || t.boundary_.empty())
    return;
  double dt = payload.deltaTime > 0.0 ? payload.deltaTime : 1.0 / 60.0;
  t.Step(dt);
}

void Tutte::ActionFixOuterFace(EmbeddedGraph &embedding, void *userData,
                                const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &t = static_cast<Tutte &>(embedding);
  t.FixOuterFace();
}

Tutte::Tutte(Graph &g, Subgraph subgraph, size_t dimension, double epsilon)
    : EmbeddedGraph(std::move(subgraph), RequireDim2(dimension)), graph_(g),
      isBoundary_(g.Size()), scratch_(g.Size() * dimension, 0.0),
      epsilon_(epsilon) {
  AddAction("tutte.step", &Tutte::ActionStep);
  AddAction("tutte.fixOuterFace", &Tutte::ActionFixOuterFace);
  AddStatSeries("tutte.maxDelta", StatChartKind::LineLog);
}

void Tutte::Begin() {
  ApplyPlanarRotation(graph_, Structure());
  SetPlanarEmbedded(true);

  std::vector<size_t> boundary = LargestFaceBoundary(graph_, Structure());
  FixConvexPolygon(boundary, 200.0);
  SeedInterior();
  begun_ = true;
}

bool Tutte::SetBoundary(std::span<const size_t> boundary,
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
  for (size_t i = 0; i < boundary_.size(); i++) {
    isBoundary_.Set(boundary_[i]);
    SetVPosition(boundary_[i], polygonPositions.data() + i * d);
  }

  return true;
}

void Tutte::SeedInterior() {
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

  iteration_ = 0;
  lastMaxDelta_ = 0.0;
  converged_ = false;
}

bool Tutte::FixConvexPolygon(std::span<const size_t> boundary,
                              double radius) {
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

void Tutte::SnapshotInterior() {
  size_t N = graph_.Size();
  size_t d = Dim();
  for (size_t u = 0; u < N; u++)
    if (!isBoundary_.Test(u))
      VecCopy(d, GetVPosition(u), scratch_.data() + u * d);
}

const double *Tutte::NeighborReadPos(size_t v) const noexcept {
  if (!useGaussSeidel_ && !isBoundary_.Test(v))
    return scratch_.data() + v * Dim();
  return GetVPosition(v);
}

void Tutte::ComputeBarycenter(size_t u, double *out) const {
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

double Tutte::RelaxVertex(size_t u, double alpha) {
  if (Structure().Degree(u) == 0)
    return 0.0;

  double bary[2];
  ComputeBarycenter(u, bary);

  size_t d = Dim();
  double *old = GetVPosition(u);
  double newp[2];
  double delta = 0.0;
  for (size_t k = 0; k < d; k++) {
    newp[k] = old[k] + alpha * (bary[k] - old[k]);
    double diff = newp[k] - old[k];
    delta += diff * diff;
  }

  SetVPosition(u, newp);
  return std::sqrt(delta);
}

double Tutte::Step(double dt) {
  size_t N = graph_.Size();

  double alpha = relaxationRate_ * dt;
  if (alpha <= 0.0)
    return 0.0;
  if (alpha > 1.0)
    alpha = 1.0;

  if (!useGaussSeidel_)
    SnapshotInterior();

  double maxDelta = 0.0;
  for (size_t u = 0; u < N; u++) {
    if (isBoundary_.Test(u))
      continue;
    double d = RelaxVertex(u, alpha);
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

size_t Tutte::Run(size_t maxIters) {
  if (boundary_.empty())
    throw std::logic_error(
        "gviz::layout::Tutte::Run: no boundary set (call Begin(), "
        "SetBoundary(), or FixConvexPolygon() first)");

  while (!converged_ && iteration_ < maxIters)
    Step(1.0);

  return iteration_;
}

bool Tutte::FixOuterFace() {
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

} // namespace gviz::layout
