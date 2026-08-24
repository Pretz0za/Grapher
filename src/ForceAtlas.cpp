#include "ForceAtlas.hpp"

#include "Graph.hpp"
#include "Subgraph.hpp"
#include "Vec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <stdexcept>
#include <system_error>

namespace gviz::layout {

namespace {

// Below this, a global-traction/global-swinging ratio is treated as
// numerically zero rather than divided by.
constexpr double kNumericEpsilon = 1e-7;

// Chunk size handed to ThreadPool::ForRange for every data-parallel phase.
constexpr size_t kParallelGrain = 1;

size_t RequireDim2(size_t dimension) {
  if (dimension != 2)
    throw DimensionError(
        "ForceAtlas requires dimension == 2 (repulsion uses a 2D quadtree)");
  return dimension;
}

} // namespace

template <GraphLike G>
double ForceAtlas<G>::DefaultBoxExtent(size_t vertexCount, double edgeLength) {
  if (vertexCount == 0)
    return edgeLength;
  double side = std::sqrt(static_cast<double>(vertexCount));
  return 0.5 * side * edgeLength;
}

// Builds the local-index-space out-adjacency (and, for a directed
// structure_, in-adjacency) CSR exactly once, at construction. An
// undirected structure_ never needs in-adjacency: its out-adjacency
// already holds both directions.
template <GraphLike G>
void ForceAtlas<G>::BuildInAdjacencyIfDirected() {
  size_t n = index_.Size();

  outOffsets_.assign(n + 1, 0);
  for (size_t i = 0; i < n; i++)
    outOffsets_[i + 1] = outOffsets_[i] + structure_.Degree(index_.ToRaw(i));
  outNeighborsLocal_.resize(outOffsets_[n]);
  {
    size_t cursor = 0;
    for (size_t i = 0; i < n; i++)
      for (size_t nb : structure_.Neighbors(index_.ToRaw(i)))
        outNeighborsLocal_[cursor++] = index_.ToLocal(nb);
  }

  if (!GraphLikeIsDirected(structure_))
    return;

  inOffsets_.assign(n + 1, 0);
  for (size_t j : outNeighborsLocal_)
    inOffsets_[j + 1]++;
  for (size_t i = 0; i < n; i++)
    inOffsets_[i + 1] += inOffsets_[i];

  inNeighborsLocal_.resize(inOffsets_[n]);
  std::vector<size_t> cursor(inOffsets_.begin(), inOffsets_.begin() + n);
  for (size_t i = 0; i < n; i++)
    for (size_t j = outOffsets_[i]; j < outOffsets_[i + 1]; j++) {
      size_t nb = outNeighborsLocal_[j];
      inNeighborsLocal_[cursor[nb]++] = i;
    }
}

template <GraphLike G>
void ForceAtlas<G>::GatherPositions() {
  size_t n = index_.Size();
  for (size_t i = 0; i < n; i++) {
    const double *vPos = EmbeddedGraph::GetVPosition(i);
    positionsScratch_[2 * i] = vPos[0];
    positionsScratch_[2 * i + 1] = vPos[1];
  }
}

template <GraphLike G>
double ForceAtlas<G>::VertexRadius(size_t index) const noexcept {
  return radiusBase_ *
         (1.0 + radiusPerDegree_ * std::sqrt(static_cast<double>(degree_[index])));
}

template <GraphLike G>
double ForceAtlas<G>::VertexRadiusIfEnabled(size_t idx) const noexcept {
  if (!preventOverlap_)
    return 0.0;
  return VertexRadius(idx);
}

template <GraphLike G>
void ForceAtlas<G>::AccumulateBHRepulsion(const QuadTree::Node *node, size_t selfIdx,
                                           double vRadius, const double *vPos,
                                           double *acc) const {
  if (!node || node->Mass() == 0.0)
    return;

  if (node->IsLeaf()) {
    size_t count = node->PointCount();
    for (size_t i = 0; i < count; i++) {
      size_t idx = node->PointAt(i);
      if (idx == selfIdx)
        continue;
      const double *uPos = positionsScratch_.data() + idx * 2;
      double otherRadius = VertexRadiusIfEnabled(idx);
      model_->Repulsive(2, vPos, uPos, mass_[selfIdx], mass_[idx], vRadius,
                         otherRadius, overlapConstant_, edgeLength_, acc);
    }
    return;
  }

  double comX, comY;
  node->CenterOfMass(&comX, &comY);
  double dx = comX - vPos[0];
  double dy = comY - vPos[1];
  double dist = std::sqrt(dx * dx + dy * dy);
  double ratio = (2.0 * node->HalfSize()) / dist;

  if (ratio < theta_) {
    double com[2] = {comX, comY};
    model_->Repulsive(2, vPos, com, mass_[selfIdx], node->Mass(), vRadius, 0.0,
                       overlapConstant_, edgeLength_, acc);
    return;
  }

  for (size_t q = 0; q < QuadTree::kQuadrantCount; q++)
    AccumulateBHRepulsion(node->Child(static_cast<QuadTree::Quadrant>(q)),
                           selfIdx, vRadius, vPos, acc);
}

template <GraphLike G>
void ForceAtlas<G>::ComputeForceRange(size_t begin, size_t end) {
  const QuadTree::Node *root = barnesHutEnabled_ ? quadtree_->Root() : nullptr;

  for (size_t i = begin; i < end; i++) {
    double *f = disp_.data() + i * 2;
    VecZero(2, f);
    double attF[2] = {0.0, 0.0};
    double repF[2] = {0.0, 0.0};
    double *vPos = positionsScratch_.data() + i * 2;

    // Out plus in makes every edge attract both endpoints regardless of
    // direction; in is empty for undirected structures, whose out rows
    // already hold both directions.
    for (size_t j = outOffsets_[i]; j < outOffsets_[i + 1]; j++) {
      const double *uPos = EmbeddedGraph::GetVPosition(outNeighborsLocal_[j]);
      model_->Attractive(2, vPos, uPos, edgeLength_, attF);
    }
    if (!inOffsets_.empty()) {
      for (size_t j = inOffsets_[i]; j < inOffsets_[i + 1]; j++) {
        const double *uPos = EmbeddedGraph::GetVPosition(inNeighborsLocal_[j]);
        model_->Attractive(2, vPos, uPos, edgeLength_, attF);
      }
    }

    double vRadius = VertexRadiusIfEnabled(i);
    if (barnesHutEnabled_) {
      AccumulateBHRepulsion(root, i, vRadius, vPos, repF);
    } else {
      size_t n = index_.Size();
      for (size_t j = 0; j < n; j++) {
        if (j == i)
          continue;
        const double *otherPos = positionsScratch_.data() + j * 2;
        double otherRadius = VertexRadiusIfEnabled(j);
        model_->Repulsive(2, vPos, otherPos, mass_[i], mass_[j], vRadius,
                           otherRadius, overlapConstant_, edgeLength_, repF);
      }
    }

    VecAxpy(2, 1.0, attF, f);
    VecAxpy(2, 1.0, repF, f);
    attForceMag_[i] = VecNorm2(2, attF);
    repForceMag_[i] = VecNorm2(2, repF);
    VecCopy(2, f, structForce_.data() + i * 2);

    VecAccGravityForce(2, vPos, gravityK_, f);
  }
}

template <GraphLike G>
void ForceAtlas<G>::ComputeSwingTractionRange(size_t begin, size_t end) {
  for (size_t i = begin; i < end; i++) {
    const double *f = structForce_.data() + i * 2;
    const double *old = oldStructForce_.data() + i * 2;
    double diff[2] = {f[0] - old[0], f[1] - old[1]};
    double sum[2] = {f[0] + old[0], f[1] + old[1]};
    swinging_[i] = mass_[i] * VecNorm2(2, diff);
    traction_[i] = mass_[i] * 0.5 * VecNorm2(2, sum);
  }
}

template <GraphLike G>
void ForceAtlas<G>::UpdateGlobalSpeed() {
  double totalSwinging = 0.0, totalEffectiveTraction = 0.0;
  size_t n = index_.Size();
  for (size_t i = 0; i < n; i++) {
    totalSwinging += swinging_[i];
    totalEffectiveTraction += traction_[i];
  }

  if (totalEffectiveTraction < kNumericEpsilon)
    return;

  double nD = static_cast<double>(n);
  double estimatedOptimalJT = 0.05 * std::sqrt(nD);
  double minJT = std::sqrt(estimatedOptimalJT);
  double maxJT = 10.0;
  double jt = jitterTolerance_ *
              std::fmax(minJT, std::fmin(maxJT, estimatedOptimalJT *
                                                     totalEffectiveTraction /
                                                     (nD * nD)));

  if (totalSwinging / totalEffectiveTraction > 2.0) {
    if (speedEfficiency_ > kSpeedEfficiencyMin)
      speedEfficiency_ *= 0.5;
    jt = std::fmax(jt, jitterTolerance_);
  }

  double targetSpeed = totalSwinging > kNumericEpsilon
                            ? jt * speedEfficiency_ * totalEffectiveTraction /
                                  totalSwinging
                            : globalSpeed_;

  if (totalSwinging > jt * totalEffectiveTraction) {
    if (speedEfficiency_ > kSpeedEfficiencyMin)
      speedEfficiency_ *= 0.7;
  } else if (globalSpeed_ < 1000.0) {
    speedEfficiency_ *= 1.3;
  }

  speedEfficiency_ = std::clamp(speedEfficiency_, kSpeedEfficiencyMin, kSpeedEfficiencyMax);

  globalSpeed_ += std::fmin(targetSpeed - globalSpeed_,
                             kSpeedMaxRise * globalSpeed_);
}

template <GraphLike G>
void ForceAtlas<G>::ApplySpeedRange(size_t begin, size_t end) {
  for (size_t i = begin; i < end; i++) {
    double factor = globalSpeed_ / (1.0 + std::sqrt(globalSpeed_ * swinging_[i]));
    const double *f = disp_.data() + i * 2;
    double *out = appliedDisp_.data() + i * 2;
    out[0] = f[0] * factor;
    out[1] = f[1] * factor;
  }
}

// pool_ may be null if the worker thread(s) failed to start; fall back to
// running the whole range synchronously on the calling thread.
template <GraphLike G>
void ForceAtlas<G>::RunForRange(size_t begin, size_t end, size_t grain,
                                 const std::function<void(size_t, size_t)> &task) {
  if (begin >= end)
    return;
  if (grain == 0)
    grain = 1;
  if (pool_) {
    pool_->ForRange(begin, end, grain, task);
  } else {
    task(begin, end);
  }
}

template <GraphLike G>
void ForceAtlas<G>::RecomputeDegreeMass() {
  size_t n = index_.Size();
  for (size_t i = 0; i < n; i++) {
    size_t outDeg = outOffsets_[i + 1] - outOffsets_[i];
    size_t inDeg = inOffsets_.empty() ? 0 : (inOffsets_[i + 1] - inOffsets_[i]);
    degree_[i] = outDeg + inDeg;
    mass_[i] = model_->VertexMass(degree_[i]);
  }
}

template <GraphLike G>
void ForceAtlas<G>::ActionStep(EmbeddedGraph &embedding, void *userData,
                                const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &fa = static_cast<ForceAtlas<G> &>(embedding);
  if (!fa.begun_)
    return;
  fa.Step();
}

template <GraphLike G>
void ForceAtlas<G>::ActionToggleOverlapPrevention(EmbeddedGraph &embedding,
                                                   void *userData,
                                                   const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &fa = static_cast<ForceAtlas<G> &>(embedding);
  fa.preventOverlap_ = !fa.preventOverlap_;
}

template <GraphLike G>
ForceAtlas<G>::ForceAtlas(G structure, size_t dimension,
                           std::unique_ptr<ForceModel> model)
    : EmbeddedGraph(GraphLikeVertexCount(structure), RequireDim2(dimension)),
      structure_(std::move(structure)), index_(structure_), model_(std::move(model)) {
  // Null pool is fine: RunForRange falls back to running serially. Built
  // in the body (not the member-init list) so a failed pthread_create's
  // std::system_error can be caught here.
  try {
    pool_ = std::make_unique<ThreadPool>();
  } catch (const std::system_error &) {
    pool_.reset();
  }

  size_t vertexCount = index_.Size();
  degree_.assign(vertexCount, 0);
  mass_.assign(vertexCount, 0.0);
  disp_.assign(vertexCount * 2, 0.0);
  positionsScratch_.assign(vertexCount * 2, 0.0);
  attForceMag_.assign(vertexCount, 0.0);
  repForceMag_.assign(vertexCount, 0.0);
  appliedDisp_.assign(vertexCount * 2, 0.0);
  swinging_.assign(vertexCount, 0.0);
  traction_.assign(vertexCount, 0.0);
  structForce_.assign(vertexCount * 2, 0.0);
  oldStructForce_.assign(vertexCount * 2, 0.0);

  BuildInAdjacencyIfDirected();
  RecomputeDegreeMass();

  boxExtent_ = DefaultBoxExtent(vertexCount, edgeLength_);

  AddAction("forceEmbedder.step", &ForceAtlas<G>::ActionStep);
  AddAction("forceEmbedder.toggleOverlapPrevention",
            &ForceAtlas<G>::ActionToggleOverlapPrevention);

  AddStatSeries("forceEmbedder.maxDisp", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.speed", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.attractiveForce", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.repulsiveForce", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.gravityForce", StatChartKind::LineLog);
}

template <GraphLike G>
void ForceAtlas<G>::Configure(double edgeLength, double boxExtent) {
  if (edgeLength > 0.0)
    edgeLength_ = edgeLength;
  if (boxExtent > 0.0)
    boxExtent_ = boxExtent;
}

template <GraphLike G>
void ForceAtlas<G>::ConfigureSpeed(double tolerance) {
  if (tolerance > 0.0)
    jitterTolerance_ = tolerance;
}

template <GraphLike G>
void ForceAtlas<G>::ConfigureBarnesHut(double theta, size_t nodesPerCell) {
  if (theta > 0.0)
    theta_ = theta;
  if (nodesPerCell > 0)
    nodesPerCell_ = nodesPerCell;
}

template <GraphLike G>
void ForceAtlas<G>::ConfigureOverlapPrevention(double constant) {
  if (constant > 0.0)
    overlapConstant_ = constant;
}

template <GraphLike G>
void ForceAtlas<G>::Begin(unsigned int seed) {
  RandomizePositions(boxExtent_, seed);

  globalSpeed_ = kSpeedInitial;
  speedEfficiency_ = kSpeedEfficiencyInitial;
  std::fill(disp_.begin(), disp_.end(), 0.0);
  std::fill(structForce_.begin(), structForce_.end(), 0.0);
  std::fill(oldStructForce_.begin(), oldStructForce_.end(), 0.0);

  iteration_ = 0;
  lastMaxDisplacement_ = 0.0;
  begun_ = true;

  GatherPositions();

  if (barnesHutEnabled_) {
    size_t n = index_.Size();
    if (!quadtree_)
      quadtree_.emplace(positionsScratch_.data(), mass_.data(), n, nodesPerCell_);
    else
      quadtree_->Rebuild(positionsScratch_.data(), mass_.data(), n);
  }
}

template <GraphLike G>
double ForceAtlas<G>::Step() {
  GatherPositions();
  oldStructForce_ = structForce_;
  size_t n = index_.Size();

  if (barnesHutEnabled_)
    quadtree_->Rebuild(positionsScratch_.data(), mass_.data(), n);

  RunForRange(0, n, kParallelGrain,
              [this](size_t b, size_t e) { ComputeForceRange(b, e); });

  RunForRange(0, n, kParallelGrain,
              [this](size_t b, size_t e) { ComputeSwingTractionRange(b, e); });
  UpdateGlobalSpeed();
  RunForRange(0, n, kParallelGrain,
              [this](size_t b, size_t e) { ApplySpeedRange(b, e); });

  double maxDisp = 0.0;
  double sumAtt = 0.0, sumRep = 0.0;
  for (size_t i = 0; i < n; i++) {
    double *f = appliedDisp_.data() + i * 2;
    double applied = VecNorm2(2, f);
    if (applied > maxDisp)
      maxDisp = applied;
    sumAtt += attForceMag_[i];
    sumRep += repForceMag_[i];
    EmbeddedGraph::AddVPosition(i, f);
  }
  double nD = static_cast<double>(n);
  double meanAtt = n == 0 ? 0.0 : sumAtt / nD;
  double meanRep = n == 0 ? 0.0 : sumRep / nD;
  double meanGrav = gravityK_;

  iteration_++;
  lastMaxDisplacement_ = maxDisp;
  StatAppend("forceEmbedder.maxDisp", maxDisp);
  StatAppend("forceEmbedder.speed", globalSpeed_);
  StatAppend("forceEmbedder.attractiveForce", meanAtt);
  StatAppend("forceEmbedder.repulsiveForce", meanRep);
  StatAppend("forceEmbedder.gravityForce", meanGrav);

  return maxDisp;
}

template <GraphLike G>
size_t ForceAtlas<G>::Run(size_t maxIters, double epsilon) {
  if (!begun_)
    throw std::logic_error("ForceAtlas::Run called before Begin()");

  size_t rounds = 0;
  while (rounds < maxIters) {
    double maxDisp = Step();
    rounds++;
    if (maxDisp < epsilon)
      break;
  }
  return rounds;
}

// Explicit instantiation for the two GraphLike types this codebase uses.
template class ForceAtlas<Graph>;
template class ForceAtlas<Subgraph>;

} // namespace gviz::layout
