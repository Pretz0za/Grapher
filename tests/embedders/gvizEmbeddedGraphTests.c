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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_accessors_matchStruct);
  RUN_TEST(test_actions_registerFindInvoke);
  RUN_TEST(test_actions_replaceAndRemove);
  RUN_TEST(test_actions_growPastInitialCapacity);
  return UNITY_END();
}
