// Port of src/embedders/gvizEmbeddedTree.c. Function-by-function mapping to
// the old free functions (all took `gvizEmbeddedTree *state` explicitly;
// here they're private members reading `this` implicitly):
//
//   rtGraph(state)                    -> graph_ (member, see header)
//   rtIsThreaded                      -> IsThreaded
//   iterateContourRightward/Leftward  -> IterateContourRightward/Leftward
//   getAncestor                       -> GetAncestor
//   seperationsToOffsets/             -> inlined into SeparateAlongContours
//     offsetsToSeperations               (see its comment: both were
//                                         partly-dead memsets over ranges
//                                         the following writes fully
//                                         overwrite anyway)
//   setAncestorAlongRightContour      -> SetAncestorAlongRightContour
//   initializeRTLeaf/SubtreeRoot      -> InitializeRTLeaf/SubtreeRoot
//   createThreads                     -> CreateThreads
//   updateExtremes                    -> UpdateExtremes
//   separateAlongContours             -> SeparateAlongContours
//   combineSubtreeLeft                -> CombineSubtreeLeft
//   gvizEmbeddedTreeCalculateOffsets  -> CalculateOffsets
//   gvizEmbeddedTreeRTInit            -> constructor
//   gvizEmbeddedTreeRTRelease         -> ~ReingoldTilford (defaulted, RAII)
//   gvizEmbeddedTreeEmbed             -> Embed

#include "ReingoldTilford.hpp"

#include "Subgraph.hpp"
#include "Tree.hpp"
#include "Vec.hpp"

#include <cassert>
#include <cmath>

