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
 * front-end can play back at its own pace. It exists purely for teaching/
 * visualization -- see e.g. grender's rtTraceDemo -- and is deliberately
 * NOT the production embedder: nothing here is on any hot path, it is not
 * used by `ReingoldTilford` or anything else in this library, and it is
 * fine for it to copy vectors liberally in places `ReingoldTilford` would
 * not. Do not use this class where `ReingoldTilford` itself is what's
 * wanted.
 *
 * Every public entry point mirrors `ReingoldTilford`'s one-shot, sequential
 * workflow exactly (construct -> CalculateOffsets(root, 0) -> Embed(root,
 * origin), each called once) and the same math: same contour-walk
 * separation logic, same thread-creation rule, same offset accumulation, so
 * that watching this class's event log genuinely teaches how
 * `ReingoldTilford` computes its layout, not a simplified stand-in
 * algorithm.
 *
 * One deliberate divergence, called out explicitly rather than silently
 * reproduced: `ReingoldTilford::CalculateOffsets` has a documented, ported-
 * as-is bug (see its header comment) where the recursive call never
 * increments `level`, so `Decorators::depth` never reflects real tree depth
 * and `UpdateExtremes`'s "which extreme subtree is deeper" comparison
 * always ties and always takes its `else` branch. This class fixes that --
 * `CalculateOffsets` here passes `level + 1` down, so `depth`/`Height()`/
 * an event's `level` field are real, and the deeper-extreme heuristic the
 * algorithm is actually supposed to run, runs (a teaching visualization
 * that silently re-demonstrated a known bug as if it were the intended
 * algorithm would misinform, not teach).
 *
 * `SeparateAlongContours`' correction cadence is one level, one
 * measure-then-correct pair, every time (`ContourMeasure` followed by
 * `ContourCorrect`) -- never a threshold-gated batch that silently lets
 * several levels' drift accumulate before reporting one lump correction.
 * Concretely: `currsep` is checked and, if it is short of `kMinSeparation`,
 * corrected back up to exactly `kMinSeparation` on *every* iteration, no
 * matter how small the shortfall; when `currsep` is already at or above
 * `kMinSeparation` (this level has slack, not a shortfall), nothing is
 * added and -- critically -- `currsep` is *not* reset back down to
 * `kMinSeparation` either, so that slack carries forward for a later,
 * deeper level to draw on instead of being corrected again for no reason.
 * That carry-forward is what makes summing every level's independently-
 * computed shortfall equal the true, single required total: correcting a
 * level only ever brings its own `currsep` up to exactly `kMinSeparation`,
 * so the *next* level's measurement starts from that same true baseline
 * plus whatever real slack preceded it, not from an arbitrarily reset
 * value that would double-count a shortfall the slack already covers.
 * `ReingoldTilford.cpp`'s `SeparateAlongContours` implements this
 * identically (same `kMinSeparation`/epsilon-gated one-directional check,
 * same carry-forward-on-slack rule) precisely so the two classes' final
 * offsets keep agreeing -- see `ReingoldTilfordTraceTests.cpp`'s
 * `test_matches*` parity tests.
 *
 * This turns out to be a no-op on final geometry, not just "usually
 * harmless": `UpdateExtremes` only ever picks between two candidate
 * vertices that are themselves always tree leaves (`Decorators::lMost`/
 * `rMost` bottom out at an actual 0-degree leaf by construction), and only
 * mutates that chosen leaf's *own* `offsets[0]` bookkeeping slot -- which
 * `IterateContourLeftward`/`Rightward` only ever reads for a vertex the
 * *outer* contour-walk loop has not yet recognized as a leaf, i.e. never
 * for that same vertex again (`gviz::search::IsLeaf` is a real-degree
 * check, unaffected by `Decorators::threadTo`). `Embed`'s only input is
 * `Decorators::offsets`, which `SeparateAlongContours` computes without
 * consulting `depth`/lMost/rMost at all. Net effect: this class's final
 * positions are provably identical to `ReingoldTilford`'s for the same
 * tree regardless of the depth fix -- verified in
 * `ReingoldTilfordTraceTests.cpp` not just on the existing symmetric
 * fixtures but on asymmetric, thread-triggering trees too. The fix still
 * matters for anything that reads `Height()` or an event's `level` field
 * (both wrong -- always 0 -- in `ReingoldTilford`), just not for positions.
 */
