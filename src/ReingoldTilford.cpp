#include "ReingoldTilford.hpp"

#include "Tree.hpp"
#include "Vec.hpp"

#include <cassert>
#include <cmath>

namespace gviz::layout {

namespace {
constexpr float kXSeparation = 500.0f;
constexpr float kYSeparation = 1000.0f;
// SeparateAlongContours' minimum-separation target between two contour
// vertices, in offset units.
constexpr float kMinSeparation = 1.0f;
// Floating-point slack below kMinSeparation treated as "no shortfall,"
// to absorb float rounding noise from repeated +=/-= accumulation.
constexpr float kSeparationEpsilon = 1e-4f;
} // namespace

ReingoldTilford::ReingoldTilford(const Graph &graph, size_t root)
    : EmbeddedGraph(graph.Size(), 2), graph_(graph) {
  if (gviz::search::IsTree(graph_, &parents_) !=
      gviz::search::TreeCheckResult::IsTree)
    throw NotATreeError();

  // parents_[root] == -1 iff root is the vertex IsTree actually found to be
  // rootless -- i.e. the tree's real root.
  if (parents_[root] != -1)
    throw NotATreeError("root is not this graph's actual root vertex");

  dec_.resize(graph_.Size());
}

float ReingoldTilford::IterateContourRightward(size_t &contour) {
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

float ReingoldTilford::IterateContourLeftward(size_t &contour) {
  if (IsThreaded(contour)) {
    float out = dec_[contour].offsets[0];
    contour = dec_[contour].threadTo;
    return out;
  }

  float out = dec_[contour].offsets[0];
  contour = graph_.Neighbor(contour, 0);
  return out;
}

size_t ReingoldTilford::GetAncestor(size_t root, size_t i) const {
  size_t ancestor = dec_[i].ancestor;
  size_t pos;
  if (graph_.NeighborPosition(root, ancestor, pos))
    return pos;
  graph_.NeighborPosition(root, defaultAncestor_, pos);
  return pos;
}

void ReingoldTilford::SetAncestorAlongRightContour(size_t i) {
  size_t curr = i;
  dec_[curr].ancestor = i;
  while (!ContourAtEnd(curr)) {
    IterateContourRightward(curr);
    dec_[curr].ancestor = i;
  }
}

void ReingoldTilford::InitializeRTLeaf(size_t i, size_t level) {
  dec_[i].rMost = i;
  dec_[i].lMost = i;
  dec_[i].ancestor = i;
  dec_[i].depth = level;
  dec_[i].threadTo = kNoThread;
  if (level > height_)
    height_ = level;

  // Still needs an offsets array to track lMost/rMost's own displacement.
  dec_[i].offsets.assign(1, 0.0f);
}

void ReingoldTilford::InitializeRTSubtreeRoot(size_t root, size_t level) {
  size_t degree = graph_.Degree(root);

  if (degree == 0) { // Base case
    InitializeRTLeaf(root, level);
    return;
  }

  // Must happen before the single-child contour walk below:
  // SetAncestorAlongRightContour reads dec_[root].offsets, and the
  // zero-initialized array gives the correct 0 displacement for an
  // unmerged child.
  dec_[root].offsets.assign(degree, 0.0f);

  if (degree == 1) // Special case
    SetAncestorAlongRightContour(root);

  // Conquering initialization
  size_t lMostSubtree = graph_.Neighbor(root, 0);
  defaultAncestor_ = lMostSubtree;
  dec_[root].lMost = dec_[lMostSubtree].lMost;
  dec_[root].rMost = dec_[lMostSubtree].rMost;
  dec_[root].depth = level;
}

void ReingoldTilford::CreateThreads(size_t root, size_t i, size_t &lrContour,
                                     size_t &rlContour,
                                     const SubtreePairExtremes &extremes,
                                     const SeparationResult &res) {
  // IterateContourRightward/Leftward mutate the accumulated offsets, so
  // res needs a local mutable copy.
  SeparationResult r = res;

  // left subtree (blob of subtrees) is deeper. thread rr.
  if (!ContourAtEnd(lrContour) && ContourAtEnd(rlContour)) {
    r.lOffset += IterateContourRightward(lrContour);
    dec_[extremes.rr].threadTo = lrContour;
    // extremes.rr's offsets[0] was computed in the frame of the blob as it
    // stood *before* this merge folded the new right subtree in --
    // SeparateAlongContours' r.rmostSeparation is exactly the separation
    // this merge just added at that gap, so it must be subtracted back out
    // before adding r.lOffset (the walk distance to the thread's target),
    // or the thread's stored delta double-counts this merge's own
    // separation on top of the one UpdateExtremes/later merges already
    // apply through dec_[root].offsets.
    dec_[extremes.rr].offsets[0] =
        dec_[extremes.rr].offsets[0] - r.rmostSeparation + r.lOffset;
  }
  // right subtree is deeper. thread ll.
  else if (ContourAtEnd(lrContour) && !ContourAtEnd(rlContour)) {
    r.rOffset += IterateContourLeftward(rlContour);
    dec_[extremes.ll].threadTo = rlContour;
    // Mirror of the "thread rr" branch: extremes.ll's offsets[0] is still
    // in the new right subtree's own local frame (relative to child i's
    // own root), not root's frame -- it needs r.totalNewSeparation/2 (the
    // recentering UpdateExtremes applies to this same vertex) plus
    // dec_[root].offsets[i] (child i's own offset from root, just written
    // by SeparateAlongContours) to land in root's frame before adding
    // r.rOffset, the walk distance to the thread's target.
    dec_[extremes.ll].offsets[0] = dec_[extremes.ll].offsets[0] +
                                    r.totalNewSeparation / 2.0f +
                                    dec_[root].offsets[i] + r.rOffset;
  }
}

void ReingoldTilford::UpdateExtremes(size_t root,
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
}

ReingoldTilford::SeparationResult
ReingoldTilford::SeparateAlongContours(size_t &lrContour, size_t &rlContour) {
  assert(parents_[lrContour] == parents_[rlContour]);

  float lOffset = 0, rOffset = 0, lstep, rstep, currsep = kMinSeparation;
  size_t root = static_cast<size_t>(parents_[lrContour]);

  // newSeparations[i] = x means all separations with index >= i will gain
  // x units of separation, to merge the right subtree in.
  size_t rightSubtree = rlContour;
  size_t rightSubtreeIndex;
  graph_.NeighborPosition(root, rlContour, rightSubtreeIndex);
  std::vector<float> newSeparations(rightSubtreeIndex, 0.0f);
  newSeparations[rightSubtreeIndex - 1] = kMinSeparation;

  while (!ContourAtEnd(lrContour) && !ContourAtEnd(rlContour)) {
    rstep = IterateContourLeftward(rlContour);
    lstep = IterateContourRightward(lrContour);

    rOffset += rstep;
    lOffset += lstep;

    currsep += rstep;
    currsep -= lstep;

    if (currsep < kMinSeparation - kSeparationEpsilon) {
      size_t ancestor = GetAncestor(root, lrContour);
      size_t n = rightSubtreeIndex - ancestor; // subtrees between the colliding vertices

      newSeparations[ancestor] += (kMinSeparation - currsep) / static_cast<float>(n);
      currsep = kMinSeparation;
    }
  }

  // Turn per-gap separations into a running sum, so newSeparations[i]
  // becomes the cumulative separation to apply from index i onward.
  float acc = 0.0f, totalNewSeparation = 0.0f;
  for (size_t i = 0; i < rightSubtreeIndex; i++) {
    acc += newSeparations[i];
    newSeparations[i] = acc;
    totalNewSeparation += acc;
  }

  // Apply new separation to state
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

  return SeparationResult{lOffset, rOffset, totalNewSeparation,
                           newSeparations[rightSubtreeIndex - 1]};
}

void ReingoldTilford::CombineSubtreeLeft(size_t root, size_t i) {
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

  SeparationResult res = SeparateAlongContours(lrContour, rlContour);

  // If the right subtree was deeper or as deep, it becomes the new default
  // ancestor; otherwise it becomes the ancestor along all rr vertices.
  if (!ContourAtEnd(rlContour) || ContourAtEnd(lrContour)) {
    defaultAncestor_ = rightSubtree;
  } else {
    SetAncestorAlongRightContour(rightSubtree);
  }

  UpdateExtremes(root, extremes, res);
  CreateThreads(root, i, lrContour, rlContour, extremes, res);
}

void ReingoldTilford::CalculateOffsets(size_t root, size_t level) {
  size_t degree = graph_.Degree(root);

  for (size_t i = 0; i < degree; i++)
    CalculateOffsets(graph_.Neighbor(root, i), level + 1);

  InitializeRTSubtreeRoot(root, level); // includes the leaf base case

  for (size_t i = 0; i < degree; i++)
    CombineSubtreeLeft(root, i);
}

void ReingoldTilford::Embed(size_t root, const double *position) {
  size_t degree = graph_.Degree(root);
  SetVPosition(root, position);
  for (size_t i = 0; i < degree; i++) {
    double newPos[2] = {dec_[root].offsets[i] * kXSeparation, kYSeparation};
    VecAxpy(2, 1.0, position, newPos);

    Embed(graph_.Neighbor(root, i), newPos);
  }
}

} // namespace gviz::layout
