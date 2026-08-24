// Port of tests/embedders/gvizEmbeddedGraphTests.c against
// gviz::layout::EmbeddedGraph.
//
// Scenarios intentionally dropped or reshaped:
//   - Direct struct-field peeks (eg.embedding.vertexPositions == Positions(),
//     &eg.subgraph == &Structure(), g.layout == NULL): EmbeddedGraph/Graph
//     have no public fields to peek at anymore. Replaced with equivalent
//     checks through the public API (Positions().data() offset arithmetic
//     matching GetVPosition, g.HasLayout() in place of g.layout == NULL).
//   - Manual Init/Release pairs: RAII (the constructor/destructor) replaces
//     them; nothing to assert.
//   - gvizEmbeddedGraphInit's 0/-1 return code: the constructor either
//     produces a valid object or throws std::bad_alloc; there is no
//     "constructed but invalid" state to check.
//   - Highlight subgraph tests: EmbeddedGraph no longer holds a highlight
//     (removed outright as part of the GraphLike/DenseIndex refactor -- it
//     was frontend/presentation state, not this library's concern; see
//     CLAUDE.md's EmbeddedGraph section). Nothing to port.
//   - Sync/growth (dynamic-graph) tests: EmbeddedGraph no longer supports
//     growing the underlying graph while an embedding is active (dynamic
//     mutation is out of scope for this library now -- see CLAUDE.md).
//     EmbeddedGraph is constructed with a fixed vertex count and stays that
//     size for its whole lifetime; there is no Sync()/OutNeighbors()/
//     InNeighbors() surface left to test.
//
// New coverage added beyond the old suite: an explicit
// test_actions_invokeUnknownIsSafeNoOp, since the "no-op on unknown action
// name" contract is called out by name in EmbeddedGraph.hpp's doc comment
// as depended-upon behavior and deserves its own assertion rather than
// riding along inside test_actions_registerFindInvoke.

#include "EmbeddedGraph.hpp"

#include "unity/unity.h"

#include <cstdio>
#include <vector>

using gviz::layout::Action;
using gviz::layout::ActionPayload;
using gviz::layout::DrawEdgePolicy;
using gviz::layout::EmbeddedGraph;
using gviz::layout::StatChartKind;
using gviz::layout::StatSeries;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// BULK ACCESSORS
// ============================================================================

static void test_accessors_dimAndPositionCount(void) {
  EmbeddedGraph eg(5, 3);

  TEST_ASSERT_EQUAL_UINT64(3, eg.Dim());
  TEST_ASSERT_EQUAL_UINT64(5, eg.PositionCount());
}

static void test_accessors_positionsSpanMatchesGetVPosition(void) {
  EmbeddedGraph eg(5, 3);

  double pos[3] = {1.5, -2.0, 4.0};
  eg.SetVPosition(2, pos);

  auto bulk = eg.Positions();
  TEST_ASSERT_EQUAL_UINT64(15, bulk.size()); // 5 vertices * 3 dims
  TEST_ASSERT_EQUAL_PTR(eg.GetVPosition(2), bulk.data() + 2 * 3);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, bulk[2 * 3 + 0]);
  TEST_ASSERT_EQUAL_DOUBLE(-2.0, bulk[2 * 3 + 1]);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, bulk[2 * 3 + 2]);
}

// ============================================================================
// ACTIONS
// ============================================================================

static int handlerACalls = 0;
static ActionPayload lastPayload;

static void handlerA(EmbeddedGraph &eg, void *userData, const ActionPayload &payload) {
  (void)eg;
  handlerACalls++;
  lastPayload = payload;
  if (userData)
    *static_cast<int *>(userData) += 1;
}

static void handlerB(EmbeddedGraph &eg, void *userData, const ActionPayload &payload) {
  (void)eg, (void)userData, (void)payload;
}

