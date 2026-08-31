// Every private method here is the direct structural analog of the
// same-named private method in ReingoldTilford.cpp; the comments here
// focus on where and why an Event gets recorded.

#include "ReingoldTilfordTrace.hpp"

#include "Tree.hpp"
#include "Vec.hpp"

#include <cassert>
#include <cmath>

namespace gviz::layout {

namespace {
// Must stay numerically in sync with ReingoldTilford.cpp's identically
// named/valued constants -- a mismatch shows up as a failing
// ReingoldTilfordTraceTests parity test.
constexpr float kXSeparation = 500.0f;
constexpr float kYSeparation = 1000.0f;
constexpr float kMinSeparation = 1.0f;
constexpr float kSeparationEpsilon = 1e-4f;
} // namespace

ReingoldTilfordTrace::ReingoldTilfordTrace(const Graph &graph, size_t root)
    : EmbeddedGraph(graph.Size(), 2), graph_(graph) {
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
    // Kept in lockstep with ReingoldTilford::CreateThreads's identical
    // formula (see its comment): must subtract r.rmostSeparation (this
    // merge's own contribution to extremes.rr's pre-merge frame) before
    // adding r.lOffset.
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
    // Kept in lockstep with ReingoldTilford::CreateThreads's identical
    // formula (see its comment): must add r.totalNewSeparation/2 +
    // dec_[root].offsets[i] to bring extremes.ll's still-local-frame
    // offsets[0] into root's frame before adding r.rOffset.
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

  // The root pair itself gets its own measure/correct cycle too, same as
  // every deeper level below. They start out fully overlapped
  // (measuredSeparation 0), so this always corrects by exactly
  // kMinSeparation, attributed to the fixed gap slot rightSubtreeIndex - 1
  // (there's no earlier collision to attribute it to yet).
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

    currsep += rstep;
    currsep -= lstep;

    // lrContour/rlContour now hold the post-step pair, the one currsep was
    // actually just computed against -- report that pair, not the
    // pre-step one already reported by the previous event.
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

    // A matching ContourCorrect always follows, even when there's nothing
    // to do: a level with slack (currsep already >= kMinSeparation)
    // carries that slack forward uncorrected for a deeper level to draw
    // on, rather than resetting it (which would double-correct).
    Event correct;
    correct.kind = EventKind::ContourCorrect;
    correct.root = root;
    correct.childIndex = rightSubtreeIndex;
    correct.vertexA = lrContour;
    correct.vertexB = rlContour;

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
