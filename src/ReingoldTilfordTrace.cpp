// Clarity-first, event-recording reimplementation of the same algorithm
// ReingoldTilford.cpp ports from the old C gvizEmbeddedTree. See
// ReingoldTilfordTrace.hpp's class comment for why this exists as its own
// class instead of instrumenting ReingoldTilford directly, and for the one
// deliberate divergence (real depth tracking) from that port.
//
// Every private method here is the direct structural analog of the
// same-named private method in ReingoldTilford.cpp -- read that file's
// comments for the algorithm itself; the comments here focus on where and
// why an Event gets recorded.

#include "ReingoldTilfordTrace.hpp"

#include "Subgraph.hpp"
#include "Tree.hpp"
#include "Vec.hpp"

#include <cassert>
#include <cmath>

namespace gviz::layout {

namespace {
// Must stay numerically in sync with ReingoldTilford.cpp's kXSeparation/
// kYSeparation: this class's whole "teaches the real algorithm" claim rests
// on producing the same final positions ReingoldTilford does (see this
// class's header comment for why that holds regardless of the depth-
// tracking fix), and ReingoldTilfordTraceTests.cpp checks that by direct
// position comparison. Not shared via a common header because
// ReingoldTilford.cpp's constants are a private implementation detail of
// the production class, not something this teaching-only class should pull
// production internals in for -- a mismatch here would simply show up as a
// failing test.
constexpr float kXSeparation = 500.0f;
constexpr float kYSeparation = 1000.0f;
// Must also stay numerically in sync with ReingoldTilford.cpp's identically-
// named/valued constants (same "not shared via a common header" rationale
// as kXSeparation/kYSeparation above -- a mismatch shows up as a failing
// ReingoldTilfordTraceTests parity test, not a silent divergence).
constexpr float kMinSeparation = 1.0f;
constexpr float kSeparationEpsilon = 1e-4f;
} // namespace

ReingoldTilfordTrace::ReingoldTilfordTrace(const Graph &graph, size_t root)
    : EmbeddedGraph(Subgraph::CreateFull(graph), 2), graph_(graph) {
  if (gviz::search::IsTree(graph_, &parents_) !=
      gviz::search::TreeCheckResult::IsTree)
    throw NotATreeError();

  if (parents_[root] != -1)
    throw NotATreeError("root is not this graph's actual root vertex");

  dec_.resize(graph_.Size());
}

float ReingoldTilfordTrace::IterateContourRightward(size_t &contour) {
  if (IsThreaded(contour)) {
    float out = dec_[contour].offsets[0];
    contour = dec_[contour].threadTo;
    return out;
  }

  size_t degree = graph_.Degree(contour);
  float out = dec_[contour].offsets[degree - 1];
  contour = graph_.Neighbor(contour, degree - 1);
  return out;
}

float ReingoldTilfordTrace::IterateContourLeftward(size_t &contour) {
  if (IsThreaded(contour)) {
    float out = dec_[contour].offsets[0];
    contour = dec_[contour].threadTo;
    return out;
  }

  float out = dec_[contour].offsets[0];
  contour = graph_.Neighbor(contour, 0);
  return out;
}

size_t ReingoldTilfordTrace::GetAncestor(size_t root, size_t i) const {
  size_t ancestor = dec_[i].ancestor;
  size_t pos;
  if (graph_.NeighborPosition(root, ancestor, pos))
    return pos;
  graph_.NeighborPosition(root, defaultAncestor_, pos);
  return pos;
}

void ReingoldTilfordTrace::SetAncestorAlongRightContour(size_t i) {
  size_t curr = i;
  dec_[curr].ancestor = i;
  while (!gviz::search::IsLeaf(graph_, curr)) {
    IterateContourRightward(curr);
    dec_[curr].ancestor = i;
  }
}

void ReingoldTilfordTrace::InitializeRTLeaf(size_t i, size_t level) {
  dec_[i].rMost = i;
  dec_[i].lMost = i;
  dec_[i].ancestor = i;
  dec_[i].depth = level;
  dec_[i].threadTo = kNoThread;
  if (level > height_)
    height_ = level;

  dec_[i].offsets.assign(1, 0.0f);
}

void ReingoldTilfordTrace::InitializeRTSubtreeRoot(size_t root, size_t level) {
  size_t degree = graph_.Degree(root);

  if (degree == 0) { // Base case
    InitializeRTLeaf(root, level);
    Event e;
    e.kind = EventKind::SubtreeIsolated;
    e.root = root;
    e.vertexA = root;
    e.level = level;
    e.offsets = dec_[root].offsets;
    events_.push_back(std::move(e));
    return;
  }

  dec_[root].offsets.assign(degree, 0.0f);

  if (degree == 1)
    SetAncestorAlongRightContour(root);

  size_t lMostSubtree = graph_.Neighbor(root, 0);
  defaultAncestor_ = lMostSubtree;
  dec_[root].lMost = dec_[lMostSubtree].lMost;
  dec_[root].rMost = dec_[lMostSubtree].rMost;
  dec_[root].depth = level;

  Event e;
  e.kind = EventKind::SubtreeIsolated;
  e.root = root;
  e.vertexA = root;
  e.level = level;
  e.offsets = dec_[root].offsets;
  events_.push_back(std::move(e));
}

void ReingoldTilfordTrace::CreateThreads(size_t root, size_t i, size_t &lrContour,
                                          size_t &rlContour,
                                          const SubtreePairExtremes &extremes,
                                          const SeparationResult &res) {
  SeparationResult r = res;

  // left subtree (blob of subtrees) is deeper. thread rr.
  if (!gviz::search::IsLeaf(graph_, lrContour) &&
      gviz::search::IsLeaf(graph_, rlContour)) {
    r.lOffset += IterateContourRightward(lrContour);
    dec_[extremes.rr].threadTo = lrContour;
    dec_[extremes.rr].offsets[0] =
        dec_[extremes.rr].offsets[0] - r.rmostSeparation + r.lOffset;

    Event e;
    e.kind = EventKind::ThreadCreated;
    e.root = root;
    e.childIndex = i;
    e.vertexA = extremes.rr;
    e.vertexB = lrContour;
    e.rightIsSource = false;
    events_.push_back(std::move(e));
  }
  // right subtree is deeper. thread ll.
  else if (gviz::search::IsLeaf(graph_, lrContour) &&
           !gviz::search::IsLeaf(graph_, rlContour)) {
    r.rOffset += IterateContourLeftward(rlContour);
    dec_[extremes.ll].threadTo = rlContour;
    dec_[extremes.ll].offsets[0] = dec_[extremes.ll].offsets[0] +
                                    r.totalNewSeparation / 2.0f +
                                    dec_[root].offsets[i] + r.rOffset;

    Event e;
    e.kind = EventKind::ThreadCreated;
    e.root = root;
    e.childIndex = i;
    e.vertexA = extremes.ll;
    e.vertexB = rlContour;
    e.rightIsSource = true;
    events_.push_back(std::move(e));
  }
}

void ReingoldTilfordTrace::UpdateExtremes(size_t root,
                                           const SubtreePairExtremes &extremes,
                                           const SeparationResult &res) {
  if (dec_[extremes.rl].depth > dec_[extremes.ll].depth) {
    dec_[root].lMost = extremes.rl;
    dec_[extremes.rl].offsets[0] -= res.totalNewSeparation / 2.0f;
  } else {
    dec_[root].lMost = extremes.ll;
    dec_[extremes.ll].offsets[0] += res.totalNewSeparation / 2.0f;
  }
  if (dec_[extremes.lr].depth > dec_[extremes.rr].depth) {
    dec_[root].rMost = extremes.lr;
    dec_[extremes.lr].offsets[0] += res.totalNewSeparation / 2.0f;
  } else {
    dec_[root].rMost = extremes.rr;
    dec_[extremes.rr].offsets[0] -= res.totalNewSeparation / 2.0f;
  }

  Event e;
  e.kind = EventKind::ExtremesUpdated;
  e.root = root;
  e.vertexA = dec_[root].lMost;
  e.vertexB = dec_[root].rMost;
  events_.push_back(std::move(e));
}

ReingoldTilfordTrace::SeparationResult
ReingoldTilfordTrace::SeparateAlongContours(size_t &lrContour, size_t &rlContour) {
  assert(parents_[lrContour] == parents_[rlContour]);

  float lOffset = 0, rOffset = 0, lstep, rstep, currsep = kMinSeparation;
  size_t root = static_cast<size_t>(parents_[lrContour]);

  size_t rightSubtree = rlContour;
  size_t rightSubtreeIndex;
  graph_.NeighborPosition(root, rlContour, rightSubtreeIndex);
  std::vector<float> newSeparations(rightSubtreeIndex, 0.0f);

  Event begin;
  begin.kind = EventKind::ContourCompareBegin;
  begin.root = root;
  begin.childIndex = rightSubtreeIndex;
  begin.vertexA = lrContour;
  begin.vertexB = rlContour;
  events_.push_back(std::move(begin));

  // The root pair itself -- lrContour/rlContour exactly as
  // ContourCompareBegin just displayed them, before any stepping -- gets
  // its own measure/correct cycle too, same as every deeper level below:
  // no level is ever just assumed correct. They start out fully
  // overlapped (measuredSeparation 0, matching how ContourCompareBegin's
  // consumer is expected to have staged them), so this always corrects by
  // exactly kMinSeparation. Unlike GetAncestor's role for every subsequent,
  // deeper level, this placement is fixed, not computed: the baseline unit
  // is always owed in the gap directly between the already-merged blob and
  // the new child being folded in (slot rightSubtreeIndex - 1), never an
  // earlier gap -- there is no earlier collision to attribute it to yet,
  // since nothing has been measured before this. Mathematically identical
  // to the old code's silent `newSeparations[rightSubtreeIndex - 1] =
  // kMinSeparation` seed (newSeparations starts all-zero, so seeding a
  // slot directly and zero-init-then-adding-kMinSeparation-to-it are the
  // same value) -- only the presentation changed, from an invisible seed
  // to a genuine, visible measure-then-correct step.
  {
    Event measure;
    measure.kind = EventKind::ContourMeasure;
    measure.root = root;
    measure.childIndex = rightSubtreeIndex;
    measure.vertexA = lrContour;
    measure.vertexB = rlContour;
    measure.measuredSeparation = 0.0f;
    events_.push_back(std::move(measure));

    Event correct;
    correct.kind = EventKind::ContourCorrect;
    correct.root = root;
    correct.childIndex = rightSubtreeIndex;
    correct.vertexA = lrContour;
    correct.vertexB = rlContour;
    correct.correctionFired = true;
    correct.correctionAmount = kMinSeparation;
    correct.ancestor = rightSubtreeIndex - 1;
    newSeparations[rightSubtreeIndex - 1] += kMinSeparation;
    events_.push_back(std::move(correct));
  }

  while (!gviz::search::IsLeaf(graph_, lrContour) &&
         !gviz::search::IsLeaf(graph_, rlContour)) {
    rstep = IterateContourLeftward(rlContour);
    lstep = IterateContourRightward(lrContour);

    rOffset += rstep;
    lOffset += lstep;

    // currsep, right after this step, is exactly this level's contour gap
    // given every correction applied at shallower levels so far -- measure
    // it fresh every iteration, no bundling across iterations.
    currsep += rstep;
    currsep -= lstep;

    // lrContour/rlContour now hold the *post-step* pair -- the one whose
    // relative position currsep was actually just computed against.
    // Report that pair, not the pre-step one: the measurement (and any
    // correction) below belongs to this newly-revealed level, not the
    // level the walk was at before this iteration ran. (The pre-step pair
    // isn't lost -- it's exactly what the previous event in this loop, or
    // ContourCompareBegin for the first iteration, already reported.)
    Event measure;
    measure.kind = EventKind::ContourMeasure;
    measure.root = root;
    measure.childIndex = rightSubtreeIndex;
    measure.vertexA = lrContour;
    measure.vertexB = rlContour;
    measure.leftContourStep = lstep;
    measure.rightContourStep = rstep;
    measure.measuredSeparation = currsep;
    events_.push_back(std::move(measure));

    // Always a matching ContourCorrect right after -- every iteration
    // measures and corrects independently, so a level with plenty of
    // slack (currsep already >= kMinSeparation) still gets its own
    // "nothing to do" event rather than being silently skipped, and a
    // level with a shortfall never has that shortfall deferred or bundled
    // into a later level's correction.
    Event correct;
    correct.kind = EventKind::ContourCorrect;
    correct.root = root;
    correct.childIndex = rightSubtreeIndex;
    correct.vertexA = lrContour;
    correct.vertexB = rlContour;

    // Only ever correct a genuine shortfall, and correct it fully and
    // immediately to exactly kMinSeparation -- never more. When currsep is
    // already at or above kMinSeparation there is real slack from this
    // level, which must carry forward uncorrected (not reset) for later,
    // deeper levels to draw on -- resetting it here would double-correct a
    // shortfall that slack already covers (see this class's header comment
    // on why summing independent per-level shortfalls this way still
    // equals the true total requirement).
    if (currsep < kMinSeparation - kSeparationEpsilon) {
      size_t ancestor = GetAncestor(root, lrContour);
      size_t n = rightSubtreeIndex - ancestor;
      float correction = (kMinSeparation - currsep) / static_cast<float>(n);
      newSeparations[ancestor] += correction;
      currsep = kMinSeparation;

      correct.correctionFired = true;
      correct.correctionAmount = correction;
      correct.ancestor = ancestor;
    }

    events_.push_back(std::move(correct));
  }

  float acc = 0.0f, totalNewSeparation = 0.0f;
  for (size_t i = 0; i < rightSubtreeIndex; i++) {
    acc += newSeparations[i];
    newSeparations[i] = acc;
    totalNewSeparation += acc;
  }

  if (rightSubtreeIndex > 1) {
    const std::vector<float> &offsets = dec_[root].offsets;
    for (size_t i = 0; i < rightSubtreeIndex - 1; i++)
      newSeparations[i] += std::fabs(offsets[i + 1] - offsets[i]);
  }

  std::vector<float> &offsets = dec_[root].offsets;
  float total = 0.0f;
  for (size_t i = 0; i < rightSubtreeIndex; i++)
    total += newSeparations[i];
  offsets[0] = -total / 2.0f;
  for (size_t i = 1; i <= rightSubtreeIndex; i++)
    offsets[i] = offsets[i - 1] + newSeparations[i - 1];

  (void)rightSubtree;
  return SeparationResult{lOffset, rOffset, totalNewSeparation,
                           newSeparations[rightSubtreeIndex - 1]};
}

void ReingoldTilfordTrace::CombineSubtreeLeft(size_t root, size_t i) {
  if (i == 0)
    return;

  size_t lrContour = graph_.Neighbor(root, i - 1);
  size_t rlContour = graph_.Neighbor(root, i);
  size_t rightSubtree = rlContour;
  SubtreePairExtremes extremes{
      dec_[root].lMost,
      dec_[root].rMost,
      dec_[rlContour].lMost,
      dec_[rlContour].rMost,
  };

  // ContourCompareBegin is recorded inside SeparateAlongContours, since
  // that's where the (possibly index-derived) rightSubtreeIndex is known.
  SeparationResult res = SeparateAlongContours(lrContour, rlContour);

  if (!gviz::search::IsLeaf(graph_, rlContour) ||
      gviz::search::IsLeaf(graph_, lrContour)) {
    defaultAncestor_ = rightSubtree;
  } else {
    SetAncestorAlongRightContour(rightSubtree);
  }

  UpdateExtremes(root, extremes, res);
  CreateThreads(root, i, lrContour, rlContour, extremes, res);

  Event merged;
  merged.kind = EventKind::SubtreeMerged;
  merged.root = root;
  merged.childIndex = i;
  merged.offsets = dec_[root].offsets;
  events_.push_back(std::move(merged));
}

void ReingoldTilfordTrace::CalculateOffsets(size_t root, size_t level) {
  size_t degree = graph_.Degree(root);

  // Divide. Unlike ReingoldTilford::CalculateOffsets, level genuinely
  // increases here -- see this class's header comment.
  for (size_t i = 0; i < degree; i++)
    CalculateOffsets(graph_.Neighbor(root, i), level + 1);

  // Initialization. Includes base case (leaf node); records
  // EventKind::SubtreeIsolated.
  InitializeRTSubtreeRoot(root, level);

  // Conquer.
  for (size_t i = 0; i < degree; i++)
    CombineSubtreeLeft(root, i);
}

void ReingoldTilfordTrace::Embed(size_t root, const double *position) {
  size_t degree = graph_.Degree(root);
  SetVPosition(root, position);

  Event e;
  e.kind = EventKind::EmbedStep;
  e.root = root;
  e.position[0] = position[0];
  e.position[1] = position[1];
  events_.push_back(std::move(e));

  for (size_t i = 0; i < degree; i++) {
    double newPos[2] = {dec_[root].offsets[i] * kXSeparation, kYSeparation};
    VecAxpy(2, 1.0, position, newPos);

    Embed(graph_.Neighbor(root, i), newPos);
  }
}

} // namespace gviz::layout