static void test_actions_registerFindInvoke(void) {
  EmbeddedGraph eg(3, 2);
  handlerACalls = 0;

  TEST_ASSERT_EQUAL_UINT64(0, eg.ActionCount());
  TEST_ASSERT_NULL(eg.FindAction("missing"));

  int counter = 0;
  TEST_ASSERT_TRUE(eg.AddAction("test.a", handlerA, &counter));
  TEST_ASSERT_TRUE(eg.AddAction("test.b", handlerB, nullptr));
  TEST_ASSERT_EQUAL_UINT64(2, eg.ActionCount());
  TEST_ASSERT_NOT_NULL(eg.FindAction("test.a"));

  ActionPayload payload;
  payload.worldX = 7.0;
  payload.worldY = -3.0;
  payload.iarg = 42;
  TEST_ASSERT_TRUE(eg.InvokeAction("test.a", &payload));
  TEST_ASSERT_EQUAL_INT(1, handlerACalls);
  TEST_ASSERT_EQUAL_INT(1, counter);
  TEST_ASSERT_EQUAL_DOUBLE(7.0, lastPayload.worldX);
  TEST_ASSERT_EQUAL_INT64(42, lastPayload.iarg);

  // NULL payload becomes a zeroed/default payload.
  TEST_ASSERT_TRUE(eg.InvokeAction("test.a", nullptr));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, lastPayload.worldX);
}

static void test_actions_invokeUnknownIsSafeNoOp(void) {
  EmbeddedGraph eg(3, 2);

  // Depended-upon behavior (EmbeddedGraph.hpp's InvokeAction doc comment):
  // invoking an unregistered name is a safe no-op, not an error/throw, so a
  // front-end can bind actions before an embedder that implements them
  // exists.
  TEST_ASSERT_FALSE(eg.InvokeAction("no.such.action", nullptr));
}

static void test_actions_replaceAndRemove(void) {
  EmbeddedGraph eg(3, 2);

  int c1 = 0, c2 = 0;
  eg.AddAction("test.a", handlerA, &c1);
  // Re-registering the same name replaces the handler/userData, no duplicate.
  eg.AddAction("test.a", handlerA, &c2);
  TEST_ASSERT_EQUAL_UINT64(1, eg.ActionCount());

  eg.InvokeAction("test.a", nullptr);
  TEST_ASSERT_EQUAL_INT(0, c1);
  TEST_ASSERT_EQUAL_INT(1, c2);

  TEST_ASSERT_TRUE(eg.RemoveAction("test.a"));
  TEST_ASSERT_EQUAL_UINT64(0, eg.ActionCount());
  TEST_ASSERT_FALSE(eg.RemoveAction("test.a"));
  TEST_ASSERT_FALSE(eg.InvokeAction("test.a", nullptr));
}

