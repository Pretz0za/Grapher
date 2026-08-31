// Tests for gviz::layout::ReingoldTilfordTrace -- the clarity-first,
// event-recording reimplementation of ReingoldTilford used by grender's
// teaching visualization (see ReingoldTilfordTrace.hpp's class comment).
//
// Two things this suite is here to prove, cheaply, rather than just assert
// by inspection:
//   1. It teaches the real algorithm: for the same tree, its final Embed()
//      positions match ReingoldTilford's exactly -- checked on both a
//      depth-symmetric tree and a depth-asymmetric, thread-triggering tree
//      (see ReingoldTilfordTrace.hpp's header comment for why the fix this
//      class makes to ReingoldTilford's known depth-tracking bug is
//      provably a no-op on final geometry).
//   2. The event log is well-formed: right event kinds/counts for a known
//      tree shape, thread events carry valid vertex ids, per-vertex depth
//      (the `level` field) is real (unlike ReingoldTilford::Height(),
//      always 0).

#include "ReingoldTilfordTrace.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "ReingoldTilford.hpp"
#include "unity/unity.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using gviz::Graph;
using gviz::NotATreeError;
using gviz::layout::ReingoldTilford;
using gviz::layout::ReingoldTilfordTrace;
using Kind = ReingoldTilfordTrace::EventKind;

void setUp(void) {}
void tearDown(void) {}

