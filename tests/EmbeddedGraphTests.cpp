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
//
// New coverage added beyond the old suite: an explicit
// test_actions_invokeUnknownIsSafeNoOp, since the "no-op on unknown action
// name" contract is called out by name in EmbeddedGraph.hpp's doc comment
// as depended-upon behavior and deserves its own assertion rather than
// riding along inside test_actions_registerFindInvoke.

#include "EmbeddedGraph.hpp"

#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cstdio>
#include <vector>

using gviz::Graph;
using gviz::Subgraph;
using gviz::layout::Action;
using gviz::layout::ActionPayload;
using gviz::layout::DrawEdgePolicy;
using gviz::layout::EmbeddedGraph;
using gviz::layout::StatChartKind;
using gviz::layout::StatSeries;

void setUp(void) {}
void tearDown(void) {}

// A path graph 0-1-...-(n-1), embedded over a FULL subgraph (static shape).
static EmbeddedGraph MakeFullEmbedding(Graph &g, size_t nvertices, size_t dim) {
  for (size_t i = 0; i < nvertices; i++)
    g.AddVertex();
  for (size_t i = 0; i + 1 < nvertices; i++)
    g.AddEdge(i, i + 1, 1.0);
  g.BuildLayout();
  return EmbeddedGraph(Subgraph::CreateFull(g), dim);
}

// Same path graph, but over a VERTEX-INDUCED subgraph with every existing
// vertex shown -- the recommended shape for a dynamic (growing) embedding;
// see EmbeddedGraph.hpp's GROWTH & SYNC section.
static EmbeddedGraph MakeInducedEmbedding(Graph &g, size_t nvertices, size_t dim) {
  for (size_t i = 0; i < nvertices; i++)
    g.AddVertex();
  for (size_t i = 0; i + 1 < nvertices; i++)
    g.AddEdge(i, i + 1, 1.0);

  Subgraph sg = Subgraph::CreateVertexInduced(g);
  for (size_t i = 0; i < nvertices; i++)
    sg.ShowVertex(i);
  return EmbeddedGraph(std::move(sg), dim);
}

// ============================================================================
// BULK ACCESSORS
// ============================================================================

static void test_accessors_dimAndPositionCount(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 5, 3);

  TEST_ASSERT_EQUAL_UINT64(3, eg.Dim());
  TEST_ASSERT_EQUAL_UINT64(5, eg.PositionCount());
  TEST_ASSERT_EQUAL_UINT64(5, eg.Structure().VertexCount());
}

static void test_accessors_positionsSpanMatchesGetVPosition(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 5, 3);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);
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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

  // Depended-upon behavior (EmbeddedGraph.hpp's InvokeAction doc comment):
  // invoking an unregistered name is a safe no-op, not an error/throw, so a
  // front-end can bind actions before an embedder that implements them
  // exists.
  TEST_ASSERT_FALSE(eg.InvokeAction("no.such.action", nullptr));
}

static void test_actions_replaceAndRemove(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

  TEST_ASSERT_TRUE(eg.StatAppend("test.auto", 3.0));
  const StatSeries *s = eg.FindStatSeries("test.auto");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_TRUE(StatChartKind::Line == s->kind);
  TEST_ASSERT_EQUAL_UINT64(1, s->samples.size());
  TEST_ASSERT_EQUAL_PTR(s, eg.StatSeriesAt(0));
}

static void test_stats_clearKeepsSeriesRegistered(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);

  TEST_ASSERT_EQUAL_UINT64(0, eg.DrawMaskRevision());
  TEST_ASSERT_TRUE(eg.IsVertexVisible(0));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(3));
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(0, 1));
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(2, 3));
}

static void test_drawMask_vertexFilterAndNoEdges(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);
  eg.DrawMaskHideVertex(0);
  eg.DrawMaskHideVertex(3);

  eg.SetDrawMaskEdgePolicy(DrawEdgePolicy::None);
  TEST_ASSERT_EQUAL_UINT64(1, eg.DrawMaskRevision());
  TEST_ASSERT_FALSE(eg.IsVertexVisible(0));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(1));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(2));
  TEST_ASSERT_FALSE(eg.IsVertexVisible(3));
  TEST_ASSERT_FALSE(eg.IsEdgeVisible(1, 2));
}

static void test_drawMask_edgesIfBothVisible(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);
  eg.DrawMaskHideVertex(3);

  eg.SetDrawMaskEdgePolicy(DrawEdgePolicy::IfBothVisible);
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(0, 1));
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(1, 2));
  TEST_ASSERT_FALSE(eg.IsEdgeVisible(2, 3));

  eg.ResetDrawMask();
  TEST_ASSERT_TRUE(eg.IsEdgeVisible(2, 3));
}