class ReingoldTilfordTrace : public EmbeddedGraph {
public:
  /** Sentinel for Decorators::threadTo meaning no contour thread is set. */
  static constexpr size_t kNoThread = static_cast<size_t>(-1);

  /** Tags which fields of `Event` are meaningful; see each field's comment
   *  in `Event` for the per-kind mapping. Ordered roughly in the sequence
   *  a single `CombineSubtreeLeft` call produces them, which is also a
   *  reasonable default grouping for a playback UI. */
  enum class EventKind {
    /** One vertex finished `InitializeRTSubtreeRoot`/`InitializeRTLeaf`:
     *  its own isolated local layout, before it takes part in any merge
     *  with a sibling. Fired once per vertex, in `CalculateOffsets`'s
     *  post-order (children before their parent). */
    SubtreeIsolated,
    /** One `CombineSubtreeLeft(root, childIndex)` call (childIndex > 0)
     *  is starting: about to merge the already-combined blob of root's
     *  children [0, childIndex) with child childIndex. */
    ContourCompareBegin,
    /** One contour-depth level of `SeparateAlongContours`' walk has just
     *  been reached, and its pair's current separation (`measuredSeparation`)
     *  has just been measured -- *before* deciding whether that needs
     *  correcting. Always immediately followed by exactly one
     *  `ContourCorrect` for the same pair: these two together are one
     *  level's "measure, then correct" cycle, the smallest unit
     *  `SeparateAlongContours` reasons in. The *first* level of every
     *  merge is the root pair itself -- lrContour/rlContour exactly as
     *  `ContourCompareBegin` just displayed them, always measured at 0
     *  (fully overlapped) and always corrected by exactly kMinSeparation;
     *  no level, not even the first, is ever just assumed correct.
     *  Every level after that steps one level deeper first -- one step
     *  rightward along the left blob's right contour, one step leftward
     *  along the new subtree's left contour -- then measures the newly-
     *  revealed pair. Advancing to the *next* level is otherwise implicit
     *  -- it's just the next `ContourMeasure` in the log. */
    ContourMeasure,
    /** The correction (if any) for the pair `ContourMeasure` just
     *  reported, immediately following it in the log. Fired independently
     *  every level -- never deferred, never bundled with a later level's
     *  correction -- so `correctionAmount` is always exactly this one
     *  level's own shortfall, nothing more (the very first level, the root
     *  pair, always fires: see `ContourMeasure`). See `correctionFired`. */
    ContourCorrect,
    /** A contour thread was just created (`CreateThreads`) because the two
     *  contours being compared reached unequal depth -- the mechanism that
     *  makes future contour walks that would otherwise re-walk an
     *  exhausted branch jump directly to where the still-going contour
     *  left off, in O(1). */
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
     *  SubtreeMerged: the merge's parent vertex ( == `graph.Neighbor`'s
     *  common parent of the two subtrees being merged). ExtremesUpdated:
     *  the blob's root. EmbedStep: the vertex just embedded. Unused
     *  (0) for ContourMeasure/ContourCorrect/ThreadCreated. */
    size_t root = 0;

    /** ContourCompareBegin/SubtreeMerged: the child position `i` passed to
     *  `CombineSubtreeLeft(root, i)` -- root's children [0, i) were
     *  already merged; this call folds in child i. Unused otherwise. */
    size_t childIndex = 0;

