#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"

void setUp(void) {}
void tearDown(void) {}

static gvizEmbeddedGraph makeEmbedding(gvizGraph *g, size_t nvertices,
                                       size_t dim) {
  gvizGraphInitAtCapacity(g, 0, nvertices);
  for (size_t i = 0; i < nvertices; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  for (size_t i = 0; i + 1 < nvertices; i++)
    gvizGraphAddEdge(g, i, i + 1);
  gvizGraphBuildLayout(g);

  gvizEmbeddedGraph eg;
  gvizEmbeddedGraphInit(&eg, gvizSubgraphCreateFull(g), dim);
  return eg;
}

// BULK ACCESSORS: --------------------------------------------------------------

void test_accessors_matchStruct(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 5, 3);

  TEST_ASSERT_EQUAL_size_t(3, gvizEmbeddedGraphDim(&eg));
  TEST_ASSERT_EQUAL_size_t(5, gvizEmbeddedGraphPositionCount(&eg));
  TEST_ASSERT_EQUAL_PTR(eg.embedding.vertexPositions,
                        gvizEmbeddedGraphPositions(&eg));
  TEST_ASSERT_EQUAL_PTR(&eg.subgraph, gvizEmbeddedGraphStructure(&eg));

  double pos[3] = {1.5, -2.0, 4.0};
  gvizEmbeddedGraphSetVPosition(&eg, 2, pos);
  const double *bulk = gvizEmbeddedGraphPositions(&eg);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, bulk[2 * 3 + 0]);
  TEST_ASSERT_EQUAL_DOUBLE(-2.0, bulk[2 * 3 + 1]);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, bulk[2 * 3 + 2]);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

// ACTIONS: ---------------------------------------------------------------------

static int handlerACalls = 0;
static gvizActionPayload lastPayload;

static void handlerA(gvizEmbeddedGraph *eg, void *userData,
                     const gvizActionPayload *payload) {
  (void)eg;
  handlerACalls++;
  lastPayload = *payload;
  if (userData)
    *(int *)userData += 1;
}

static void handlerB(gvizEmbeddedGraph *eg, void *userData,
                     const gvizActionPayload *payload) {
  (void)eg, (void)userData, (void)payload;
}