// 7-vertex complete binary tree (depth-symmetric: every CombineSubtreeLeft
// call in this tree merges same-depth subtrees, exactly matching
// ReingoldTilfordTests.cpp's AddBinaryTreeDepth2).
//           0
//         /   \
//        1     2
//       / \   / \
//      3   4 5   6
static void AddBinaryTreeDepth2(Graph &g) {
  for (int i = 0; i < 7; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(1, 4, 1.0);
  g.AddEdge(2, 5, 1.0);
  g.AddEdge(2, 6, 1.0);
}

// 10-vertex depth-asymmetric tree, empirically verified (see the task's
// exploration) to exercise at least one CreateThreads call:
//   0 -> 1, 2
//   1 -> 3, 4        (3 is a leaf at depth 2; 4 has children at depth 3)
//   4 -> 5, 6
//   2 -> 7 -> 8 -> 9  (a single-child chain down to depth 4)
static void AddAsymmetricThreadingTree(Graph &g) {
  for (int i = 0; i < 10; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(1, 4, 1.0);
  g.AddEdge(4, 5, 1.0);
  g.AddEdge(4, 6, 1.0);
  g.AddEdge(2, 7, 1.0);
  g.AddEdge(7, 8, 1.0);
  g.AddEdge(8, 9, 1.0);
}

// Two matched-structure "blobs" merged directly under a common root: each
// blob is a complete ternary tree of depth 2 (1 mid-level vertex -> 3
// children -> each with 3 leaf grandchildren). Regression fixture for a
// reported "crossing edges" bug in grender's rtTraceDemo: root's single
// merge (combining blob 1 with blob 14) requires *two* real
// ContourMeasure/ContourCorrect iterations (root-pair 1/14, then their
// rightmost/leftmost grandchildren 4/15) before either contour bottoms
// out, both of which land on the same (only) ancestor gap 0 --
// investigation traced the demo's *visual* crossing to its own
// incremental-apply staging (translating a whole rigid blob to a still-
// partial answer between the two corrections), not to any misattribution
// or bundling in this event log: see the two ContourCorrect assertions
// below, and the no-inversion check on final Embed() positions.
static void AddMatchedTernaryBlobsTree(Graph &g) {
  for (int i = 0; i < 27; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 14, 1.0);

  g.AddEdge(1, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(1, 4, 1.0);
  g.AddEdge(2, 5, 1.0);
  g.AddEdge(2, 6, 1.0);
  g.AddEdge(2, 7, 1.0);
  g.AddEdge(3, 8, 1.0);
  g.AddEdge(3, 9, 1.0);
  g.AddEdge(3, 10, 1.0);
  g.AddEdge(4, 11, 1.0);
  g.AddEdge(4, 12, 1.0);
  g.AddEdge(4, 13, 1.0);

  g.AddEdge(14, 15, 1.0);
  g.AddEdge(14, 16, 1.0);
  g.AddEdge(14, 17, 1.0);
  g.AddEdge(15, 18, 1.0);
  g.AddEdge(15, 19, 1.0);
  g.AddEdge(15, 20, 1.0);
  g.AddEdge(16, 21, 1.0);
  g.AddEdge(16, 22, 1.0);
  g.AddEdge(16, 23, 1.0);
  g.AddEdge(17, 24, 1.0);
  g.AddEdge(17, 25, 1.0);
  g.AddEdge(17, 26, 1.0);
}

static void AddDirectedCycle(Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 0, 1.0);
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

static void test_construct_validTree_succeeds(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  TEST_ASSERT_EQUAL_UINT64(7, trace.PositionCount());
  TEST_ASSERT_TRUE(trace.Events().empty()); // nothing run yet
}

static void test_construct_directedCycle_throws(void) {
  Graph g(/*directed=*/true);
  AddDirectedCycle(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilfordTrace trace(g, 0);
    (void)trace;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_construct_wrongRoot_throws(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilfordTrace trace(g, 1); // not the tree's actual root
    (void)trace;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// ============================================================================
// POSITION EQUIVALENCE WITH ReingoldTilford
// ============================================================================

static void test_matchesReingoldTilford_binaryTree(void) {
  Graph gProd(/*directed=*/true);
  AddBinaryTreeDepth2(gProd);
  gProd.BuildLayout();
  ReingoldTilford prod(gProd, 0);
  prod.CalculateOffsets(0, 0);
  double origin[2] = {0.0, 0.0};
  prod.Embed(0, origin);

  Graph gTrace(/*directed=*/true);
  AddBinaryTreeDepth2(gTrace);
  gTrace.BuildLayout();
  ReingoldTilfordTrace trace(gTrace, 0);
  trace.CalculateOffsets(0, 0);
  trace.Embed(0, origin);

  for (size_t i = 0; i < 7; i++) {
    double *a = prod.GetVPosition(i);
    double *b = trace.GetVPosition(i);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, a[0], b[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, a[1], b[1]);
  }
}

static void test_matchesReingoldTilford_asymmetricThreadingTree(void) {
  Graph gProd(/*directed=*/true);
  AddAsymmetricThreadingTree(gProd);
  gProd.BuildLayout();
  ReingoldTilford prod(gProd, 0);
  prod.CalculateOffsets(0, 0);
  double origin[2] = {0.0, 0.0};
  prod.Embed(0, origin);

  Graph gTrace(/*directed=*/true);
  AddAsymmetricThreadingTree(gTrace);
  gTrace.BuildLayout();
  ReingoldTilfordTrace trace(gTrace, 0);
  trace.CalculateOffsets(0, 0);
  trace.Embed(0, origin);

  // First confirm this tree is actually exercising a thread -- otherwise
  // this test wouldn't prove what its name claims.
  bool sawThread = false;
  for (const auto &e : trace.Events())
    if (e.kind == Kind::ThreadCreated)
      sawThread = true;
  TEST_ASSERT_TRUE(sawThread);

  for (size_t i = 0; i < 10; i++) {
    double *a = prod.GetVPosition(i);
    double *b = trace.GetVPosition(i);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, a[0], b[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, a[1], b[1]);
  }
}

static void test_matchesReingoldTilford_matchedTernaryBlobs(void) {
  Graph gProd(/*directed=*/true);
  AddMatchedTernaryBlobsTree(gProd);
  gProd.BuildLayout();
  ReingoldTilford prod(gProd, 0);
  prod.CalculateOffsets(0, 0);
  double origin[2] = {0.0, 0.0};
  prod.Embed(0, origin);

  Graph gTrace(/*directed=*/true);
  AddMatchedTernaryBlobsTree(gTrace);
  gTrace.BuildLayout();
  ReingoldTilfordTrace trace(gTrace, 0);
  trace.CalculateOffsets(0, 0);
  trace.Embed(0, origin);

  for (size_t i = 0; i < 27; i++) {
    double *a = prod.GetVPosition(i);
    double *b = trace.GetVPosition(i);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, a[0], b[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, a[1], b[1]);
  }
}

// Regression test for the reported "crossing edges" bug and its follow-up
// mislabeling and process-model fixes: root's one merge (blob 1 vs blob
// 14) requires two full measure/correct iterations landing on the same
// ancestor gap (0 is the only gap there is, since root has exactly two
// children) before the walk bottoms out at a leaf pair. Confirms both
// iterations fire independently -- i.e. this event log was never bundling
// multiple levels' drift into one lump for this merge, so the original
// crossing-edges artifact traced back to the demo's incremental staging,
// not to gviz -- AND that each correction is reported against the
// *post-step* pair it was actually computed for (SeparateAlongContours
// checks currsep/fires a correction using lrContour/rlContour *after*
// that iteration's Iterate...calls, so ContourMeasure/ContourCorrect's
// vertexA/vertexB must reflect that same post-step pair, not the pre-step
// one the walk started this iteration at -- see
// ReingoldTilfordTrace.hpp's Event::vertexA/vertexB doc comments). Also
// confirms final Embed() positions preserve sibling order and minimum
// separation despite that intermediate two-step convergence.
static void test_matchedTernaryBlobs_contourStepsIndependentAndFinalOrderPreserved(void) {
  Graph g(/*directed=*/true);
  AddMatchedTernaryBlobsTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  bool sawOne = false, sawSix = false, sawTwo = false;
  for (const auto &e : trace.Events()) {
    if (e.kind != Kind::ContourCorrect || e.root != 0 || e.childIndex != 1)
      continue;
    TEST_ASSERT_TRUE(e.correctionFired);
    // All three corrections land on the merge's one and only gap -- proves
    // there's no misattribution to a wrong ancestor for this shape.
    TEST_ASSERT_EQUAL_UINT64(0, e.ancestor);
    if (std::fabs(static_cast<double>(e.correctionAmount) - 1.0) < 1e-3) {
      sawOne = true;
      // The 1.0 correction is the root-level pair (1, 14) itself, before
      // any stepping -- always exactly kMinSeparation since it starts
      // fully overlapped (measuredSeparation 0).
      TEST_ASSERT_EQUAL_UINT64(1, e.vertexA);
      TEST_ASSERT_EQUAL_UINT64(14, e.vertexB);
    }
    if (std::fabs(static_cast<double>(e.correctionAmount) - 6.0) < 1e-3) {
      sawSix = true;
      // The 6.0 correction is the root-pair (1, 14) stepping down to their
      // rightmost/leftmost children -- it must be reported against (4, 15),
      // the pair it was actually computed for, not the stale (1, 14) pair
      // the walk started this iteration at.
      TEST_ASSERT_EQUAL_UINT64(4, e.vertexA);
      TEST_ASSERT_EQUAL_UINT64(15, e.vertexB);
    }
    if (std::fabs(static_cast<double>(e.correctionAmount) - 2.0) < 1e-3) {
      sawTwo = true;
      // The 2.0 correction is the (4, 15) pair stepping down one more
      // level to their own rightmost/leftmost children -- reported against
      // (13, 18), not the stale (4, 15) pair.
      TEST_ASSERT_EQUAL_UINT64(13, e.vertexA);
      TEST_ASSERT_EQUAL_UINT64(18, e.vertexB);
    }
  }
  // Three independent, correctly-sized corrections (1.0 for the root-level
  // pair, 6.0 for the first stepped-down comparison, 2.0 for the deepest)
  // -- not one lumped 9.0, and not a single event silently absorbing all.
  TEST_ASSERT_TRUE(sawOne);
  TEST_ASSERT_TRUE(sawSix);
  TEST_ASSERT_TRUE(sawTwo);

  // Every ContourMeasure for this merge must be immediately followed by
  // exactly one ContourCorrect for the same pair -- the measure/correct
  // cycle is never split across a different pair or dropped.
  size_t measures = 0, corrects = 0;
  bool prevWasMeasure = false;
  size_t prevA = 0, prevB = 0;
  for (const auto &e : trace.Events()) {
    if (e.root != 0 || e.childIndex != 1)
      continue;
    if (e.kind == Kind::ContourMeasure) {
      measures++;
      prevWasMeasure = true;
      prevA = e.vertexA;
      prevB = e.vertexB;
    } else if (e.kind == Kind::ContourCorrect) {
      TEST_ASSERT_TRUE(prevWasMeasure);
      TEST_ASSERT_EQUAL_UINT64(prevA, e.vertexA);
      TEST_ASSERT_EQUAL_UINT64(prevB, e.vertexB);
      corrects++;
      prevWasMeasure = false;
    }
  }
  // Root-level pair + two stepped-down levels = 3 measure/correct cycles.
  TEST_ASSERT_EQUAL_UINT64(3, measures);
  TEST_ASSERT_EQUAL_UINT64(3, corrects);

  double origin[2] = {0.0, 0.0};
  trace.Embed(0, origin);

  // Every vertex under blob 1 (ids 1-13) must sit strictly left of every
  // vertex under blob 14 (ids 14-26) in the final layout -- no crossing.
  double maxLeft = trace.GetVPosition(1)[0];
  for (size_t i = 1; i <= 13; i++)
    maxLeft = std::max(maxLeft, trace.GetVPosition(i)[0]);
  double minRight = trace.GetVPosition(14)[0];
  for (size_t i = 14; i <= 26; i++)
    minRight = std::min(minRight, trace.GetVPosition(i)[0]);
  TEST_ASSERT_TRUE(maxLeft < minRight);
}

// ============================================================================
// HEIGHT / DEPTH (this class's one deliberate, positionally-inert fix)
// ============================================================================

static void test_height_reflectsRealDepth(void) {
  Graph g(/*directed=*/true);
  AddAsymmetricThreadingTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  // Real height of this tree: 0 -> 2 -> 7 -> 8 -> 9 is 4 edges deep.
  TEST_ASSERT_EQUAL_UINT64(4, trace.Height());
}

static void test_subtreeIsolated_levelsMatchRealDepth(void) {
  Graph g(/*directed=*/true);
  AddAsymmetricThreadingTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  size_t levelOf[10] = {0};
  bool seen[10] = {false};
  for (const auto &e : trace.Events()) {
    if (e.kind != Kind::SubtreeIsolated)
      continue;
    levelOf[e.root] = e.level;
    seen[e.root] = true;
  }
  for (size_t i = 0; i < 10; i++)
    TEST_ASSERT_TRUE(seen[i]);

  TEST_ASSERT_EQUAL_UINT64(0, levelOf[0]);
  TEST_ASSERT_EQUAL_UINT64(1, levelOf[1]);
  TEST_ASSERT_EQUAL_UINT64(1, levelOf[2]);
  TEST_ASSERT_EQUAL_UINT64(2, levelOf[3]);
  TEST_ASSERT_EQUAL_UINT64(2, levelOf[4]);
  TEST_ASSERT_EQUAL_UINT64(3, levelOf[5]);
  TEST_ASSERT_EQUAL_UINT64(3, levelOf[6]);
  TEST_ASSERT_EQUAL_UINT64(2, levelOf[7]);
  TEST_ASSERT_EQUAL_UINT64(3, levelOf[8]);
  TEST_ASSERT_EQUAL_UINT64(4, levelOf[9]);
}

// ============================================================================
// EVENT LOG SHAPE
// ============================================================================

static void test_events_oneSubtreeIsolatedPerVertex(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  size_t count = 0;
  for (const auto &e : trace.Events())
    if (e.kind == Kind::SubtreeIsolated)
      count++;
  TEST_ASSERT_EQUAL_UINT64(7, count);
}

static void test_events_oneEmbedStepPerVertex(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);
  double origin[2] = {0.0, 0.0};
  trace.Embed(0, origin);

  size_t count = 0;
  bool seenVertex[7] = {false};
  for (const auto &e : trace.Events()) {
    if (e.kind != Kind::EmbedStep)
      continue;
    count++;
    seenVertex[e.root] = true;
  }
  TEST_ASSERT_EQUAL_UINT64(7, count);
  for (size_t i = 0; i < 7; i++)
    TEST_ASSERT_TRUE(seenVertex[i]);
}

static void test_events_subtreeMergedOncePerInternalMerge(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  // Every internal vertex (0, 1, 2) has exactly 2 children -> exactly one
  // CombineSubtreeLeft(root, 1) call (CombineSubtreeLeft(root, 0) is a
  // documented no-op, nothing to merge against yet) -> one SubtreeMerged
  // event each.
  size_t count = 0;
  for (const auto &e : trace.Events())
    if (e.kind == Kind::SubtreeMerged)
      count++;
  TEST_ASSERT_EQUAL_UINT64(3, count);
}

static void test_events_threadCreated_vertexIdsInRange(void) {
  Graph g(/*directed=*/true);
  AddAsymmetricThreadingTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  bool sawThread = false;
  for (const auto &e : trace.Events()) {
    if (e.kind != Kind::ThreadCreated)
      continue;
    sawThread = true;
    TEST_ASSERT_TRUE(e.vertexA < 10);
    TEST_ASSERT_TRUE(e.vertexB < 10);
  }
  TEST_ASSERT_TRUE(sawThread);
}

static void test_events_contourMeasure_capturesOffsetDeltas(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  // Three internal merges total (root 0 merging its two children 1, 2; and
  // each of 1 and 2 merging their own two leaf children) -> three
  // ContourCompareBegin events, and -- since every merge now gets its own
  // root-level measure/correct cycle before any stepping, even a merge
  // whose while loop never runs at all -- three root-level
  // ContourMeasure/ContourCorrect pairs (one per merge) plus one more pair
  // from the top merge's single stepped-down iteration (0, combining
  // subtrees 1 and 2, both non-leaf, one iteration before lrContour
  // reaches a leaf): four ContourMeasure/ContourCorrect pairs total.
  size_t compareBegin = 0, measures = 0, corrects = 0;
  for (const auto &e : trace.Events()) {
    if (e.kind == Kind::ContourCompareBegin)
      compareBegin++;
    if (e.kind == Kind::ContourMeasure)
      measures++;
    if (e.kind == Kind::ContourCorrect)
      corrects++;
  }
  TEST_ASSERT_EQUAL_UINT64(3, compareBegin);
  TEST_ASSERT_EQUAL_UINT64(4, measures);
  TEST_ASSERT_EQUAL_UINT64(4, corrects);
}

static void test_events_contourMeasure_firesOnDeeperMerge(void) {
  Graph g(/*directed=*/true);
  AddAsymmetricThreadingTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  // The top-level merge (root 0's two children, 1 and 2) are both internal
  // (non-leaf) vertices, so SeparateAlongContours' loop runs at least once,
  // and every iteration produces one ContourMeasure and one ContourCorrect.
  size_t measures = 0, corrects = 0;
  for (const auto &e : trace.Events()) {
    if (e.kind == Kind::ContourMeasure)
      measures++;
    if (e.kind == Kind::ContourCorrect)
      corrects++;
  }
  TEST_ASSERT_TRUE(measures >= 1);
  TEST_ASSERT_EQUAL_UINT64(measures, corrects);
}

static void test_events_contourCorrect_ancestorInRange(void) {
  Graph g(/*directed=*/true);
  AddAsymmetricThreadingTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  // ancestor indexes one of root's newSeparations[] slots -- one per gap
  // between children [0, childIndex] -- so it must be strictly less than
  // childIndex (SeparateAlongContours divides by
  // `childIndex - ancestor`, which would be a division by zero otherwise).
  bool sawCorrection = false;
  for (const auto &e : trace.Events()) {
    if (e.kind != Kind::ContourCorrect || !e.correctionFired)
      continue;
    sawCorrection = true;
    TEST_ASSERT_TRUE(e.ancestor < e.childIndex);
  }
  TEST_ASSERT_TRUE(sawCorrection);
}

// Every ContourMeasure must be immediately followed by exactly one
// ContourCorrect (the "measure, then correct, every single iteration, no
// exceptions" invariant the rewrite exists to guarantee) -- across every
// fixture tree in this suite that has any contour walk at all, not just
// the matched-blobs regression case.
static void test_events_everyContourMeasureHasOneImmediateContourCorrect(void) {
  Graph g(/*directed=*/true);
  AddMatchedTernaryBlobsTree(g);
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);

  const auto &events = trace.Events();
  size_t measures = 0, corrects = 0;
  for (size_t i = 0; i < events.size(); i++) {
    if (events[i].kind != Kind::ContourMeasure)
      continue;
    measures++;
    TEST_ASSERT_TRUE(i + 1 < events.size());
    TEST_ASSERT_TRUE(events[i + 1].kind == Kind::ContourCorrect);
    TEST_ASSERT_EQUAL_UINT64(events[i].vertexA, events[i + 1].vertexA);
    TEST_ASSERT_EQUAL_UINT64(events[i].vertexB, events[i + 1].vertexB);
  }
  for (size_t i = 0; i < events.size(); i++)
    if (events[i].kind == Kind::ContourCorrect)
      corrects++;
  TEST_ASSERT_TRUE(measures > 0);
  TEST_ASSERT_EQUAL_UINT64(measures, corrects);
}

// ============================================================================
// EDGE CASES
// ============================================================================

static void test_singleVertex_succeeds(void) {
  Graph g(/*directed=*/true);
  g.AddVertex();
  g.BuildLayout();

  ReingoldTilfordTrace trace(g, 0);
  trace.CalculateOffsets(0, 0);
  double origin[2] = {0.0, 0.0};
  trace.Embed(0, origin);

  double *pos = trace.GetVPosition(0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, pos[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, pos[1]);
  TEST_ASSERT_EQUAL_UINT64(0, trace.Height());

  // One SubtreeIsolated (the leaf/root itself), one EmbedStep, nothing else.
  TEST_ASSERT_EQUAL_UINT64(2, trace.Events().size());
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_construct_validTree_succeeds);
  RUN_TEST(test_construct_directedCycle_throws);
  RUN_TEST(test_construct_wrongRoot_throws);

  RUN_TEST(test_matchesReingoldTilford_binaryTree);
  RUN_TEST(test_matchesReingoldTilford_asymmetricThreadingTree);
  RUN_TEST(test_matchesReingoldTilford_matchedTernaryBlobs);
  RUN_TEST(test_matchedTernaryBlobs_contourStepsIndependentAndFinalOrderPreserved);

  RUN_TEST(test_height_reflectsRealDepth);
  RUN_TEST(test_subtreeIsolated_levelsMatchRealDepth);

  RUN_TEST(test_events_oneSubtreeIsolatedPerVertex);
  RUN_TEST(test_events_oneEmbedStepPerVertex);
  RUN_TEST(test_events_subtreeMergedOncePerInternalMerge);
  RUN_TEST(test_events_threadCreated_vertexIdsInRange);
  RUN_TEST(test_events_contourMeasure_capturesOffsetDeltas);
  RUN_TEST(test_events_contourMeasure_firesOnDeeperMerge);
  RUN_TEST(test_events_contourCorrect_ancestorInRange);
  RUN_TEST(test_events_everyContourMeasureHasOneImmediateContourCorrect);

  RUN_TEST(test_singleVertex_succeeds);

  return UNITY_END();
}