static void test_actions_growPastInitialCapacity(void) {
  EmbeddedGraph eg(3, 2);

  static const char *names[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i"};
  for (size_t i = 0; i < 9; i++)
    TEST_ASSERT_TRUE(eg.AddAction(names[i], handlerB, nullptr));

  TEST_ASSERT_EQUAL_UINT64(9, eg.ActionCount());
  for (size_t i = 0; i < 9; i++)
    TEST_ASSERT_NOT_NULL(eg.FindAction(names[i]));
  TEST_ASSERT_NOT_NULL(eg.ActionAt(8));
  TEST_ASSERT_NULL(eg.ActionAt(9));
}

// ============================================================================
// STATS
// ============================================================================

static void test_stats_registerAndAppend(void) {
  EmbeddedGraph eg(3, 2);

  TEST_ASSERT_EQUAL_UINT64(0, eg.StatSeriesCount());
  TEST_ASSERT_NULL(eg.FindStatSeries("missing"));
  TEST_ASSERT_NULL(eg.StatSeriesAt(0));

  StatSeries *heat = eg.AddStatSeries("test.heat", StatChartKind::LineLog);
  TEST_ASSERT_NOT_NULL(heat);
  TEST_ASSERT_TRUE(StatChartKind::LineLog == heat->kind);
  TEST_ASSERT_EQUAL_UINT64(1, eg.StatSeriesCount());

  TEST_ASSERT_TRUE(eg.StatAppend("test.heat", 1.5));
  TEST_ASSERT_TRUE(eg.StatAppend("test.heat", 0.5));

  const StatSeries *found = eg.FindStatSeries("test.heat");
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_UINT64(2, found->samples.size());
  TEST_ASSERT_EQUAL_DOUBLE(1.5, found->samples[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.5, found->samples[1]);
  TEST_ASSERT_EQUAL_UINT64(2, found->revision);
}

static void test_stats_appendAutoCreatesSeries(void) {
  EmbeddedGraph eg(3, 2);

  TEST_ASSERT_TRUE(eg.StatAppend("test.auto", 3.0));
  const StatSeries *s = eg.FindStatSeries("test.auto");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_TRUE(StatChartKind::Line == s->kind);
  TEST_ASSERT_EQUAL_UINT64(1, s->samples.size());
  TEST_ASSERT_EQUAL_PTR(s, eg.StatSeriesAt(0));
}

static void test_stats_clearKeepsSeriesRegistered(void) {
  EmbeddedGraph eg(3, 2);

  eg.StatAppend("test.s", 1.0);
  eg.StatAppend("test.s", 2.0);
  uint64_t revBefore = eg.FindStatSeries("test.s")->revision;

  eg.StatClear("test.s");
  const StatSeries *s = eg.FindStatSeries("test.s");
  TEST_ASSERT_EQUAL_UINT64(0, s->samples.size());
  TEST_ASSERT_EQUAL_UINT64(revBefore + 1, s->revision);
  TEST_ASSERT_EQUAL_UINT64(1, eg.StatSeriesCount());

  eg.StatClear("missing"); // no-op, must not crash
}

static void test_stats_growPastInitialCapacities(void) {
  EmbeddedGraph eg(3, 2);

  static const char *names[] = {"s0", "s1", "s2", "s3", "s4", "s5"};
  for (size_t i = 0; i < 6; i++)
    for (size_t k = 0; k < 200; k++)
      TEST_ASSERT_TRUE(eg.StatAppend(names[i], static_cast<double>(k)));

  TEST_ASSERT_EQUAL_UINT64(6, eg.StatSeriesCount());
  for (size_t i = 0; i < 6; i++) {
    const StatSeries *s = eg.FindStatSeries(names[i]);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_UINT64(200, s->samples.size());
    TEST_ASSERT_EQUAL_DOUBLE(199.0, s->samples[199]);
  }
}

// ============================================================================
// DRAW MASK
// ============================================================================

static void test_drawMask_defaultsShowAll(void) {
  EmbeddedGraph eg(4, 2);

  TEST_ASSERT_EQUAL_UINT64(0, eg.DrawMaskRevision());
  TEST_ASSERT_TRUE(eg.IsVertexVisible(0));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(3));
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(0, 1, /*edgeExists=*/true));
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(2, 3, /*edgeExists=*/true));
}

static void test_drawMask_vertexFilterAndNoEdges(void) {
  EmbeddedGraph eg(4, 2);
  eg.DrawMaskHideVertex(0);
  eg.DrawMaskHideVertex(3);

  eg.SetDrawMaskEdgePolicy(DrawEdgePolicy::None);
  TEST_ASSERT_EQUAL_UINT64(1, eg.DrawMaskRevision());
  TEST_ASSERT_FALSE(eg.IsVertexVisible(0));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(1));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(2));
  TEST_ASSERT_FALSE(eg.IsVertexVisible(3));
  TEST_ASSERT_FALSE(eg.IsEdgeVisible(1, 2, /*edgeExists=*/true));
}

static void test_drawMask_edgesIfBothVisible(void) {
  EmbeddedGraph eg(4, 2);
  eg.DrawMaskHideVertex(3);

  eg.SetDrawMaskEdgePolicy(DrawEdgePolicy::IfBothVisible);
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(0, 1, /*edgeExists=*/true));
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(1, 2, /*edgeExists=*/true));
  TEST_ASSERT_FALSE(eg.IsEdgeVisible(2, 3, /*edgeExists=*/true));

  eg.ResetDrawMask();
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(2, 3, /*edgeExists=*/true));
}

static void test_drawMask_clearAndNotify(void) {
  EmbeddedGraph eg(4, 2);

  uint64_t rev = eg.DrawMaskRevision();
  eg.DrawMaskClearVertices();
  // Show/Hide/Clear do not bump the revision on their own...
  TEST_ASSERT_EQUAL_UINT64(rev, eg.DrawMaskRevision());
  for (size_t v = 0; v < 4; v++)
    TEST_ASSERT_FALSE(eg.IsVertexVisible(v));

  eg.DrawMaskShowVertex(2);
  TEST_ASSERT_TRUE(eg.IsVertexVisible(2));

  // ...NotifyChanged is the batch commit.
  eg.DrawMaskNotifyChanged();
  TEST_ASSERT_EQUAL_UINT64(rev + 1, eg.DrawMaskRevision());

  TEST_ASSERT_TRUE(DrawEdgePolicy::All == eg.GetDrawMask().edgePolicy);
}