void test_actions_registerFindInvoke(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);
  handlerACalls = 0;

  TEST_ASSERT_EQUAL_size_t(0, gvizEmbeddedGraphActionCount(&eg));
  TEST_ASSERT_NULL(gvizEmbeddedGraphFindAction(&eg, "missing"));
  TEST_ASSERT_EQUAL_INT(-1, gvizEmbeddedGraphInvokeAction(&eg, "missing", NULL));

  int counter = 0;
  TEST_ASSERT_EQUAL_INT(
      0, gvizEmbeddedGraphAddAction(&eg, "test.a", handlerA, &counter));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphAddAction(&eg, "test.b", handlerB,
                                                      NULL));
  TEST_ASSERT_EQUAL_size_t(2, gvizEmbeddedGraphActionCount(&eg));
  TEST_ASSERT_NOT_NULL(gvizEmbeddedGraphFindAction(&eg, "test.a"));

  gvizActionPayload payload = {.worldX = 7.0, .worldY = -3.0, .iarg = 42};
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphInvokeAction(&eg, "test.a",
                                                         &payload));
  TEST_ASSERT_EQUAL_INT(1, handlerACalls);
  TEST_ASSERT_EQUAL_INT(1, counter);
  TEST_ASSERT_EQUAL_DOUBLE(7.0, lastPayload.worldX);
  TEST_ASSERT_EQUAL_INT64(42, lastPayload.iarg);

  // NULL payload becomes a zeroed payload.
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphInvokeAction(&eg, "test.a", NULL));
  TEST_ASSERT_EQUAL_DOUBLE(0.0, lastPayload.worldX);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_actions_replaceAndRemove(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);
  handlerACalls = 0;

  int c1 = 0, c2 = 0;
  gvizEmbeddedGraphAddAction(&eg, "test.a", handlerA, &c1);
  // Re-registering the same name replaces the handler/userData, no duplicate.
  gvizEmbeddedGraphAddAction(&eg, "test.a", handlerA, &c2);
  TEST_ASSERT_EQUAL_size_t(1, gvizEmbeddedGraphActionCount(&eg));

  gvizEmbeddedGraphInvokeAction(&eg, "test.a", NULL);
  TEST_ASSERT_EQUAL_INT(0, c1);
  TEST_ASSERT_EQUAL_INT(1, c2);

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphRemoveAction(&eg, "test.a"));
  TEST_ASSERT_EQUAL_size_t(0, gvizEmbeddedGraphActionCount(&eg));
  TEST_ASSERT_EQUAL_INT(-1, gvizEmbeddedGraphRemoveAction(&eg, "test.a"));
  TEST_ASSERT_EQUAL_INT(-1, gvizEmbeddedGraphInvokeAction(&eg, "test.a", NULL));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_actions_growPastInitialCapacity(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  static const char *names[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i"};
  for (size_t i = 0; i < 9; i++)
    TEST_ASSERT_EQUAL_INT(
        0, gvizEmbeddedGraphAddAction(&eg, names[i], handlerB, NULL));

  TEST_ASSERT_EQUAL_size_t(9, gvizEmbeddedGraphActionCount(&eg));
  for (size_t i = 0; i < 9; i++)
    TEST_ASSERT_NOT_NULL(gvizEmbeddedGraphFindAction(&eg, names[i]));
  TEST_ASSERT_NOT_NULL(gvizEmbeddedGraphActionAt(&eg, 8));
  TEST_ASSERT_NULL(gvizEmbeddedGraphActionAt(&eg, 9));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

// STATS: -----------------------------------------------------------------------

void test_stats_registerAndAppend(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  TEST_ASSERT_EQUAL_size_t(0, gvizEmbeddedGraphStatSeriesCount(&eg));
  TEST_ASSERT_NULL(gvizEmbeddedGraphFindStatSeries(&eg, "missing"));
  TEST_ASSERT_NULL(gvizEmbeddedGraphStatSeriesAt(&eg, 0));

  gvizStatSeries *heat =
      gvizEmbeddedGraphAddStatSeries(&eg, "test.heat", GVIZ_STAT_CHART_LINE_LOG);
  TEST_ASSERT_NOT_NULL(heat);
  TEST_ASSERT_EQUAL_INT(GVIZ_STAT_CHART_LINE_LOG, heat->kind);
  TEST_ASSERT_EQUAL_size_t(1, gvizEmbeddedGraphStatSeriesCount(&eg));

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphStatAppend(&eg, "test.heat", 1.5));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphStatAppend(&eg, "test.heat", 0.5));

  const gvizStatSeries *found = gvizEmbeddedGraphFindStatSeries(&eg, "test.heat");
  TEST_ASSERT_NOT_NULL(found);
  TEST_ASSERT_EQUAL_size_t(2, found->count);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, found->samples[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.5, found->samples[1]);
  TEST_ASSERT_EQUAL_UINT64(2, found->revision);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_stats_appendAutoCreatesSeries(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphStatAppend(&eg, "test.auto", 3.0));
  const gvizStatSeries *s = gvizEmbeddedGraphFindStatSeries(&eg, "test.auto");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL_INT(GVIZ_STAT_CHART_LINE, s->kind);
  TEST_ASSERT_EQUAL_size_t(1, s->count);
  TEST_ASSERT_EQUAL_PTR(s, gvizEmbeddedGraphStatSeriesAt(&eg, 0));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_stats_clearKeepsSeriesRegistered(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  gvizEmbeddedGraphStatAppend(&eg, "test.s", 1.0);
  gvizEmbeddedGraphStatAppend(&eg, "test.s", 2.0);
  const gvizStatSeries *s = gvizEmbeddedGraphFindStatSeries(&eg, "test.s");
  uint64_t revBefore = s->revision;

  gvizEmbeddedGraphStatClear(&eg, "test.s");
  TEST_ASSERT_EQUAL_size_t(0, s->count);
  TEST_ASSERT_EQUAL_UINT64(revBefore + 1, s->revision);
  TEST_ASSERT_EQUAL_size_t(1, gvizEmbeddedGraphStatSeriesCount(&eg));

  gvizEmbeddedGraphStatClear(&eg, "missing"); // no-op, must not crash

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_stats_growPastInitialCapacities(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  static const char *names[] = {"s0", "s1", "s2", "s3", "s4", "s5"};
  for (size_t i = 0; i < 6; i++)
    for (size_t k = 0; k < 200; k++)
      TEST_ASSERT_EQUAL_INT(
          0, gvizEmbeddedGraphStatAppend(&eg, names[i], (double)k));

  TEST_ASSERT_EQUAL_size_t(6, gvizEmbeddedGraphStatSeriesCount(&eg));
  for (size_t i = 0; i < 6; i++) {
    const gvizStatSeries *s = gvizEmbeddedGraphFindStatSeries(&eg, names[i]);
    TEST_ASSERT_NOT_NULL(s);
    TEST_ASSERT_EQUAL_size_t(200, s->count);
    TEST_ASSERT_EQUAL_DOUBLE(199.0, s->samples[199]);
  }

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

// DRAW MASK: -------------------------------------------------------------------

void test_drawMask_defaultsShowAll(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);

  TEST_ASSERT_EQUAL_UINT64(0, gvizEmbeddedGraphDrawMaskRevision(&eg));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 0));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 3));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsEdgeVisible(&eg, 0, 1));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsEdgeVisible(&eg, 2, 3));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_drawMask_vertexFilterAndNoEdges(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);
  gvizEmbeddedGraphDrawMaskHideVertex(&eg, 0);
  gvizEmbeddedGraphDrawMaskHideVertex(&eg, 3);

  gvizEmbeddedGraphSetDrawMaskEdgePolicy(&eg, GVIZ_DRAW_EDGES_NONE);
  TEST_ASSERT_EQUAL_UINT64(1, gvizEmbeddedGraphDrawMaskRevision(&eg));
  TEST_ASSERT_FALSE(gvizEmbeddedGraphIsVertexVisible(&eg, 0));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 1));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 2));
  TEST_ASSERT_FALSE(gvizEmbeddedGraphIsVertexVisible(&eg, 3));
  TEST_ASSERT_FALSE(gvizEmbeddedGraphIsEdgeVisible(&eg, 1, 2));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_drawMask_edgesIfBothVisible(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);
  gvizEmbeddedGraphDrawMaskHideVertex(&eg, 3);

  gvizEmbeddedGraphSetDrawMaskEdgePolicy(&eg, GVIZ_DRAW_EDGES_IF_BOTH_VISIBLE);
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsEdgeVisible(&eg, 0, 1));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsEdgeVisible(&eg, 1, 2));
  TEST_ASSERT_FALSE(gvizEmbeddedGraphIsEdgeVisible(&eg, 2, 3));

  gvizEmbeddedGraphResetDrawMask(&eg);
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsEdgeVisible(&eg, 2, 3));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