namespace gviz::layout {

namespace {
constexpr float kXSeparation = 500.0f;
constexpr float kYSeparation = 1000.0f;
// SeparateAlongContours' minimum-separation target between two contour
// vertices, in offset units. A gap of exactly kMinSeparation never needs
// correcting; anything less does, by exactly the shortfall -- no more.
constexpr float kMinSeparation = 1.0f;
// Floating-point slack below kMinSeparation treated as "no shortfall,"
// purely to absorb float rounding noise from repeated +=/-= accumulation
// -- NOT a re-introduction of the old 0.1f slop threshold. Unlike that
// threshold, this epsilon never lets a real shortfall silently carry over
// into a later iteration: every iteration's own currsep is checked and, if
// it's short by more than this, corrected and reset to exactly
// kMinSeparation right then, so no iteration's requirement is ever
// deferred or bundled with another's.
constexpr float kSeparationEpsilon = 1e-4f;
} // namespace

ReingoldTilford::ReingoldTilford(const Graph &graph, size_t root)
    : EmbeddedGraph(Subgraph::CreateFull(graph), 2), graph_(graph) {
  if (gviz::search::IsTree(graph_, &parents_) !=
      gviz::search::TreeCheckResult::IsTree)
    throw NotATreeError();

  // parents_[root] == -1 iff root is the vertex IsTree actually found to be
  // rootless -- i.e. the tree's real root. See the header's class comment:
  // the old C RTInit never made this check (a wrong root silently laid out
  // only the subtree beneath it), closed here since it costs nothing beyond
  // an array read IsTree already paid for.
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
  // res is read-only here, but IterateContourRightward/Leftward mutate the
  // accumulated offsets below; a local mutable copy stands in for the old
  // C in/out `SeparationResult *res` parameter.
  SeparationResult r = res;

  // left subtree (blob of subtrees) is deeper. thread rr.
  if (!ContourAtEnd(lrContour) && ContourAtEnd(rlContour)) {
    r.lOffset += IterateContourRightward(lrContour);
    dec_[extremes.rr].threadTo = lrContour;
    dec_[extremes.rr].offsets[0] =
        dec_[extremes.rr].offsets[0] - r.rmostSeparation + r.lOffset;

  }
  // right subtree is deeper. thread ll.
  else if (ContourAtEnd(lrContour) && !ContourAtEnd(rlContour)) {
    r.rOffset += IterateContourLeftward(rlContour);
    dec_[extremes.ll].threadTo = rlContour;
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

  // tracks how much separation needs to be added to merge the right
  // subtree. newSeparations[i] = x means all separations with index >= i
  // will gain x units of separation.
  size_t rightSubtree = rlContour;
  size_t rightSubtreeIndex;
  graph_.NeighborPosition(root, rlContour, rightSubtreeIndex);
  std::vector<float> newSeparations(rightSubtreeIndex, 0.0f);
  newSeparations[rightSubtreeIndex - 1] = kMinSeparation;

  while (!ContourAtEnd(lrContour) && !ContourAtEnd(rlContour)) {

    // Step one level deeper along each contour, storing x-displacement.
    rstep = IterateContourLeftward(rlContour);
    lstep = IterateContourRightward(lrContour);

    // update total offset
    rOffset += rstep;
    lOffset += lstep;

    // currsep, right after this step, is exactly this level's contour gap
    // given every correction applied at shallower levels so far -- measure
    // it fresh every iteration, no bundling.
    currsep += rstep;
    currsep -= lstep;

    // Only ever correct a genuine shortfall (currsep below kMinSeparation),
    // and correct it fully and immediately -- not a threshold-gated slop
    // that lets several iterations' worth of drift accumulate unreported
    // before dumping it as one lump. When currsep is already at or above
    // kMinSeparation there is real slack from this level, which must
    // carry forward uncorrected (not reset) for later, deeper levels to
    // draw on -- resetting it here would double-correct a shortfall that
    // slack already covers.
    if (currsep < kMinSeparation - kSeparationEpsilon) {
      size_t ancestor = GetAncestor(root, lrContour);

      // # of subtrees between the colliding vertices
      size_t n = rightSubtreeIndex - ancestor;

      newSeparations[ancestor] += (kMinSeparation - currsep) / static_cast<float>(n);
      currsep = kMinSeparation;
    }
  }

  // Accumulate final separation distribution
  float acc = 0.0f, totalNewSeparation = 0.0f;
  for (size_t i = 0; i < rightSubtreeIndex; i++) {
    // accumulate each element we see to build the separation of this
    // iteration
    acc += newSeparations[i];

    // newSeparations[i] overwritten to store separation of this iteration
    newSeparations[i] = acc;

    // accumulate all separations for total added separation
    totalNewSeparation += acc;
  }

  // Apply new separation to state
  if (rightSubtreeIndex > 1) {
    const std::vector<float> &offsets = dec_[root].offsets;
    for (size_t i = 0; i < rightSubtreeIndex - 1; i++)
      newSeparations[i] += std::fabs(offsets[i + 1] - offsets[i]);
  }

  // seperationsToOffsets, inlined: rewrite dec_[root].offsets[0..rightSubtreeIndex]
  // from the accumulated separations. The old C version also memset the
  // [0, rightSubtreeIndex) prefix to 0 first, but every one of those
  // entries is unconditionally overwritten below (offsets[0] explicitly,
  // offsets[1..] by the running-sum loop) -- the memset never had an
  // observable effect and is dropped here.
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

  // Iterates through both contours and separates all children to add the
  // right subtree to the blob.
  SeparationResult res = SeparateAlongContours(lrContour, rlContour);

  // Maintain ancestor values.
  // If the right subtree was deeper or as deep
  if (!ContourAtEnd(rlContour) || ContourAtEnd(lrContour)) {
    // New default ancestor
    defaultAncestor_ = rightSubtree;
  } else {
    // Sets rightSubtree to be the ancestor along all rr contour vertices
    SetAncestorAlongRightContour(rightSubtree);
  }

  UpdateExtremes(root, extremes, res);
  CreateThreads(root, i, lrContour, rlContour, extremes, res);
}

void ReingoldTilford::CalculateOffsets(size_t root, size_t level) {
  size_t degree = graph_.Degree(root);

  // Divide
  for (size_t i = 0; i < degree; i++)
    CalculateOffsets(graph_.Neighbor(root, i), level);

  // Initialization. Includes base case (leaf node)
  InitializeRTSubtreeRoot(root, level);

  // Conquer
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