static void test_drawMask_clearAndNotify(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 20, 2);

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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);
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
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);
  const char *path = "gviz_test_embedding_mismatch_cxx.tmp";
  TEST_ASSERT_TRUE(eg.SaveEmbedding("test", path));

  Graph g2(false);
  EmbeddedGraph eg2 = MakeFullEmbedding(g2, 5, 2); // different vertex count
  TEST_ASSERT_FALSE(eg2.LoadEmbedding(path));
  TEST_ASSERT_FALSE(eg.LoadEmbedding("no/such/file"));

  remove(path);
}

// ============================================================================
// HIGHLIGHT
// ============================================================================

static void test_highlight_setClearOwnership(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 4, 2);

  TEST_ASSERT_FALSE(eg.HasHighlight());
  TEST_ASSERT_NULL(eg.GetHighlight());

  Subgraph hl = Subgraph::CreateEmpty(g);
  hl.ShowVertex(1);
  hl.ShowVertex(2);
  eg.SetHighlight(std::move(hl)); // takes ownership

  TEST_ASSERT_TRUE(eg.HasHighlight());
  const Subgraph *got = eg.GetHighlight();
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_TRUE(got->HasVertex(1));
  TEST_ASSERT_FALSE(got->HasVertex(0));

  // Replacing releases the old highlight (checked by ASan builds).
  Subgraph hl2 = Subgraph::CreateEmpty(g);
  hl2.ShowVertex(3);
  eg.SetHighlight(std::move(hl2));
  TEST_ASSERT_TRUE(eg.GetHighlight()->HasVertex(3));

  eg.ClearHighlight();
  TEST_ASSERT_FALSE(eg.HasHighlight());
  eg.ClearHighlight(); // double clear is a no-op
}

// ============================================================================
// GROWTH & SYNC (commit semantics)
// ============================================================================

// A vertex added directly to the Graph is invisible everywhere on the
// embedding -- membership, draw mask, position count, accessors -- until
// Sync() commits it; the commit admits it with a zeroed position, preserves
// existing positions, and bumps the draw mask revision.
static void test_sync_commitsNewVertex(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeInducedEmbedding(g, 3, 2);
  TEST_ASSERT_TRUE(eg.Sync());   // first commit
  TEST_ASSERT_FALSE(eg.Sync()); // now a no-op

  double preset[2] = {5.0, 6.0};
  eg.SetVPosition(1, preset);

  TEST_ASSERT_EQUAL_UINT64(3, g.AddVertex());

  TEST_ASSERT_EQUAL_UINT64(3, eg.PositionCount());
  TEST_ASSERT_FALSE(eg.Structure().HasVertex(3));
  TEST_ASSERT_FALSE(eg.IsVertexVisible(3));
  TEST_ASSERT_EQUAL_UINT64(0, eg.OutDegree(3));

  uint64_t rev = eg.DrawMaskRevision();
  TEST_ASSERT_TRUE(eg.Sync());
  TEST_ASSERT_EQUAL_UINT64(rev + 1, eg.DrawMaskRevision());

  TEST_ASSERT_EQUAL_UINT64(4, eg.PositionCount());
  TEST_ASSERT_TRUE(eg.Structure().HasVertex(3));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(3));

  double *p = eg.GetVPosition(3);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, p[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, p[1]);
  double *p1 = eg.GetVPosition(1);
  TEST_ASSERT_EQUAL_DOUBLE(5.0, p1[0]);
  TEST_ASSERT_EQUAL_DOUBLE(6.0, p1[1]);
}

// An edge added directly to the Graph stays out of the synced adjacency
// accessors until Sync() commits it, then shows up symmetrically from both
// endpoints on an undirected graph.
static void test_sync_commitsNewEdge_accessorsSymmetric(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeInducedEmbedding(g, 3, 2); // path 0-1-2
  TEST_ASSERT_TRUE(eg.Sync());

  TEST_ASSERT_EQUAL_UINT64(1, eg.OutDegree(0));
  TEST_ASSERT_EQUAL_UINT64(2, eg.OutDegree(1));
  TEST_ASSERT_EQUAL_UINT64(0, eg.InDegree(1)); // undirected

  g.AddEdge(0, 2, 1.0);
  TEST_ASSERT_EQUAL_UINT64(1, eg.OutDegree(0)); // deferred

  TEST_ASSERT_TRUE(eg.Sync());
  TEST_ASSERT_EQUAL_UINT64(2, eg.OutDegree(0));
  TEST_ASSERT_EQUAL_UINT64(2, eg.OutDegree(2));

  auto nbrs0 = eg.OutNeighbors(0);
  TEST_ASSERT_EQUAL_UINT64(2, nbrs0.size());
  TEST_ASSERT_TRUE(nbrs0[0] == 2 || nbrs0[1] == 2);
  auto nbrs2 = eg.OutNeighbors(2);
  TEST_ASSERT_EQUAL_UINT64(2, nbrs2.size());
  TEST_ASSERT_TRUE(nbrs2[0] == 0 || nbrs2[1] == 0);
}