// POSITIONS: -------------------------------------------------------------------

void test_positions_setAddGet(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  double p[2] = {1.0, 2.0};
  gvizEmbeddedGraphSetVPosition(&eg, 1, p);
  double delta[2] = {0.5, -1.0};
  gvizEmbeddedGraphAddVPosition(&eg, 1, delta);

  double *got = gvizEmbeddedGraphGetVPosition(&eg, 1);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, got[0]);
  TEST_ASSERT_EQUAL_DOUBLE(1.0, got[1]);

  // untouched vertices stay at the zero init
  double *other = gvizEmbeddedGraphGetVPosition(&eg, 0);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, other[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, other[1]);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_positions_randomizeStaysInBox(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 20, 2);

  gvizEmbeddedGraphRandomizePositions(&eg, 50.0, 1234);

  int anyNonZero = 0;
  for (size_t v = 0; v < 20; v++) {
    double *p = gvizEmbeddedGraphGetVPosition(&eg, v);
    TEST_ASSERT_TRUE(p[0] >= -50.0 && p[0] <= 50.0);
    TEST_ASSERT_TRUE(p[1] >= -50.0 && p[1] <= 50.0);
    if (p[0] != 0.0 || p[1] != 0.0)
      anyNonZero = 1;
  }
  TEST_ASSERT_TRUE(anyNonZero);

  // Same seed must reproduce the same placement.
  double first[2];
  double *p0 = gvizEmbeddedGraphGetVPosition(&eg, 0);
  first[0] = p0[0];
  first[1] = p0[1];
  gvizEmbeddedGraphRandomizePositions(&eg, 50.0, 1234);
  TEST_ASSERT_EQUAL_DOUBLE(first[0], p0[0]);
  TEST_ASSERT_EQUAL_DOUBLE(first[1], p0[1]);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_positions_saveLoadRoundtrip(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);
  gvizEmbeddedGraphRandomizePositions(&eg, 10.0, 77);

  double saved[4][2];
  for (size_t v = 0; v < 4; v++) {
    double *p = gvizEmbeddedGraphGetVPosition(&eg, v);
    saved[v][0] = p[0];
    saved[v][1] = p[1];
  }

  const char *path = "gviz_test_embedding.tmp";
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphSaveEmbedding(&eg, "test", path));

  gvizEmbeddedGraphRandomizePositions(&eg, 10.0, 999); // scramble
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphLoadEmbedding(&eg, path));

  for (size_t v = 0; v < 4; v++) {
    double *p = gvizEmbeddedGraphGetVPosition(&eg, v);
    // text format stores %f (6 decimals)
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, saved[v][0], p[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-5, saved[v][1], p[1]);
  }

  remove(path);
  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_positions_loadRejectsMismatch(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);
  const char *path = "gviz_test_embedding_mismatch.tmp";
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphSaveEmbedding(&eg, "test", path));

  gvizGraph g2;
  gvizEmbeddedGraph eg2 = makeEmbedding(&g2, 5, 2); // different vertex count
  TEST_ASSERT_EQUAL_INT(-1, gvizEmbeddedGraphLoadEmbedding(&eg2, path));
  TEST_ASSERT_EQUAL_INT(-1, gvizEmbeddedGraphLoadEmbedding(&eg, "no/such/file"));

  remove(path);
  gvizEmbeddedGraphRelease(&eg);
  gvizEmbeddedGraphRelease(&eg2);
  gvizGraphRelease(&g);
  gvizGraphRelease(&g2);
}

