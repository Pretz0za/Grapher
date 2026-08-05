#include "ForceAtlas.hpp"

#include "Vec.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <stdexcept>

namespace gviz::layout {

namespace {

// Below this, a global-traction/global-swinging ratio is treated as
// numerically zero rather than divided by. Matches the old C
// gvizForceDirected.h's engine-local gvizNumericEpsilon (1e-7); kept file-
// local here for the same reason it was file-local there -- nothing outside
// this translation unit needs it (gvizForceModel.c never included that
// header either).
constexpr double kNumericEpsilon = 1e-7;

// Chunk size handed to ThreadPool::ForRange for every data-parallel phase
// below. 1 (near-total per-index parallelism) matches the old C
// FORCE_EMBEDDER_PARALLEL_GRAIN.
constexpr size_t kParallelGrain = 1;

// Validated before EmbeddedGraph is ever constructed (see the mem-
// initializer list in ForceAtlas's constructor): dimension is structural,
// exactly like the old C gvizForceEmbedderInit's `if (dimension != 2) return
// -2;` check, which ran before gvizEmbeddedGraphInit did any allocation at
// all. Because this runs as an argument expression to EmbeddedGraph's own
// constructor, throwing here means EmbeddedGraph's constructor -- and the
// wasted position-buffer allocation it would otherwise perform for an
// invalid embedder -- never runs.
size_t RequireDim2(size_t dimension) {
  if (dimension != 2)
    throw DimensionError(
        "ForceAtlas requires dimension == 2 (repulsion uses a 2D quadtree)");
  return dimension;
}

} // namespace

double ForceAtlas::DefaultBoxExtent(size_t vertexCount, double edgeLength) {
  if (vertexCount == 0)
    return edgeLength;
  double side = std::sqrt(static_cast<double>(vertexCount));
  return 0.5 * side * edgeLength;
}

// Builds a fully-grown replacement for every per-vertex vector into local
// temporaries first, then moves them all into place only once every one has
// succeeded -- the same "build into temporaries, publish only on success"
// shape EmbeddedGraph::Sync uses for its CSR rebuild (see EmbeddedGraph.cpp)
// and strictly stronger than the old C growPerVertexArrays, which tolerated
// some arrays growing and others not on OOM (harmless there only because
// state->vertexCount stayed unpublished until every array had grown). Used
// by both the constructor (growing from empty) and Sync (growing from the
// previous vertex count) -- vertices_.size() is always the "old count" that
// determines which entries carry over vs. get freshly zeroed.
void ForceAtlas::GrowPerVertexArraysTo(size_t newCount) {
  // Generic "copy, then resize the copy" helper: T is either size_t or
  // double here, so a default-constructed T{} (0) is always the right fill
  // value for newly grown entries, matching the old C growArray's
  // zero-filled tail.
  auto grown = [newCount]<typename T>(const std::vector<T> &v) {
    std::vector<T> out = v;
    out.resize(newCount, T{});
    return out;
  };
  auto grownX2 = [newCount]<typename T>(const std::vector<T> &v) {
    std::vector<T> out = v;
    out.resize(newCount * 2, T{});
    return out;
  };

  auto newVertices = grown(vertices_);
  auto newDegree = grown(degree_);
  auto newMass = grown(mass_);
  auto newDisp = grownX2(disp_);
  auto newPositionsScratch = grownX2(positionsScratch_);
  auto newAttForceMag = grown(attForceMag_);
  auto newRepForceMag = grown(repForceMag_);
  auto newAppliedDisp = grownX2(appliedDisp_);
  auto newSwinging = grown(swinging_);
  auto newTraction = grown(traction_);
  auto newStructForce = grownX2(structForce_);
  auto newOldStructForce = grownX2(oldStructForce_);

  // Every temporary above has been built successfully by this point; the
  // moves below are all noexcept, so this class's own state is only ever
  // observed either fully at the old size or fully at newCount, never
  // partway grown.
  vertices_ = std::move(newVertices);
  degree_ = std::move(newDegree);
  mass_ = std::move(newMass);
  disp_ = std::move(newDisp);
  positionsScratch_ = std::move(newPositionsScratch);
  attForceMag_ = std::move(newAttForceMag);
  repForceMag_ = std::move(newRepForceMag);
  appliedDisp_ = std::move(newAppliedDisp);
  swinging_ = std::move(newSwinging);
  traction_ = std::move(newTraction);
  structForce_ = std::move(newStructForce);
  oldStructForce_ = std::move(newOldStructForce);
}

void ForceAtlas::GatherPositions() {
  for (size_t i = 0; i < vertices_.size(); i++) {
    const double *vPos = GetVPosition(vertices_[i]);
    positionsScratch_[2 * i] = vPos[0];
    positionsScratch_[2 * i + 1] = vPos[1];
  }
}

double ForceAtlas::VertexRadius(size_t index) const noexcept {
  return radiusBase_ *
         (1.0 + radiusPerDegree_ * std::sqrt(static_cast<double>(degree_[index])));
}

double ForceAtlas::VertexRadiusIfEnabled(size_t idx) const noexcept {
  if (!preventOverlap_)
    return 0.0;
  return VertexRadius(idx);
}

void ForceAtlas::AccumulateBHRepulsion(const QuadTree::Node *node, size_t selfIdx,
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

void ForceAtlas::ComputeForceRange(size_t begin, size_t end) {
  const QuadTree::Node *root = barnesHutEnabled_ ? quadtree_->Root() : nullptr;

  for (size_t i = begin; i < end; i++) {
    size_t v = vertices_[i];
    double *f = disp_.data() + i * 2;
    VecZero(2, f);
    double attF[2] = {0.0, 0.0};
    double repF[2] = {0.0, 0.0};
    double *vPos = positionsScratch_.data() + i * 2;

    // The embedding's synced CSRs, never the live Graph: a mutation stays
    // invisible to physics (and renderers) until Sync() commits it. Out-row
    // plus in-row makes every edge attract both endpoints regardless of
    // direction; the in-row is empty for undirected graphs, whose out rows
    // already hold both directions.
    for (size_t nb : OutNeighbors(v)) {
      const double *uPos = GetVPosition(nb);
      model_->Attractive(2, vPos, uPos, edgeLength_, attF);
    }
    for (size_t nb : InNeighbors(v)) {
      const double *uPos = GetVPosition(nb);
      model_->Attractive(2, vPos, uPos, edgeLength_, attF);
    }

    double vRadius = VertexRadiusIfEnabled(i);
    if (barnesHutEnabled_) {
      AccumulateBHRepulsion(root, i, vRadius, vPos, repF);
    } else {
      for (size_t j = 0; j < vertices_.size(); j++) {
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

void ForceAtlas::ComputeSwingTractionRange(size_t begin, size_t end) {
  for (size_t i = begin; i < end; i++) {
    const double *f = structForce_.data() + i * 2;
    const double *old = oldStructForce_.data() + i * 2;
    double diff[2] = {f[0] - old[0], f[1] - old[1]};
    double sum[2] = {f[0] + old[0], f[1] + old[1]};
    swinging_[i] = mass_[i] * VecNorm2(2, diff);
    traction_[i] = mass_[i] * 0.5 * VecNorm2(2, sum);
  }
}

void ForceAtlas::UpdateGlobalSpeed() {
  double totalSwinging = 0.0, totalEffectiveTraction = 0.0;
  for (size_t i = 0; i < vertices_.size(); i++) {
    totalSwinging += swinging_[i];
    totalEffectiveTraction += traction_[i];
  }

  if (totalEffectiveTraction < kNumericEpsilon)
    return;

  double n = static_cast<double>(vertices_.size());
  double estimatedOptimalJT = 0.05 * std::sqrt(n);
  double minJT = std::sqrt(estimatedOptimalJT);
  double maxJT = 10.0;
  double jt = jitterTolerance_ *
              std::fmax(minJT, std::fmin(maxJT, estimatedOptimalJT *
                                                     totalEffectiveTraction /
                                                     (n * n)));

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

void ForceAtlas::ApplySpeedRange(size_t begin, size_t end) {
  for (size_t i = begin; i < end; i++) {
    double factor = globalSpeed_ / (1.0 + std::sqrt(globalSpeed_ * swinging_[i]));
    const double *f = disp_.data() + i * 2;
    double *out = appliedDisp_.data() + i * 2;
    out[0] = f[0] * factor;
    out[1] = f[1] * factor;
  }
}

// Shared by the constructor and Sync(): degree_[i] is out-degree + in-degree
// from the embedding's synced CSR rows (every incident edge regardless of
// direction; the in term is 0 for undirected graphs, whose out rows already
// hold both directions), and mass follows the model. Sync() must recompute
// every vertex, old and new alike, since a new edge can raise an already-
// tracked vertex's degree without that vertex being new itself.
void ForceAtlas::RecomputeDegreeMass() {
  for (size_t i = 0; i < vertices_.size(); i++) {
    size_t rawId = vertices_[i];
    degree_[i] = OutDegree(rawId) + InDegree(rawId);
    mass_[i] = model_->VertexMass(degree_[i]);
  }
}

// Positions a vertex Sync() just added to physics: near the centroid of
// whatever neighbors (out and, for directed graphs, in) it already has in
// the freshly committed snapshot -- jittered so landing exactly on a
// neighbor doesn't hand the next Step() a zero-distance repulsion -- or, if
// it has none yet, uniformly at random in the layout's box, matching
// Begin()'s own initial placement. @p i is vertices_'s compact index for the
// vertex being placed; must run after EmbeddedGraph::Sync() has committed,
// since it reads the synced rows.
void ForceAtlas::PlaceGrownVertex(size_t i, unsigned int &seed) {
  size_t rawId = vertices_[i];
  double centroid[2] = {0.0, 0.0};
  size_t neighborCount = 0;

  for (size_t nb : OutNeighbors(rawId)) {
    const double *p = GetVPosition(nb);
    centroid[0] += p[0];
    centroid[1] += p[1];
    neighborCount++;
  }
  for (size_t nb : InNeighbors(rawId)) {
    const double *p = GetVPosition(nb);
    centroid[0] += p[0];
    centroid[1] += p[1];
    neighborCount++;
  }

  double unitX = static_cast<double>(rand_r(&seed)) / (static_cast<double>(RAND_MAX) + 1.0);
  double unitY = static_cast<double>(rand_r(&seed)) / (static_cast<double>(RAND_MAX) + 1.0);
  double pos[2];
  if (neighborCount > 0) {
    pos[0] = centroid[0] / static_cast<double>(neighborCount) +
             edgeLength_ * 0.5 * (2.0 * unitX - 1.0);
    pos[1] = centroid[1] / static_cast<double>(neighborCount) +
             edgeLength_ * 0.5 * (2.0 * unitY - 1.0);
  } else {
    pos[0] = boxExtent_ * (2.0 * unitX - 1.0);
    pos[1] = boxExtent_ * (2.0 * unitY - 1.0);
  }

  SetVPosition(rawId, pos);
}

void ForceAtlas::ActionStep(EmbeddedGraph &embedding, void *userData,
                             const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &fa = static_cast<ForceAtlas &>(embedding);
  if (!fa.begun_)
    return;
  fa.Step();
}

void ForceAtlas::ActionToggleOverlapPrevention(EmbeddedGraph &embedding,
                                                void *userData,
                                                const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &fa = static_cast<ForceAtlas &>(embedding);
  fa.preventOverlap_ = !fa.preventOverlap_;
}

ForceAtlas::ForceAtlas(Subgraph subgraph, size_t dimension,
                        std::unique_ptr<ForceModel> model)
    : EmbeddedGraph(std::move(subgraph), RequireDim2(dimension)),
      model_(std::move(model)), pool_(std::make_unique<ThreadPool>()) {
  // First structural commit: builds the synced adjacency CSRs the physics
  // below (and any renderer) reads until the next Sync(). Explicitly
  // qualified because ForceAtlas::Sync (below) hides this name -- see the
  // class comment.
  EmbeddedGraph::Sync();

  size_t vertexCount = Structure().VertexCount();
  GrowPerVertexArraysTo(vertexCount);

  size_t k = 0;
  for (size_t u : Structure())
    vertices_[k++] = u;

  RecomputeDegreeMass();

  boxExtent_ = DefaultBoxExtent(vertexCount, edgeLength_);
  physicsSyncedMutationCount_ = Structure().ParentMutationCount();

  AddAction("forceEmbedder.step", &ForceAtlas::ActionStep);
  AddAction("forceEmbedder.toggleOverlapPrevention",
            &ForceAtlas::ActionToggleOverlapPrevention);

  AddStatSeries("forceEmbedder.maxDisp", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.speed", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.attractiveForce", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.repulsiveForce", StatChartKind::LineLog);
  AddStatSeries("forceEmbedder.gravityForce", StatChartKind::LineLog);
}

void ForceAtlas::Configure(double edgeLength, double boxExtent) {
  if (edgeLength > 0.0)
    edgeLength_ = edgeLength;
  if (boxExtent > 0.0)
    boxExtent_ = boxExtent;
}

void ForceAtlas::ConfigureSpeed(double tolerance) {
  if (tolerance > 0.0)
    jitterTolerance_ = tolerance;
}

void ForceAtlas::ConfigureBarnesHut(double theta, size_t nodesPerCell) {
  if (theta > 0.0)
    theta_ = theta;
  if (nodesPerCell > 0)
    nodesPerCell_ = nodesPerCell;
}

void ForceAtlas::ConfigureOverlapPrevention(double constant) {
  if (constant > 0.0)
    overlapConstant_ = constant;
}

void ForceAtlas::Begin(unsigned int seed) {
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
    if (!quadtree_)
      quadtree_.emplace(positionsScratch_.data(), mass_.data(), vertices_.size(),
                         nodesPerCell_);
    else
      quadtree_->Rebuild(positionsScratch_.data(), mass_.data(), vertices_.size());
  }
}

bool ForceAtlas::Sync(unsigned int seed) {
  // Structural commit first: admits new vertices, grows the position
  // buffer/draw mask, and rebuilds the synced CSRs (O(1) no-op when the
  // graph's mutation counter hasn't moved).
  EmbeddedGraph::Sync();

  // Compare against the live graph's mutation count rather than
  // EmbeddedGraph::Sync()'s own return value: if a previous call committed
  // structurally but failed in the physics growth below,
  // EmbeddedGraph::Sync() reports "nothing new" on this call while the
  // physics is still behind -- this catches that and retries.
  uint64_t current = Structure().ParentMutationCount();
  if (physicsSyncedMutationCount_ == current)
    return false;

  if (seed == 0)
    seed = static_cast<unsigned int>(time(nullptr));

  size_t oldCount = vertices_.size();
  size_t newTotal = Structure().VertexCount();

  GrowPerVertexArraysTo(newTotal);

  // Growth is append-only (no vertex removal anywhere in this stack), so the
  // subgraph's vertex ids in iteration order are exactly vertices_'s
  // existing prefix followed by whatever's new; only the new tail needs
  // writing.
  size_t k = 0;
  for (size_t u : Structure()) {
    if (k >= oldCount)
      vertices_[k] = u;
    k++;
  }

  // Placement reads the committed snapshot for other new vertices too, so it
  // must run only after every new vertex is already registered in
  // vertices_ above.
  for (size_t i = oldCount; i < newTotal; i++)
    PlaceGrownVertex(i, seed);

  RecomputeDegreeMass();

  double grownBoxExtent = DefaultBoxExtent(newTotal, edgeLength_);
  if (grownBoxExtent > boxExtent_)
    boxExtent_ = grownBoxExtent;

  physicsSyncedMutationCount_ = current;

  return true;
}

double ForceAtlas::Step() {
  GatherPositions();
  oldStructForce_ = structForce_;

  if (barnesHutEnabled_)
    quadtree_->Rebuild(positionsScratch_.data(), mass_.data(), vertices_.size());

  pool_->ForRange(0, vertices_.size(), kParallelGrain,
                   [this](size_t b, size_t e) { ComputeForceRange(b, e); });

  pool_->ForRange(0, vertices_.size(), kParallelGrain,
                   [this](size_t b, size_t e) { ComputeSwingTractionRange(b, e); });
  UpdateGlobalSpeed();
  pool_->ForRange(0, vertices_.size(), kParallelGrain,
                   [this](size_t b, size_t e) { ApplySpeedRange(b, e); });

  double maxDisp = 0.0;
  double sumAtt = 0.0, sumRep = 0.0;
  for (size_t i = 0; i < vertices_.size(); i++) {
    double *f = appliedDisp_.data() + i * 2;
    double applied = VecNorm2(2, f);
    if (applied > maxDisp)
      maxDisp = applied;
    sumAtt += attForceMag_[i];
    sumRep += repForceMag_[i];
    AddVPosition(vertices_[i], f);
  }
  double n = static_cast<double>(vertices_.size());
  double meanAtt = vertices_.empty() ? 0.0 : sumAtt / n;
  double meanRep = vertices_.empty() ? 0.0 : sumRep / n;
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

size_t ForceAtlas::Run(size_t maxIters, double epsilon) {
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

} // namespace gviz::layout
