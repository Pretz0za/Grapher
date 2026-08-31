#ifndef GVIZ_REINGOLDTILFORDTRACE_HPP
#define GVIZ_REINGOLDTILFORDTRACE_HPP

#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "Graph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * A from-scratch, clarity-first reimplementation of the same
 * Buchheim/Walker-style linear-time Reingold-Tilford algorithm
 * `ReingoldTilford` (ReingoldTilford.hpp) implements, whose only job is to
 * record every intermediate decision as a flat, pure-data event log a
 * front-end can play back at its own pace. Exists purely for teaching/
 * visualization; not used by `ReingoldTilford` or anything else in this
 * library. Do not use this class where `ReingoldTilford` itself is wanted.
 *
 * Mirrors `ReingoldTilford`'s one-shot, sequential workflow exactly
 * (construct -> CalculateOffsets(root, 0) -> Embed(root, origin)) and the
 * same math: same contour-walk separation logic, same thread-creation
 * rule, same offset accumulation. Unlike `ReingoldTilford::CalculateOffsets`
 * (which never increments `level` on its recursive call), this class
 * correctly propagates `level + 1`, so `Height()` and each event's `level`
 * field are real tree depth -- this has no effect on final positions
 * (`UpdateExtremes` only ever mutates a leaf's own offsets slot, which is
 * never re-read for that vertex again), only on depth-derived fields.
 */
class ReingoldTilfordTrace : public EmbeddedGraph {
public:
  /** Sentinel for Decorators::threadTo meaning no contour thread is set. */
  static constexpr size_t kNoThread = static_cast<size_t>(-1);

  /** Tags which fields of `Event` are meaningful; see each field's comment
   *  in `Event` for the per-kind mapping. Ordered roughly in the sequence
   *  a single `CombineSubtreeLeft` call produces them. */
  enum class EventKind {
    /** One vertex finished `InitializeRTSubtreeRoot`/`InitializeRTLeaf`:
     *  its own isolated local layout, before any merge with a sibling.
     *  Fired once per vertex, in `CalculateOffsets`'s post-order. */
    SubtreeIsolated,
    /** One `CombineSubtreeLeft(root, childIndex)` call (childIndex > 0)
     *  is starting: about to merge the already-combined blob of root's
     *  children [0, childIndex) with child childIndex. */
    ContourCompareBegin,
    /** One contour-depth level of `SeparateAlongContours`' walk has just
     *  been reached and its pair's current separation
     *  (`measuredSeparation`) measured, before deciding whether it needs
     *  correcting. Always immediately followed by exactly one
     *  `ContourCorrect` for the same pair. */
    ContourMeasure,
    /** The correction (if any) for the pair `ContourMeasure` just
     *  reported, immediately following it in the log. See
     *  `correctionFired`. */
    ContourCorrect,
    /** A contour thread was just created (`CreateThreads`) because the two
     *  contours being compared reached unequal depth, so a future contour
     *  walk can jump directly to where the still-going contour left off. */
    ThreadCreated,
    /** `UpdateExtremes` just picked the merged blob's new leftmost/
     *  rightmost extreme vertices. */
    ExtremesUpdated,
    /** One `CombineSubtreeLeft(root, childIndex)` call finished: root's
     *  local offsets are now up to date through childIndex. */
    SubtreeMerged,
    /** One vertex was just assigned its final absolute position by
     *  `Embed`. */
    EmbedStep,
  };

  /**
   * One discrete algorithm event. Deliberately pure data -- no pixel,
   * color, timing, or other rendering concept appears anywhere in this
   * type or this file; a consumer (e.g. a renderer) decides entirely on
   * its own how to stage, color, and pace playback of the event list
   * `Events()` returns. Which fields are meaningful depends on `kind`;
   * fields not documented for a given kind hold their default value and
   * should be ignored.
   */
  struct Event {
    EventKind kind;

    /** SubtreeIsolated: the vertex that finished. ContourCompareBegin/
     *  SubtreeMerged: the merge's parent vertex. ExtremesUpdated: the
     *  blob's root. EmbedStep: the vertex just embedded. Unused for
     *  ContourMeasure/ContourCorrect/ThreadCreated. */
    size_t root = 0;

    /** ContourCompareBegin/SubtreeMerged: the child position `i` passed to
     *  `CombineSubtreeLeft(root, i)`. Unused otherwise. */
    size_t childIndex = 0;

    /** SubtreeIsolated: == root. ContourCompareBegin/ContourMeasure: the
     *  left blob's right-contour vertex at this level (post-step, except
     *  the root-level pair). ContourCorrect: same vertex as the preceding
     *  ContourMeasure. ThreadCreated: the extreme vertex that received the
     *  new thread (see `rightIsSource`). ExtremesUpdated: the vertex
     *  chosen as the blob's new leftmost extreme. EmbedStep: unused. */
    size_t vertexA = 0;

    /** ContourCompareBegin/ContourMeasure: the new subtree's left-contour
     *  vertex at this level (post-step, except the root-level pair).
     *  ContourCorrect: same vertex as the preceding ContourMeasure.
     *  ThreadCreated: the contour vertex the thread now points to.
     *  ExtremesUpdated: the vertex chosen as the blob's new rightmost
     *  extreme. Unused otherwise. */
    size_t vertexB = 0;