// HIGHLIGHT: -------------------------------------------------------------------

void test_highlight_setClearOwnership(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);

  TEST_ASSERT_FALSE(gvizEmbeddedGraphHasHighlight(&eg));
  TEST_ASSERT_NULL(gvizEmbeddedGraphGetHighlight(&eg));

  gvizSubgraph hl = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphShowVertex(&hl, 1);
  gvizSubgraphShowVertex(&hl, 2);
  gvizEmbeddedGraphSetHighlight(&eg, hl); // takes ownership

  TEST_ASSERT_TRUE(gvizEmbeddedGraphHasHighlight(&eg));
  const gvizSubgraph *got = gvizEmbeddedGraphGetHighlight(&eg);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(got, 1));
  TEST_ASSERT_FALSE(gvizSubgraphHasVertex(got, 0));

  // Replacing releases the old highlight (checked by ASAN builds).
  gvizSubgraph hl2 = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphShowVertex(&hl2, 3);
  gvizEmbeddedGraphSetHighlight(&eg, hl2);
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(gvizEmbeddedGraphGetHighlight(&eg), 3));

  gvizEmbeddedGraphClearHighlight(&eg);
  TEST_ASSERT_FALSE(gvizEmbeddedGraphHasHighlight(&eg));
  gvizEmbeddedGraphClearHighlight(&eg); // double clear is a no-op

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

// DRAW MASK BATCHING: ----------------------------------------------------------

void test_drawMask_clearAndNotify(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 4, 2);

  uint64_t rev = gvizEmbeddedGraphDrawMaskRevision(&eg);
  gvizEmbeddedGraphDrawMaskClearVertices(&eg);
  // Show/Hide/Clear do not bump the revision on their own...
  TEST_ASSERT_EQUAL_UINT64(rev, gvizEmbeddedGraphDrawMaskRevision(&eg));
  for (size_t v = 0; v < 4; v++)
    TEST_ASSERT_FALSE(gvizEmbeddedGraphIsVertexVisible(&eg, v));

  gvizEmbeddedGraphDrawMaskShowVertex(&eg, 2);
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 2));

  // ...NotifyChanged is the batch commit.
  gvizEmbeddedGraphDrawMaskNotifyChanged(&eg);
  TEST_ASSERT_EQUAL_UINT64(rev + 1, gvizEmbeddedGraphDrawMaskRevision(&eg));

  const gvizDrawMask *mask = gvizEmbeddedGraphGetDrawMask(&eg);
  TEST_ASSERT_NOT_NULL(mask);
  TEST_ASSERT_EQUAL_INT(GVIZ_DRAW_EDGES_ALL, mask->edgePolicy);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

