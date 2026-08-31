#include "GRIP.hpp"

#include "Error.hpp"
#include "Vec.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <system_error>
#include <utility>

namespace gviz::layout {

namespace {

/**
 * GVIZ_GRIP_DEBUG_FILTRATION / GVIZ_GRIP_STAGE_TIMING: opt-in, stderr-only
 * debug logging documented in README.md's "Debug and profiling environment
 * variables" section. Direct port of the old C gvizGRIPEmbedder.c's
 * GRIP_FILTRATION_DEBUG() macro and gvizGRIPEmbedderNextStage's inline
 * getenv("GVIZ_GRIP_STAGE_TIMING") check -- both log sites (IterMISFiltration/
 * VerticesWithinRadius below, and NextStage) preserve the exact line formats
 * README documents, since GRIPFiltrationDebug/GRIPPlaceBench (the C++ ports
 * of the old white-box tools that exercise this output) drive GRIP through
 * this same public Begin()/NextStage() path, not a reimplementation.
 */
bool FiltrationDebugEnabled() {
  return std::getenv("GVIZ_GRIP_DEBUG_FILTRATION") != nullptr;
}

constexpr double kNumericEpsilon = 1e-7;
constexpr double kEdgeLength = 10.0;
constexpr size_t kKnnCapacityDefault = 256;
constexpr size_t kKMaxDefault = 128;
constexpr size_t kParallelGrain = 1;
constexpr size_t kMigrateBfsDepth = 64;
constexpr size_t kMaxRoundsPerLayer = 30;
constexpr double kConvergenceFactor = 1e-3;
// GRIP applies VecAccGRIPFRAttForce (a repulsive-shaped op used as GRIP's
// "attractive" spring to k-nearest non-edge vertices) scaled down relative
// to the along-edge repulsion, matching the old C gvizForceDirected.c's
// FR_SCALE_FACTOR.
constexpr double kFrScaleFactor = 0.05;

size_t ClampK(size_t k, size_t minK, size_t maxK) {
  if (k < minK)
    k = minK;
  if (k > maxK)
    k = maxK;
  return k;
}

/**
 * Generates the n+1 vertices of a regular n-simplex centered at the origin,
 * scaled so every pairwise distance equals @p sideLength. @p out must hold
 * (n+1)*n doubles. Direct port of the old C makeRegularSimplex (Gram-matrix
 * construction: each new vertex is placed to be equidistant from every
 * previously placed one, then the whole thing is rescaled from the unit
 * simplex's edge length to @p sideLength).
 */
void MakeRegularSimplex(size_t n, double sideLength, double *out) {
  std::fill(out, out + (n + 1) * n, 0.0);

  for (size_t k = 0; k <= n; k++) {
    double *vk = out + k * n;

    if (k == 0) {
      vk[0] = 1.0;
      continue;
    }

    // Unit regular simplex: dot(vi, vj) = -1/n for all i != j.
    double c = -1.0 / static_cast<double>(n);

    for (size_t j = 0; j < k; j++) {
      double *vj = out + j * n;
      double dotSoFar = 0.0;
      for (size_t m = 0; m < j; m++)
        dotSoFar += vk[m] * vj[m];
      vk[j] = (c - dotSoFar) / vj[j];
    }

    double normSq = 0.0;
    for (size_t m = 0; m < k; m++)
      normSq += vk[m] * vk[m];
    if (k < n) {
      double rem = 1.0 - normSq;
      vk[k] = rem > 0.0 ? std::sqrt(rem) : 0.0;
    }
  }

  double *v0 = out;
  double *v1 = out + n;
  double unitEdge = 0.0;
  for (size_t m = 0; m < n; m++) {
    double d = v0[m] - v1[m];
    unitEdge += d * d;
  }
  unitEdge = std::sqrt(unitEdge);

  double scale = sideLength / unitEdge;
  for (size_t i = 0; i < (n + 1) * n; i++)
    out[i] *= scale;
}

/** Solves the 3x3 linear system a*x = b via partial-pivot Gaussian
 *  elimination, in place on @p a. Returns false (leaving @p x untouched) if
 *  @p a is (numerically) singular. Direct port of the old C solve3x3, used
 *  only by GRIP::RemoveNetRotation's 3D inertia-tensor solve. */
bool Solve3x3(double a[3][3], const double *b, double *x) {
  size_t idx[3] = {0, 1, 2};
  double rhs[3] = {b[0], b[1], b[2]};

  for (size_t col = 0; col < 3; col++) {
    size_t piv = col;
    for (size_t row = col + 1; row < 3; row++)
      if (std::fabs(a[idx[row]][col]) > std::fabs(a[idx[piv]][col]))
        piv = row;
    if (std::fabs(a[idx[piv]][col]) < kNumericEpsilon)
      return false;
    std::swap(idx[col], idx[piv]);

    for (size_t row = col + 1; row < 3; row++) {
      double f = a[idx[row]][col] / a[idx[col]][col];
      for (size_t j = col; j < 3; j++)
        a[idx[row]][j] -= f * a[idx[col]][j];
      rhs[idx[row]] -= f * rhs[idx[col]];
    }
  }

  for (size_t col = 3; col-- > 0;) {
    double s = rhs[idx[col]];
    for (size_t j = col + 1; j < 3; j++)
      s -= a[idx[col]][j] * x[j];
    x[col] = s / a[idx[col]][col];
  }
  return true;
}

/**
 * Validates @p dimension/@p sg before EmbeddedGraph's (potentially large)
 * base allocation runs, and forwards @p sg through unchanged on success.
 * Used in GRIP's constructor's base-class initializer so a rejected
 * construction (bad dimension, too few active vertices) never allocates the
 * position buffer at all.
 */
Subgraph ValidateForGRIP(Subgraph sg, size_t dimension) {
  if (dimension < 2 || dimension > 4)
    throw DimensionError("GRIP requires an embedding dimension of 2, 3, or 4");
  if (sg.VertexCount() < dimension + 1)
    throw InsufficientVerticesError(
        "GRIP needs at least dimension + 1 active vertices to place the "
        "coarsest simplex");
  return sg;
}

} // namespace

GRIP::GRIP(Subgraph subgraph, size_t diameter, size_t dimension, Config config)
    : EmbeddedGraph(ValidateForGRIP(std::move(subgraph), dimension), dimension),
      knnCapacity_(config.knnCapacity > 0 ? config.knnCapacity
                                           : kKnnCapacityDefault),
      placementKMax_(kKMaxDefault), refinementKMax_(kKMaxDefault),
      kPolicy_(KPolicy::Constant), statsEnabled_(config.statsEnabled),
      radiusBfsScratch_(Structure().VertexCapacity()) {
  size_t n = Structure().VertexCapacity();
  size_t activeCount = Structure().VertexCount();

  size_t misBorderReserve = 1024;
  if (diameter)
    misBorderReserve =
        static_cast<size_t>(std::log2(static_cast<double>(diameter))) + 8;
  misBorder_.reserve(misBorderReserve);
  misBorder_.push_back(activeCount);

  misFiltration_.assign(n, 0);
  dec_.resize(n);
  for (auto &d : dec_) {
    d.knn.resize(knnCapacity_);
    d.disp.assign(dimension, 0.0);
    d.oldDisp.assign(dimension, 0.0);
  }

  dispCalculated_ = BitSet(n);
  radiusBfsDepth_.assign(n, 0);

  // EmbeddedGraph's base constructor defaults the draw mask to "every
  // subgraph vertex visible" (the right default for a static layout).
  // GRIP instead builds visibility up one MIS layer at a time (Begin()/
  // NextStage() call DrawMaskShowVertex as each layer is placed), so it
  // must start from nothing visible -- matching the old C Init's
  // gripClearDrawMask(state) call.
  DrawMaskClearVertices();

  AddAction("grip.refineRound", &GRIP::ActionRefineRound, nullptr);
  AddAction("grip.nextStage", &GRIP::ActionNextStage, nullptr);

  if (statsEnabled_) {
    AddStatSeries("grip.heat", StatChartKind::LineLog);
    AddStatSeries("grip.meanDisp", StatChartKind::LineLog);
    AddStatSeries("grip.maxDisp", StatChartKind::LineLog);
    AddStatSeries("grip.meanForce", StatChartKind::LineLog);
  }

  // NULL pool is fine: parallel phases fall back to running serially (see
  // RunForRange). Unlike the old C gvizThreadPoolCreate (which tolerated
  // partial worker-startup failure and only failed if zero workers came
  // up), gviz::ThreadPool throws immediately on the first worker failure;
  // GRIP treats that identically to "no pool available" rather than
  // propagating it, since a construction failure here would be surprising
  // for what is, from the caller's perspective, a resource GRIP manages
  // internally purely as a performance optimization.
  try {
    pool_ = std::make_unique<ThreadPool>(0);
  } catch (const std::system_error &) {
    pool_.reset();
  }

  knnScratchCount_ = pool_ ? pool_->ThreadCount() + 1 : 1;
  knnScratch_.reserve(knnScratchCount_);
  for (size_t i = 0; i < knnScratchCount_; i++)
    knnScratch_.emplace_back(Structure().VertexCapacity());
}

// CONFIGURATION: -------------------------------------------------------------

void GRIP::ConfigureK(size_t placementKMax, size_t refinementKMax,
                       KPolicy policy) {
  kPolicy_ = policy;
  if (placementKMax > 0)
    placementKMax_ =
        placementKMax > knnCapacity_ ? knnCapacity_ : placementKMax;
  if (refinementKMax > 0)
    refinementKMax_ =
        refinementKMax > knnCapacity_ ? knnCapacity_ : refinementKMax;
}

size_t GRIP::ComputeK(size_t maxK, bool forPlacement) const {
  size_t minK = Dim() + 1;
  size_t cap = knnCapacity_;
  if (maxK > cap)
    maxK = cap;

  KPolicy policy = kPolicy_;
  if (forPlacement && policy == KPolicy::PlacementDecay)
    policy = KPolicy::LayerDecay;
  else if (!forPlacement && policy == KPolicy::PlacementDecay)
    policy = KPolicy::Constant;
  else if (forPlacement && policy == KPolicy::Budget)
    policy = KPolicy::LayerDecay;

  size_t k;
  switch (policy) {
  case KPolicy::Constant:
    k = maxK;
    break;
  case KPolicy::LayerDecay:
    k = maxK >> currLayer_;
    break;
  case KPolicy::LayerGrow: {
    size_t depthFromCoarse =
        layerCount_ > 0 ? layerCount_ - 1 - currLayer_ : 0;
    k = maxK >> depthFromCoarse;
    break;
  }
  case KPolicy::Budget: {
    size_t active = MisBorderAt(currLayer_);
    size_t finest = MisBorderAt(0);
    if (active == 0)
      return ClampK(maxK, minK, cap);
    double scaled = static_cast<double>(maxK) *
                    (static_cast<double>(finest) / static_cast<double>(active));
    k = scaled >= static_cast<double>(cap) ? cap : static_cast<size_t>(scaled);
    return ClampK(k, minK, cap);
  }
  case KPolicy::PlacementDecay:
    k = maxK;
    break;
  default:
    k = maxK;
    break;
  }

  return ClampK(k, minK, maxK);
}

size_t GRIP::PlacementK() const { return ComputeK(placementKMax_, true); }
size_t GRIP::RefinementK() const { return ComputeK(refinementKMax_, false); }

// ACTIONS: -------------------------------------------------------------------

void GRIP::ActionRefineRound(EmbeddedGraph &embedding, void *userData,
                              const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &grip = static_cast<GRIP &>(embedding);
  if (grip.layerCount_ == 0)
    return; // Begin() has not run yet
  grip.RefineRound();
}

void GRIP::ActionNextStage(EmbeddedGraph &embedding, void *userData,
                            const ActionPayload &payload) {
  (void)userData;
  (void)payload;
  auto &grip = static_cast<GRIP &>(embedding);
  if (grip.layerCount_ == 0)
    return;
  grip.NextStage();
}

void GRIP::SyncDrawMask() {
  DrawEdgePolicy edges = currLayer_ == 0 ? DrawEdgePolicy::IfBothVisible
                                         : DrawEdgePolicy::None;
  SetDrawMaskEdgePolicy(edges);
}

// SCRATCH / SCHEDULING HELPERS: -----------------------------------------------

search::KNearestScratch &GRIP::KnnScratchForCaller() {
  size_t slot = pool_ ? pool_->WorkerSlot() : 0;
  if (slot >= knnScratch_.size())
    slot = 0;
  return knnScratch_[slot];
}

void GRIP::RunForRange(size_t begin, size_t end, size_t grain,
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

void GRIP::Barycenter(std::span<const search::FoundVertex> neighbors,
                       double *out) const {
  size_t dim = Dim();
  VecZero(dim, out);
  for (const auto &fv : neighbors)
    VecAxpy(dim, 1.0, GetVPosition(fv.v), out);
  VecScale(dim, 1.0 / static_cast<double>(neighbors.size()), out);
}

// MIS FILTRATION: --------------------------------------------------------------

// NOTE: could potentially be improved by randomizing the order vertices are
// visited in, either by shuffling or assigning random priorities to each
// vertex (carried over from the old C's comment -- still true here).
void GRIP::MakeFirstMISPartition(BitSet &out) {
  size_t writePos = MisBorderAt(0) - 1;
  BitSet states(Structure().VertexCapacity());

  for (size_t i : Structure()) {
    if (states.Test(i))
      continue;

    out.Set(i);

    for (size_t neighbor : Structure().Neighbors(i)) {
      if (!states.Test(neighbor)) {
        states.Set(neighbor);
        misFiltration_[writePos--] = neighbor;
      }
    }
  }

  size_t border = out.Popcount();
  misBorder_.push_back(border);
}

void GRIP::VerticesWithinRadius(size_t source, size_t maxDepth, BitSet &out) {
  size_t epoch = radiusBfsScratch_.BeginEpoch();
  radiusBfsScratch_.MarkVisited(source, epoch);
  radiusBfsDepth_[source] = 0;

  auto &queue = radiusBfsScratch_.Queue();
  queue.push_back(search::FoundVertex{source, 0});

  size_t dbgVisited = 1;
  size_t dbgMarked = 0;
  size_t dbgQueuePeak = queue.size();

  while (!queue.empty()) {
    search::FoundVertex nd = queue.front();
    queue.pop_front();

    if (maxDepth && nd.dist >= maxDepth)
      continue;

    for (size_t neighbor : Structure().Neighbors(nd.v)) {
      if (radiusBfsScratch_.Visited(neighbor, epoch))
        continue;
      radiusBfsScratch_.MarkVisited(neighbor, epoch);
      dbgVisited++;

      size_t nextDepth = nd.dist + 1;
      radiusBfsDepth_[neighbor] = nextDepth;
      if (nextDepth <= maxDepth) {
        out.Set(neighbor);
        dbgMarked++;
      }

      queue.push_back(search::FoundVertex{neighbor, nextDepth});
      dbgQueuePeak = std::max(dbgQueuePeak, queue.size());
    }
  }

  // dbgPushFail is always 0: std::deque::push_back either succeeds or
  // throws std::bad_alloc (which would unwind out of this function
  // entirely), unlike the old gvizDeque's checked push failure -- there is
  // no soft-fail path left here for the old "[grip-bfs] push fail ..." line
  // to report, so that line has no C++ equivalent. dbgQueuePeak reports the
  // largest BFS-frontier size actually reached instead of the old code's
  // `queue->capacity` (std::deque manages its own capacity and doesn't
  // expose it) -- a real "peak" value that fits the "queuePeak" label at
  // least as well as the field it replaces.
  if (FiltrationDebugEnabled() && maxDepth >= 64) {
    static size_t dbgCalls = 0;
    if (dbgCalls++ < 3)
      std::fprintf(stderr,
                    "[grip-bfs] src=%zu maxDepth=%zu visited=%zu marked=%zu "
                    "pushFail=%zu queuePeak=%zu\n",
                    source, maxDepth, dbgVisited, dbgMarked, size_t{0}, dbgQueuePeak);
  }
}

bool GRIP::IterMISFiltration(size_t i, BitSet &vertices) {
  size_t nvertices = Structure().VertexCapacity();
  BitSet newVertices(nvertices);
  BitSet newMisStates(nvertices);

  size_t radius = size_t{1} << (i - 1);

  size_t dbgPicked = 0;
  size_t dbgSkipped = 0;
  for (size_t curr : vertices) {
    if (newMisStates.Test(curr)) {
      dbgSkipped++;
      continue;
    }

    newVertices.Set(curr);
    newMisStates.Set(curr);
    dbgPicked++;

    VerticesWithinRadius(curr, radius, newMisStates);
  }

  size_t writePos = MisBorderAt(i - 1) - 1;
  size_t dbgRemoved = 0;
  for (size_t curr : vertices) {
    if (!newVertices.Test(curr)) {
      misFiltration_[writePos--] = curr;
      dbgRemoved++;
    }
  }

  size_t border = newVertices.Popcount();
  bool cont = border > Dim() + 1;

  if (FiltrationDebugEnabled() && (i >= 6 || border <= 1100)) {
    std::fprintf(stderr,
                  "[grip-filt] layer=%zu radius=%zu in=%zu picked=%zu skipped=%zu "
                  "removed=%zu out=%zu continue=%d\n",
                  i, radius, dbgPicked + dbgSkipped, dbgPicked, dbgSkipped,
                  dbgRemoved, border, cont ? 1 : 0);
  }

  misBorder_.push_back(border);
  vertices = newVertices;

  return cont;
}

/**
 * Among misFiltration_[candBegin, candEnd), picks the vertex whose graph
 * distance to the current final set (misFiltration_[0, finalEnd)) is
 * largest. Runs one multi-source BFS capped at @p maxDepth (GRIP-style local
 * search); candidates beyond the cap are treated as equally far and
 * preferred over nearer ones. Direct port of the old C gripPickFarCandidate.
 */
size_t GRIP::PickFarCandidate(size_t finalEnd, size_t candBegin,
                               size_t candEnd, size_t maxDepth) {
  size_t epoch = radiusBfsScratch_.BeginEpoch();
  auto &queue = radiusBfsScratch_.Queue();

  for (size_t i = 0; i < finalEnd; i++) {
    size_t v = misFiltration_[i];
    radiusBfsScratch_.MarkVisited(v, epoch);
    radiusBfsDepth_[v] = 0;
    queue.push_back(search::FoundVertex{v, 0});
  }

  while (!queue.empty()) {
    search::FoundVertex nd = queue.front();
    queue.pop_front();
    if (nd.dist >= maxDepth)
      continue;

    for (size_t neighbor : Structure().Neighbors(nd.v)) {
      if (radiusBfsScratch_.Visited(neighbor, epoch))
        continue;
      radiusBfsScratch_.MarkVisited(neighbor, epoch);
      size_t nextDepth = nd.dist + 1;
      radiusBfsDepth_[neighbor] = nextDepth;
      queue.push_back(search::FoundVertex{neighbor, nextDepth});
    }
  }

  size_t bestIdx = candBegin;
  size_t bestScore = 0;
  size_t beyondDepth = maxDepth + 1;

  for (size_t j = candBegin; j < candEnd; j++) {
    size_t v = misFiltration_[j];
    size_t score = radiusBfsScratch_.Visited(v, epoch) ? radiusBfsDepth_[v]
                                                        : beyondDepth;
    if (score > bestScore || (score == bestScore && j > bestIdx)) {
      bestScore = score;
      bestIdx = j;
    }
  }
  return bestIdx;
}

// The pool immediately behind the final layer (layerCount-2) can be
// exhausted (fully absorbed) before the final layer reaches dim+1 members,
// e.g. when MIS coarsening collapses a small subgraph to a single vertex two
// rounds running. When that happens, finalEnd has caught up to that layer's
// border, so its vertices are contiguous with the next-shallower layer's
// drop set; walk outward through count-3, count-4, ... down to layer 0 (the
// full active set) until a non-empty pool is found. Returns false only when
// layer 0 itself is exhausted, which means the subgraph has fewer than
// dim+1 vertices in total (unreachable given the constructor's
// InsufficientVerticesError precondition, kept as a defensive fallback).
bool GRIP::MigrateOneToFinalLayer(size_t count) {
  size_t finalEnd = MisBorderAt(count - 1);

  size_t srcLayer = count - 1;
  size_t candEnd;
  do {
    if (srcLayer == 0)
      return false;
    srcLayer--;
    candEnd = MisBorderAt(srcLayer);
  } while (candEnd <= finalEnd);

  size_t pick = PickFarCandidate(finalEnd, finalEnd, candEnd, kMigrateBfsDepth);

  std::swap(misFiltration_[pick], misFiltration_[finalEnd]);
  misBorder_[count - 1]++;
  return true;
}

size_t GRIP::CreateMISFiltration() {
  size_t nvertices = Structure().VertexCapacity();
  BitSet curr(nvertices);

  MakeFirstMISPartition(curr);

  size_t i = 2;
  while (IterMISFiltration(i, curr))
    i++;

  size_t k = 0;
  for (size_t vtx : curr)
    misFiltration_[k++] = vtx;

  while (MisBorderAt(i) < Dim() + 1) {
    if (!MigrateOneToFinalLayer(i + 1))
      break;
  }

  return i + 1;
}

size_t GRIP::DebugBuildFiltrationPreMigrate() {
  size_t nvertices = Structure().VertexCapacity();
  BitSet curr(nvertices);

  MakeFirstMISPartition(curr);

  size_t i = 2;
  while (IterMISFiltration(i, curr))
    i++;

  size_t k = 0;
  for (size_t vtx : curr)
    misFiltration_[k++] = vtx;

  return i;
}

// PLACEMENT: ---------------------------------------------------------------

// Places misFiltration_[begin, end) at the barycenter of their nearest
// visible neighbors. Safe to run concurrently over disjoint ranges: the draw
// mask and the positions of visible vertices are only read, and each
// iteration writes the position of a distinct hidden vertex.
void GRIP::PlaceVertexRange(size_t begin, size_t end) {
  search::KNearestScratch &scratch = KnnScratchForCaller();
  size_t placementK = PlacementK();
  const BitSet &visible = VisibleVertices();

  std::vector<search::FoundVertex> found(placementK);
  std::vector<double> pos(Dim());

  for (size_t i = begin; i < end; i++) {
    size_t target = misFiltration_[i];
    assert(!visible.Test(target));

    size_t count = search::KNearest(Structure(), found, placementK, target,
                                     &visible, scratch);
    assert(count > 0);

    Barycenter(std::span<const search::FoundVertex>(found.data(), count),
               pos.data());
    SetVPosition(target, pos.data());
  }
}

// DO NOT CALL FOR THE FIRST LAYER (misBorderAt(currLayer_ + 1) must exist).
void GRIP::PlaceLayerVertices() {
  size_t layer = currLayer_;
  size_t begin = MisBorderAt(layer + 1);
  size_t end = MisBorderAt(layer);
  size_t placeCount = end - begin;
  size_t placementK = PlacementK();
  const BitSet &visible = VisibleVertices();
  size_t visibleCount = visible.Popcount();
  bool batchPlaced = false;

  if (placeCount > 0 && visibleCount > 0 && visibleCount < placeCount &&
      search::KNearestPreferBatch(Structure(), visibleCount, placeCount)) {
    std::vector<search::KnnBatchTarget> targets(placeCount);
    std::vector<search::FoundVertex> found(placeCount * placementK);
    for (size_t i = 0; i < placeCount; i++) {
      targets[i].vertex = misFiltration_[begin + i];
      targets[i].out = std::span<search::FoundVertex>(
          found.data() + i * placementK, placementK);
      targets[i].count = 0;
    }
    if (search::KNearestFromVisibleBatch(Structure(), &visible, placementK,
                                          targets,
                                          KnnScratchForCaller()) ==
        search::BatchResult::Applied) {
      batchPlaced = true;
      std::vector<double> pos(Dim());
      for (size_t i = 0; i < placeCount; i++) {
        assert(targets[i].count > 0);
        Barycenter(std::span<const search::FoundVertex>(targets[i].out.data(),
                                                          targets[i].count),
                   pos.data());
        SetVPosition(targets[i].vertex, pos.data());
      }
    }
  }

  if (!batchPlaced)
    RunForRange(begin, end, kParallelGrain,
                [this](size_t b, size_t e) { PlaceVertexRange(b, e); });

  for (size_t i = begin; i < end; i++)
    DrawMaskShowVertex(misFiltration_[i]);
}

// REFINEMENT: ----------------------------------------------------------------

void GRIP::UpdateLocalTemp(size_t v) {
  size_t dim = Dim();
  Decorators &dec = dec_[v];
  double nrm = VecNorm2(dim, dec.disp.data());
  double oldNrm = VecNorm2(dim, dec.oldDisp.data());

  if (std::fabs(nrm) < kNumericEpsilon || std::fabs(oldNrm) < kNumericEpsilon)
    return;

  double cosv =
      VecDot(dim, dec.disp.data(), dec.oldDisp.data()) / (nrm * oldNrm);

  constexpr double kHeatR = 0.15;
  constexpr double kHeatS = 3.0;
  if (cosv > 0 && dec.oldCos > 0)
    dec.heat *= (1 + cosv * kHeatR * kHeatS);
  else
    dec.heat *= (1 + cosv * kHeatR);

  dec.oldCos = cosv;
}

// DO NOT TOUCH THIS FUNCTION, VERY FRAGILE (carried over from the old C --
// still true here: the force balance was tuned empirically).
void GRIP::CalculateSpringForces(size_t v, size_t layer) {
  size_t dim = Dim();
  std::vector<double> f(dim, 0.0);
  Decorators &dec = dec_[v];

  if (layer > 0) {
    for (size_t i = 0; i < dec.knnCount; i++) {
      const search::FoundVertex &other = dec.knn[i];
      VecAccKKForce(dim, GetVPosition(v), GetVPosition(other.v),
                    static_cast<double>(other.dist) * kEdgeLength, f.data());
    }
  } else {
    for (size_t other : Structure().Neighbors(v))
      VecAccGRIPFRRepForce(dim, GetVPosition(v), GetVPosition(other),
                            kEdgeLength, f.data());

    for (size_t i = 0; i < dec.knnCount; i++) {
      const search::FoundVertex &other = dec.knn[i];
      VecAccGRIPFRAttForce(dim, GetVPosition(v), GetVPosition(other.v),
                            kEdgeLength, kFrScaleFactor, f.data());
    }
  }

  VecCopy(dim, f.data(), dec.disp.data());
}

void GRIP::ClearDecorators() {
  dispCalculated_.ClearAll();

  for (size_t i = 0; i < MisBorderAt(currLayer_); i++) {
    size_t curr = misFiltration_[i];
    Decorators &dec = dec_[curr];
    dec.heat = 0.0;
    dec.oldCos = 0.0;
    dec.knnCount = 0;
    std::fill(dec.disp.begin(), dec.disp.end(), 0.0);
    std::fill(dec.oldDisp.begin(), dec.oldDisp.end(), 0.0);
  }
}

void GRIP::UpdateKNNRange(size_t begin, size_t end) {
  search::KNearestScratch &scratch = KnnScratchForCaller();
  size_t refinementK = RefinementK();
  const BitSet &visible = VisibleVertices();

  for (size_t i = begin; i < end; i++) {
    size_t curr = misFiltration_[i];
    Decorators &dec = dec_[curr];
    size_t count = search::KNearest(
        Structure(), std::span<search::FoundVertex>(dec.knn.data(), dec.knn.size()),
        refinementK, curr, &visible, scratch);
    dec.knnCount = count;
  }
}

void GRIP::UpdateKNNs() {
  size_t end = MisBorderAt(currLayer_);
  size_t refinementK = RefinementK();
  const BitSet &visible = VisibleVertices();
  size_t visibleCount = visible.Popcount();
  bool batchUpdated = false;

  if (end > 0 && visibleCount > 0 && visibleCount < end &&
      search::KNearestPreferBatch(Structure(), visibleCount, end)) {
    std::vector<search::KnnBatchTarget> targets(end);
    for (size_t i = 0; i < end; i++) {
      size_t curr = misFiltration_[i];
      targets[i].vertex = curr;
      targets[i].out = std::span<search::FoundVertex>(dec_[curr].knn.data(),
                                                        dec_[curr].knn.size());
      targets[i].count = 0;
    }
    if (search::KNearestFromVisibleBatch(Structure(), &visible, refinementK,
                                          targets,
                                          KnnScratchForCaller()) ==
        search::BatchResult::Applied) {
      batchUpdated = true;
      for (size_t i = 0; i < end; i++) {
        size_t curr = misFiltration_[i];
        dec_[curr].knnCount = targets[i].count;
      }
    }
  }

  if (!batchUpdated)
    RunForRange(0, end, kParallelGrain,
                [this](size_t b, size_t e) { UpdateKNNRange(b, e); });
}

void GRIP::Begin() {
  layerCount_ = CreateMISFiltration();
  currLayer_ = layerCount_ - 1;

  size_t dim = Dim();
  std::vector<double> simplex(dim * (dim + 1));
  MakeRegularSimplex(dim, kEdgeLength * 1000.0, simplex.data());

  for (size_t j = 0; j < dim + 1; j++) {
    DrawMaskShowVertex(misFiltration_[j]);
    SetVPosition(misFiltration_[j], simplex.data() + j * dim);
  }

  ClearDecorators();
  UpdateKNNs();
  SyncDrawMask();
}

void GRIP::NextStage() {
  if (currLayer_ == 0)
    return;
  currLayer_--;
  currRound_ = 0;

  bool timing = std::getenv("GVIZ_GRIP_STAGE_TIMING") != nullptr;
  std::chrono::steady_clock::time_point t0;
  if (timing)
    t0 = std::chrono::steady_clock::now();

  PlaceLayerVertices();

  if (timing) {
    auto t1 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[grip-stage] placeLayerVertices: %.3fs\n",
                 std::chrono::duration<double>(t1 - t0).count());
    t0 = t1;
  }

  ClearDecorators();
  UpdateKNNs();

  if (timing) {
    auto t1 = std::chrono::steady_clock::now();
    std::fprintf(stderr, "[grip-stage] updateKNNs: %.3fs\n",
                 std::chrono::duration<double>(t1 - t0).count());
  }

  SyncDrawMask();
}

void GRIP::RefinementPass1Range(size_t begin, size_t end) {
  size_t dim = Dim();
  size_t layer = currLayer_;

  for (size_t i = begin; i < end; i++) {
    size_t curr = misFiltration_[i];
    Decorators &dec = dec_[curr];

    VecCopy(dim, dec.disp.data(), dec.oldDisp.data());

    CalculateSpringForces(curr, layer);

    if (!dispCalculated_.Test(curr)) {
      dispCalculated_.Set(curr);
      dec.heat = kEdgeLength / 6.0;
    } else {
      UpdateLocalTemp(curr);
    }

    dec.heat = std::fmin(dec.heat, kEdgeLength * 10.0);
    dec.heat = std::fmax(dec.heat, kEdgeLength * 1e-4);

    double nrm = VecNorm2(dim, dec.disp.data());
    dec.lastForceMag = nrm;
    if (nrm > kNumericEpsilon)
      VecScale(dim, dec.heat / nrm, dec.disp.data());
  }
}

// Rigid translations and rotations are zero modes of the layout energy:
// nothing in the spring forces resists them, and the adaptive heat rewards
// their coherent direction, so any residual net force (e.g. from asymmetric
// KNN lists) makes the whole embedding drift or spin indefinitely.
// Subtracting the mean displacement pins the barycenter without altering
// relative motion.
void GRIP::RemoveNetTranslation() {
  size_t dim = Dim();
  size_t count = MisBorderAt(currLayer_);
  if (count == 0)
    return;

  std::vector<double> mean(dim, 0.0);
  for (size_t i = 0; i < count; i++)
    VecAxpy(dim, 1.0, dec_[misFiltration_[i]].disp.data(), mean.data());
  VecScale(dim, 1.0 / static_cast<double>(count), mean.data());

  for (size_t i = 0; i < count; i++)
    VecAxpy(dim, -1.0, mean.data(), dec_[misFiltration_[i]].disp.data());
}

// Removes net rotation in the (a, b) coordinate plane using the same 2D
// formula as the z-axis rotation case in RemoveNetRotation.
void GRIP::RemoveNetRotationPlane(size_t a, size_t b, const double *com,
                                   size_t count) {
  double L = 0.0;
  double inertia = 0.0;

  for (size_t i = 0; i < count; i++) {
    size_t v = misFiltration_[i];
    double *p = GetVPosition(v);
    double *d = dec_[v].disp.data();
    double ra = p[a] - com[a];
    double rb = p[b] - com[b];
    L += ra * d[b] - rb * d[a];
    inertia += ra * ra + rb * rb;
  }

  if (inertia < kNumericEpsilon)
    return;

  double omega = L / inertia;
  for (size_t i = 0; i < count; i++) {
    size_t v = misFiltration_[i];
    double *p = GetVPosition(v);
    double *d = dec_[v].disp.data();
    double ra = p[a] - com[a];
    double rb = p[b] - com[b];
    d[a] += omega * rb;
    d[b] -= omega * ra;
  }
}

// Projects the best-fit rigid rotation about the barycenter out of the
// displacements: omega = I^-1 L with L the "angular momentum" of the disp
// field and I the inertia tensor of the active vertices. In 4D, all six
// coordinate-plane rotations are removed sequentially.
void GRIP::RemoveNetRotation() {
  size_t dim = Dim();
  size_t count = MisBorderAt(currLayer_);
  if (count < 3 || (dim != 2 && dim != 3 && dim != 4))
    return;

  double com[4] = {0, 0, 0, 0};
  for (size_t i = 0; i < count; i++) {
    double *p = GetVPosition(misFiltration_[i]);
    for (size_t d = 0; d < dim; d++)
      com[d] += p[d];
  }
  for (size_t d = 0; d < dim; d++)
    com[d] /= static_cast<double>(count);

  if (dim == 4) {
    RemoveNetRotationPlane(0, 1, com, count);
    RemoveNetRotationPlane(0, 2, com, count);
    RemoveNetRotationPlane(0, 3, com, count);
    RemoveNetRotationPlane(1, 2, com, count);
    RemoveNetRotationPlane(1, 3, com, count);
    RemoveNetRotationPlane(2, 3, com, count);
    return;
  }

  double com3[3] = {com[0], com[1], dim == 3 ? com[2] : 0.0};

  double L[3] = {0, 0, 0};
  double inertia[3][3] = {{0}};
  for (size_t i = 0; i < count; i++) {
    size_t v = misFiltration_[i];
    double *p = GetVPosition(v);
    double *d = dec_[v].disp.data();
    double r[3] = {p[0] - com3[0], p[1] - com3[1],
                   dim == 3 ? p[2] - com3[2] : 0.0};
    double dz = dim == 3 ? d[2] : 0.0;

    L[0] += r[1] * dz - r[2] * d[1];
    L[1] += r[2] * d[0] - r[0] * dz;
    L[2] += r[0] * d[1] - r[1] * d[0];

    double r2 = r[0] * r[0] + r[1] * r[1] + r[2] * r[2];
    for (size_t a = 0; a < 3; a++)
      for (size_t b = 0; b < 3; b++)
        inertia[a][b] += (a == b ? r2 : 0.0) - r[a] * r[b];
  }

  double omega[3] = {0, 0, 0};
  if (dim == 2) {
    if (std::fabs(inertia[2][2]) < kNumericEpsilon)
      return;
    omega[2] = L[2] / inertia[2][2];
  } else if (!Solve3x3(inertia, L, omega)) {
    return;
  }

  for (size_t i = 0; i < count; i++) {
    size_t v = misFiltration_[i];
    double *p = GetVPosition(v);
    double *d = dec_[v].disp.data();
    double r[3] = {p[0] - com3[0], p[1] - com3[1],
                   dim == 3 ? p[2] - com3[2] : 0.0};
    d[0] -= omega[1] * r[2] - omega[2] * r[1];
    d[1] -= omega[2] * r[0] - omega[0] * r[2];
    if (dim == 3)
      d[2] -= omega[0] * r[1] - omega[1] * r[0];
  }
}

void GRIP::RefineRound() {
  size_t layer = currLayer_;
  size_t dim = Dim();

  RunForRange(0, MisBorderAt(layer), kParallelGrain,
              [this](size_t b, size_t e) { RefinementPass1Range(b, e); });

  RemoveNetTranslation();
  RemoveNetRotation();

  double maxDisp = 0.0, sumDisp = 0.0, sumHeat = 0.0, sumForce = 0.0;
  size_t active = MisBorderAt(layer);
  for (size_t i = 0; i < active; i++) {
    size_t curr = misFiltration_[i];
    Decorators &dec = dec_[curr];
    double nrm = VecNorm2(dim, dec.disp.data());
    if (nrm > maxDisp)
      maxDisp = nrm;
    sumDisp += nrm;
    sumHeat += dec.heat;
    sumForce += dec.lastForceMag;

    AddVPosition(curr, dec.disp.data());
    assert(!std::isnan(GetVPosition(curr)[0]));
  }
  lastRoundStats_.maxDisplacement = maxDisp;
  lastRoundStats_.meanDisplacement = active ? sumDisp / active : 0.0;
  lastRoundStats_.meanForce = active ? sumForce / active : 0.0;

  if (statsEnabled_) {
    StatAppend("grip.heat", active ? sumHeat / active : 0.0);
    StatAppend("grip.meanDisp", lastRoundStats_.meanDisplacement);
    StatAppend("grip.maxDisp", maxDisp);
    StatAppend("grip.meanForce", lastRoundStats_.meanForce);
  }

  currRound_++;
}

void GRIP::Embed() {
  Begin();
  if (layerCount_ == 0)
    return; // Defensive: unreachable given the constructor's
            // InsufficientVerticesError precondition.

  double epsilon = kEdgeLength * kConvergenceFactor;
  for (;;) {
    for (size_t round = 0; round < kMaxRoundsPerLayer; round++) {
      RefineRound();
      if (lastRoundStats_.maxDisplacement < epsilon)
        break;
    }
    if (currLayer_ == 0)
      break;
    NextStage();
  }
}

} // namespace gviz::layout