    /** ThreadCreated only: true when the left (already-merged) blob was
     *  the deeper side; false when the new right subtree was deeper.
     *  Distinguishes `CreateThreads`'s two mutually exclusive branches. */
    bool rightIsSource = false;

    /** ContourMeasure: this level's rightward contour displacement
     *  (`lstep` in `SeparateAlongContours`); 0 for the root-level pair.
     *  Unused otherwise. */
    float leftContourStep = 0.0f;

    /** ContourMeasure: this level's leftward contour displacement
     *  (`rstep`); 0 for the root-level pair. Unused otherwise. */
    float rightContourStep = 0.0f;

    /** ContourMeasure only: `currsep` for this level, before any
     *  correction -- the pair's current separation in offset units.
     *  kMinSeparation (1.0) is "just enough, no collision"; less is a
     *  shortfall the following ContourCorrect fixes; more is slack, not
     *  corrected away. Always 0 for the root-level pair. */
    float measuredSeparation = 0.0f;

    /** ContourCorrect only: true when the just-measured pair's separation
     *  was below kMinSeparation (past a float-noise epsilon) and a
     *  correction was applied to bring it back up to exactly
     *  kMinSeparation. False means nothing was added;
     *  `correctionAmount`/`ancestor` are both 0 in that case. */
    bool correctionFired = false;

    /** ContourCorrect only, meaningful when `correctionFired`: the
     *  magnitude of the correction just applied -- exactly this level's
     *  own shortfall. */
    float correctionAmount = 0.0f;

    /** ContourCorrect only, meaningful when `correctionFired`: which index
     *  of root's `newSeparations` array this correction was added to --
     *  `GetAncestor(root, lrContour)`'s result for every level after the
     *  first, or `childIndex - 1` for the root-level pair. The final
     *  per-child offsets `SubtreeMerged.offsets` reports are a prefix sum
     *  over every gap's accumulated corrections. */
    size_t ancestor = 0;

    /** SubtreeIsolated: the vertex's local offsets right after
     *  initialization (one entry per child, all 0). SubtreeMerged: root's
     *  full local offsets array after this merge. Empty otherwise. */
    std::vector<float> offsets;

    /** SubtreeIsolated: the depth this vertex was initialized at (real
     *  tree depth). EmbedStep: unused. */
    size_t level = 0;

    /** EmbedStep only: the absolute 2D position just assigned to `root`. */
    double position[2] = {0.0, 0.0};
  };

  /** Same validation as `ReingoldTilford::ReingoldTilford`: throws
   *  `NotATreeError`/`NoLayoutError` under the same conditions. */
  ReingoldTilfordTrace(const Graph &graph, size_t root);

  /**
   * Computes horizontal offsets for the subtree rooted at @p root, exactly
   * as `ReingoldTilford::CalculateOffsets` does, appending one or more
   * `Event`s to `Events()` for every step along the way. Must run once,
   * before `Embed`, over the same root passed to the constructor.
   */
  void CalculateOffsets(size_t root, size_t level);

  /**
   * Recursively assigns 2D positions starting from @p root at @p position,
   * exactly as `ReingoldTilford::Embed` does, appending one `EmbedStep`
   * event per vertex. Call `CalculateOffsets` first.
   */
  void Embed(size_t root, const double *position);

  /** The calculated height of the whole tree. */
  size_t Height() const noexcept { return height_; }

  /** The full, ordered event log recorded by `CalculateOffsets` (and, once
   *  called, `Embed`). Valid to read at any point; grows as those methods
   *  run. Empty before `CalculateOffsets` is called. */
  const std::vector<Event> &Events() const noexcept { return events_; }

private:
  struct Decorators {
    std::vector<float> offsets;
    size_t lMost = 0;
    size_t rMost = 0;
    size_t depth = 0;
    size_t ancestor = 0;
    size_t threadTo = kNoThread;
  };

  struct SeparationResult {
    float lOffset;
    float rOffset;
    float totalNewSeparation;
    /** See ReingoldTilford::SeparationResult::rmostSeparation -- kept in
     *  lockstep with that field. */
    float rmostSeparation;
  };

  struct SubtreePairExtremes {
    size_t ll;
    size_t lr;
    size_t rl;
    size_t rr;
  };

  bool IsThreaded(size_t v) const noexcept { return dec_[v].threadTo != kNoThread; }
  float IterateContourRightward(size_t &contour);
  float IterateContourLeftward(size_t &contour);
  size_t GetAncestor(size_t root, size_t i) const;
  void SetAncestorAlongRightContour(size_t i);
  void InitializeRTLeaf(size_t i, size_t level);
  void InitializeRTSubtreeRoot(size_t root, size_t level);
  void CreateThreads(size_t root, size_t i, size_t &lrContour, size_t &rlContour,
                      const SubtreePairExtremes &extremes, const SeparationResult &res);
  void UpdateExtremes(size_t root, const SubtreePairExtremes &extremes,
                       const SeparationResult &res);
  SeparationResult SeparateAlongContours(size_t &lrContour, size_t &rlContour);
  void CombineSubtreeLeft(size_t root, size_t i);

  const Graph &graph_;
  std::vector<Decorators> dec_;
  std::vector<int> parents_;
  std::vector<Event> events_;
  size_t height_ = 0;
  size_t defaultAncestor_ = 0;
};

} // namespace gviz::layout

#endif