    /** SubtreeIsolated: == root (repeated here for a uniform "the vertex
     *  this event is about" reading). ContourCompareBegin: the left
     *  blob's right-contour starting vertex, before any step -- identical
     *  to the *first* `ContourMeasure`/`ContourCorrect` pair's vertexA for
     *  the same merge (see below). ContourMeasure: for the root-level pair
     *  (the first level of any merge), the same un-stepped vertex
     *  `ContourCompareBegin` reported; for every level after that, the
     *  right-contour vertex this level's `IterateContourRightward` call
     *  just moved *to* -- the post-step vertex `measuredSeparation` was
     *  computed against. ContourCorrect: the same vertex as the
     *  `ContourMeasure` event immediately preceding it (repeated for a
     *  playback UI that only looks at `ContourCorrect` events, so it
     *  doesn't need to remember the prior event's fields to know what's
     *  being corrected). ThreadCreated: the extreme vertex that received
     *  the new thread (`extremes.rr` when the left blob is deeper,
     *  `extremes.ll` when the right subtree is deeper -- see
     *  `rightIsSource`). ExtremesUpdated: the vertex chosen as the blob's
     *  new leftmost extreme. EmbedStep: unused (== root). */
    size_t vertexA = 0;

    /** ContourCompareBegin: the new subtree's left-contour starting
     *  vertex, before any step -- identical to the *first*
     *  `ContourMeasure`/`ContourCorrect` pair's vertexB for the same merge.
     *  ContourMeasure: for the root-level pair, the same un-stepped vertex
     *  `ContourCompareBegin` reported; for every level after that, the
     *  left-contour vertex this level's `IterateContourLeftward` call just
     *  moved *to* -- the post-step counterpart to vertexA. ContourCorrect:
     *  same vertex as the preceding `ContourMeasure`, same repetition
     *  rationale as vertexA. ThreadCreated: the contour vertex the thread
     *  now points to. ExtremesUpdated: the vertex chosen as the blob's new
     *  rightmost extreme. Unused otherwise. */
    size_t vertexB = 0;

    /** ThreadCreated only: true when the left (already-merged) blob was
     *  the deeper side (vertexA's tree threads forward to vertexB);
     *  false when the new right subtree was deeper (vertexA's tree
     *  threads backward, conceptually, but is still recorded the same
     *  vertexA-threads-to-vertexB way). Distinguishes
     *  `CreateThreads`'s two mutually exclusive branches. */
    bool rightIsSource = false;

    /** ContourMeasure: this level's rightward contour displacement
     *  (`IterateContourRightward`'s return value, `lstep` in
     *  `SeparateAlongContours`) -- 0 for the root-level pair (the first
     *  level of any merge), which has no step behind it. Unused otherwise. */
    float leftContourStep = 0.0f;

    /** ContourMeasure: this level's leftward contour displacement
     *  (`rstep`) -- 0 for the root-level pair, same rationale as
     *  leftContourStep. Unused otherwise. */
    float rightContourStep = 0.0f;

    /** ContourMeasure only: `currsep` for this level, *before* any
     *  correction -- the pair's current separation in offset units, given
     *  every correction applied at shallower levels so far (kMinSeparation,
     *  i.e. 1.0, is the "just enough, no collision" value; anything less is
     *  a shortfall that the immediately-following `ContourCorrect` will
     *  report fixing; anything more is slack this level happens to have,
     *  which is not "corrected away" -- see
     *  `ContourCorrect::correctionAmount`). Always exactly 0 for the
     *  root-level pair (the first level of any merge): it starts fully
     *  overlapped, by construction, before anything has been corrected.
     *  This is the number a playback caption should show as "currently X
     *  apart" during the measure beat. */
    float measuredSeparation = 0.0f;

    /** ContourCorrect only: true when the just-measured pair's separation
     *  was below `kMinSeparation` (by more than a small float-noise
     *  epsilon) and a correction was distributed into `newSeparations` to
     *  bring it back up to exactly `kMinSeparation` -- never more. False
     *  means this level already had enough (or exactly enough) separation
     *  on its own and nothing was added; `correctionAmount`/`ancestor` are
     *  both 0 in that case. Every `ContourMeasure` gets exactly one
     *  `ContourCorrect`, whether or not it actually fires -- corrections
     *  are never deferred to a later iteration or bundled with one. */
    bool correctionFired = false;