// ============================================================================
// POSITIONS
// ============================================================================

static void test_positions_setAddGet(void) {
  EmbeddedGraph eg(3, 2);

  double p[2] = {1.0, 2.0};
  eg.SetVPosition(1, p);
  double delta[2] = {0.5, -1.0};
  eg.AddVPosition(1, delta);

  double *got = eg.GetVPosition(1);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, got[0]);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, got[1]);

  // untouched vertices stay at the zero init
  double *other = eg.GetVPosition(0);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, other[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, other[1]);
}

static void test_positions_randomizeStaysInBox(void) {
  EmbeddedGraph eg(20, 2);

  eg.RandomizePositions(50.0, 1234);

  bool anyNonZero = false;
  for (size_t v = 0; v < 20; v++) {
    double *p = eg.GetVPosition(v);
    TEST_ASSERT_TRUE(p[0] >= -50.0 && p[0] <= 50.0);
    TEST_ASSERT_TRUE(p[1] >= -50.0 && p[1] <= 50.0);
    if (p[0] != 0.0 || p[1] != 0.0)
      anyNonZero = true;
  }
  TEST_ASSERT_TRUE(anyNonZero);

  // Same seed must reproduce the same placement.
  double *p0 = eg.GetVPosition(0);
  double first[2] = {p0[0], p0[1]};
  eg.RandomizePositions(50.0, 1234);
  TEST_ASSERT_EQUAL_DOUBLE(first[0], p0[0]);
  TEST_ASSERT_EQUAL_DOUBLE(first[1], p0[1]);
}

static void test_positions_saveLoadRoundtrip(void) {
  EmbeddedGraph eg(4, 2);
  eg.RandomizePositions(10.0, 77);

  double saved[4][2];
  for (size_t v = 0; v < 4; v++) {
    double *p = eg.GetVPosition(v);
    saved[v][0] = p[0];
    saved[v][1] = p[1];
  }

  const char *path = "gviz_test_embedding_cxx.tmp";
  TEST_ASSERT_TRUE(eg.SaveEmbedding("test", path));

  eg.RandomizePositions(10.0, 999); // scramble
  TEST_ASSERT_TRUE(eg.LoadEmbedding(path));

  for (size_t v = 0; v < 4; v++) {
    double *p = eg.GetVPosition(v);
    // text format stores %f (6 decimals)
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, saved[v][0], p[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, saved[v][1], p[1]);
  }

  remove(path);
}

static void test_positions_loadRejectsMismatch(void) {
  EmbeddedGraph eg(4, 2);
  const char *path = "gviz_test_embedding_mismatch_cxx.tmp";
  TEST_ASSERT_TRUE(eg.SaveEmbedding("test", path));

  EmbeddedGraph eg2(5, 2); // different vertex count
  TEST_ASSERT_FALSE(eg2.LoadEmbedding(path));
  TEST_ASSERT_FALSE(eg.LoadEmbedding("no/such/file"));

  remove(path);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_accessors_dimAndPositionCount);
  RUN_TEST(test_accessors_positionsSpanMatchesGetVPosition);

  RUN_TEST(test_actions_registerFindInvoke);
  RUN_TEST(test_actions_invokeUnknownIsSafeNoOp);
  RUN_TEST(test_actions_replaceAndRemove);
  RUN_TEST(test_actions_growPastInitialCapacity);

  RUN_TEST(test_stats_registerAndAppend);
  RUN_TEST(test_stats_appendAutoCreatesSeries);
  RUN_TEST(test_stats_clearKeepsSeriesRegistered);
  RUN_TEST(test_stats_growPastInitialCapacities);

  RUN_TEST(test_drawMask_defaultsShowAll);
  RUN_TEST(test_drawMask_vertexFilterAndNoEdges);
  RUN_TEST(test_drawMask_edgesIfBothVisible);
  RUN_TEST(test_drawMask_clearAndNotify);

  RUN_TEST(test_positions_setAddGet);
  RUN_TEST(test_positions_randomizeStaysInBox);
  RUN_TEST(test_positions_saveLoadRoundtrip);
  RUN_TEST(test_positions_loadRejectsMismatch);

  return UNITY_END();
}