// GROWTH (ADD VERTEX / ADD EDGE): -----------------------------------------------

void test_addVertex_growsFullEmbedding(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2);

  TEST_ASSERT_EQUAL_size_t(3, gvizEmbeddedGraphPositionCount(&eg));
  TEST_ASSERT_TRUE(gvizSubgraphIsFull(gvizEmbeddedGraphStructure(&eg)));

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphAddVertex(&eg, NULL));

  TEST_ASSERT_EQUAL_size_t(4, gvizEmbeddedGraphPositionCount(&eg));
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(gvizEmbeddedGraphStructure(&eg), 3));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 3));

  double *p = gvizEmbeddedGraphGetVPosition(&eg, 3);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, p[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, p[1]);

  // Pre-existing vertex positions must survive the growth realloc.
  double preset[2] = {5.0, 6.0};
  gvizEmbeddedGraphSetVPosition(&eg, 1, preset);
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphAddVertex(&eg, NULL));
  double *p1 = gvizEmbeddedGraphGetVPosition(&eg, 1);
  TEST_ASSERT_EQUAL_DOUBLE(5.0, p1[0]);
  TEST_ASSERT_EQUAL_DOUBLE(6.0, p1[1]);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_addEdge_visibleOnFullEmbedding(void) {
  gvizGraph g;
  gvizEmbeddedGraph eg = makeEmbedding(&g, 3, 2); // path 0-1-2, no edge 0-2
  TEST_ASSERT_FALSE(gvizEmbeddedGraphIsEdgeVisible(&eg, 0, 2));

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphAddEdge(&eg, 0, 2));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsEdgeVisible(&eg, 0, 2));
  TEST_ASSERT_TRUE(gvizSubgraphHasEdge(gvizEmbeddedGraphStructure(&eg), 0, 2));

  // Out-of-bounds endpoint fails, matching gvizGraphAddEdge.
  TEST_ASSERT_EQUAL_INT(-1, gvizEmbeddedGraphAddEdge(&eg, 0, 99));

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

void test_addVertexAndEdge_vertexInducedEmbeddingNeverBuildsLayout(void) {
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, 3);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizVertexSubsetShowVertex(vs, 1);
  gvizVertexSubsetShowVertex(vs, 2);
  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);

  gvizEmbeddedGraph eg;
  gvizEmbeddedGraphInit(&eg, sg, 2);
  TEST_ASSERT_NULL(g.layout);
  TEST_ASSERT_EQUAL_size_t(3, gvizEmbeddedGraphPositionCount(&eg));

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphAddVertex(&eg, NULL));
  TEST_ASSERT_EQUAL_size_t(4, gvizEmbeddedGraphPositionCount(&eg));
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(gvizEmbeddedGraphStructure(&eg), 3));
  TEST_ASSERT_TRUE(gvizEmbeddedGraphIsVertexVisible(&eg, 3));
  TEST_ASSERT_NULL(g.layout);

  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedGraphAddEdge(&eg, 3, 0));
  TEST_ASSERT_TRUE(gvizSubgraphHasEdge(gvizEmbeddedGraphStructure(&eg), 3, 0));
  TEST_ASSERT_NULL(g.layout);

  gvizEmbeddedGraphRelease(&eg);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_accessors_matchStruct);
  RUN_TEST(test_actions_registerFindInvoke);
  RUN_TEST(test_actions_replaceAndRemove);
  RUN_TEST(test_actions_growPastInitialCapacity);
  RUN_TEST(test_stats_registerAndAppend);
  RUN_TEST(test_stats_appendAutoCreatesSeries);
  RUN_TEST(test_stats_clearKeepsSeriesRegistered);
  RUN_TEST(test_stats_growPastInitialCapacities);
  RUN_TEST(test_drawMask_defaultsShowAll);
  RUN_TEST(test_drawMask_vertexFilterAndNoEdges);
  RUN_TEST(test_drawMask_edgesIfBothVisible);
  RUN_TEST(test_addVertex_growsFullEmbedding);
  RUN_TEST(test_addEdge_visibleOnFullEmbedding);
  RUN_TEST(test_addVertexAndEdge_vertexInducedEmbeddingNeverBuildsLayout);
  return UNITY_END();
}