// Directed snapshots expose out- and in-edges separately: the frontend can
// ask both "who does v point at" and "who points at v" for any synced
// vertex.
static void test_sync_directedInOutAccessors(void) {
  Graph g(true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(2, 1, 1.0);

  Subgraph sg = Subgraph::CreateVertexInduced(g);
  for (size_t i = 0; i < 3; i++)
    sg.ShowVertex(i);

  EmbeddedGraph eg(std::move(sg), 2);
  TEST_ASSERT_TRUE(eg.Sync());

  TEST_ASSERT_EQUAL_UINT64(1, eg.OutDegree(0));
  TEST_ASSERT_EQUAL_UINT64(0, eg.OutDegree(1));
  TEST_ASSERT_EQUAL_UINT64(0, eg.InDegree(0));
  TEST_ASSERT_EQUAL_UINT64(2, eg.InDegree(1));

  auto in = eg.InNeighbors(1);
  TEST_ASSERT_EQUAL_UINT64(2, in.size());
  TEST_ASSERT_TRUE((in[0] == 0 && in[1] == 2) || (in[0] == 2 && in[1] == 0));
}

// Querying anything about a not-yet-committed vertex must be safe and
// empty -- no out-of-bounds reads (ASan builds verify), no phantom
// membership.
static void test_sync_uncommittedVertexQueriesAreSafe(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeInducedEmbedding(g, 2, 2);
  TEST_ASSERT_TRUE(eg.Sync());

  TEST_ASSERT_EQUAL_UINT64(2, g.AddVertex());
  g.AddEdge(2, 0, 1.0);

  size_t newId = 2;
  TEST_ASSERT_FALSE(eg.Structure().HasVertex(newId));
  TEST_ASSERT_FALSE(eg.IsVertexVisible(newId));
  TEST_ASSERT_FALSE(eg.IsEdgeVisible(newId, 0));
  TEST_ASSERT_EQUAL_UINT64(0, eg.Structure().Degree(newId));
  TEST_ASSERT_EQUAL_UINT64(0, eg.OutDegree(newId));
  TEST_ASSERT_EQUAL_UINT64(0, eg.InDegree(newId));
  auto nbrs = eg.OutNeighbors(newId);
  TEST_ASSERT_TRUE(nbrs.empty());

  TEST_ASSERT_TRUE(eg.Sync());
  TEST_ASSERT_EQUAL_UINT64(1, eg.OutDegree(newId));
}

// The whole grow-and-commit cycle on a vertex-induced embedding never
// touches the graph's shared edge layout.
static void test_sync_vertexInducedEmbeddingNeverBuildsLayout(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeInducedEmbedding(g, 3, 2);
  TEST_ASSERT_FALSE(g.HasLayout());
  TEST_ASSERT_TRUE(eg.Sync());
  TEST_ASSERT_FALSE(g.HasLayout());

  TEST_ASSERT_EQUAL_UINT64(3, g.AddVertex());
  g.AddEdge(3, 0, 1.0);
  TEST_ASSERT_TRUE(eg.Sync());

  TEST_ASSERT_EQUAL_UINT64(4, eg.PositionCount());
  TEST_ASSERT_TRUE(eg.Structure().HasEdge(3, 0));
  TEST_ASSERT_EQUAL_UINT64(1, eg.OutDegree(3));
  TEST_ASSERT_FALSE(g.HasLayout());
}

// Sync on a full-subgraph embedding still admits new vertices (static
// full-subgraph consumers keep working if their graph grows); new EDGES are
// not auto-shown in a full subgraph's explicitly-managed edge subset, which
// is why dynamic embeddings use the vertex-induced shape.
static void test_sync_fullSubgraphAdmitsNewVertices(void) {
  Graph g(false);
  EmbeddedGraph eg = MakeFullEmbedding(g, 3, 2);
  TEST_ASSERT_TRUE(eg.Structure().IsFull());
  TEST_ASSERT_TRUE(eg.Sync());

  TEST_ASSERT_EQUAL_UINT64(3, g.AddVertex());
  TEST_ASSERT_EQUAL_UINT64(3, eg.PositionCount());

  TEST_ASSERT_TRUE(eg.Sync());
  TEST_ASSERT_EQUAL_UINT64(4, eg.PositionCount());
  TEST_ASSERT_TRUE(eg.Structure().HasVertex(3));
  TEST_ASSERT_TRUE(eg.IsVertexVisible(3));
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

  RUN_TEST(test_highlight_setClearOwnership);

  RUN_TEST(test_sync_commitsNewVertex);
  RUN_TEST(test_sync_commitsNewEdge_accessorsSymmetric);
  RUN_TEST(test_sync_directedInOutAccessors);
  RUN_TEST(test_sync_uncommittedVertexQueriesAreSafe);
  RUN_TEST(test_sync_vertexInducedEmbeddingNeverBuildsLayout);
  RUN_TEST(test_sync_fullSubgraphAdmitsNewVertices);

  return UNITY_END();
}
