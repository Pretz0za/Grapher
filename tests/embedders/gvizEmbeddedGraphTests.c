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
  return UNITY_END();
}