    /** ContourCorrect only: the magnitude of the correction just applied,
     *  meaningful only when `correctionFired`. Always exactly this one
     *  level's own shortfall (kMinSeparation minus the preceding
     *  `ContourMeasure`'s `measuredSeparation`) -- never a sum carried
     *  over from an earlier, unreported level. */
    float correctionAmount = 0.0f;

    /** ContourCorrect only, meaningful only when `correctionFired`: which
     *  index of root's `newSeparations` array (`SeparateAlongContours`'
     *  internal per-gap accumulator, one slot per gap between root's
     *  children `[0, childIndex]`) this correction was added to. For every
     *  level after the first, `GetAncestor(root, lrContour)`'s result --
     *  not necessarily `childIndex` itself, since a correction can be
     *  attributed to an earlier gap between two already-merged children.
     *  For the root-level pair (the first level of any merge, which always
     *  fires) this is always exactly `childIndex - 1`, a fixed placement
     *  rather than a `GetAncestor` lookup -- the baseline unit is always
     *  owed in the gap directly between the already-merged blob and the
     *  new child, since nothing has been measured yet to attribute it
     *  anywhere earlier. Either way, this is exactly why the final
     *  per-child offsets `SubtreeMerged.offsets` reports are a prefix sum
     *  over every gap's accumulated corrections, not a single value
     *  attributable to the newly-compared child alone. A consumer that
     *  wants to reproduce `SeparateAlongContours`'s exact final offsets
     *  (not just an approximation) needs this field to replicate
     *  the accumulation faithfully. */
    size_t ancestor = 0;

    /** SubtreeIsolated: the vertex's local offsets right after
     *  initialization (one entry per child, all 0). SubtreeMerged: root's
     *  full local offsets array after this merge, one entry per child of
     *  root -- the moment those offsets actually changed is exactly this
     *  event. Empty otherwise. */
    std::vector<float> offsets;

    /** SubtreeIsolated: the depth this vertex was initialized at (see
     *  this class's header comment on the depth-tracking divergence from
     *  `ReingoldTilford` -- this is real tree depth here). EmbedStep:
     *  unused. */
    size_t level = 0;

    /** EmbedStep only: the absolute 2D position just assigned to `root`. */
    double position[2] = {0.0, 0.0};
  };

  /**
   * Same validation as `ReingoldTilford::ReingoldTilford` -- see its doc
   * comment for the exact contract (throws `NotATreeError`/`NoLayoutError`
   * under the same conditions). Duplicated here rather than shared because
   * this class intentionally holds its own decorator/event state and does
   * not depend on `ReingoldTilford` in any way (a teaching implementation
   * that silently wrapped the production one would not actually
   * demonstrate the algorithm).
   */
  ReingoldTilfordTrace(const Graph &graph, size_t root);

  /**
   * Computes horizontal offsets for the subtree rooted at @p root, exactly
   * as `ReingoldTilford::CalculateOffsets` does, appending one or more
   * `Event`s to `Events()` for every step along the way. Must run once,
   * before `Embed`, over the same root passed to the constructor. Unlike
   * `ReingoldTilford::CalculateOffsets`, @p level is genuinely propagated
   * as `level + 1` on every recursive call -- see this class's header
   * comment for why and what it changes.
   */
  void CalculateOffsets(size_t root, size_t level);

  /**
   * Recursively assigns 2D positions starting from @p root at @p position,
   * exactly as `ReingoldTilford::Embed` does, appending one `EmbedStep`
   * event per vertex. Call `CalculateOffsets` first.
   */
  void Embed(size_t root, const double *position);

  /** The calculated height of the whole tree. Unlike
   *  `ReingoldTilford::Height`, this is real (see the depth-tracking
   *  divergence documented on this class). */
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
