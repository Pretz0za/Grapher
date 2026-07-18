#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizFRPairwiseEmbedder.h"
#include "unity/unity.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

static gvizSubgraph makeFullSubgraph(gvizGraph *g) {
  gvizGraphBuildLayout(g);
  return gvizSubgraphCreateFull(g);
}

static double dist2(const double *a, const double *b) {
  double dx = a[0] - b[0];
  double dy = a[1] - b[1];
  return sqrt(dx * dx + dy * dy);
}

void test_frPairwise_begin_positionsWithinBox(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 20; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int i = 0; i < 19; i++)
    gvizGraphAddEdge(&g, i, i + 1);

  gvizFRPairwiseState s;
  TEST_ASSERT_EQUAL(0,
                    gvizFRPairwiseEmbedderInit(&s, makeFullSubgraph(&g), 2));
  gvizFRPairwiseEmbedderBegin(&s, 42);

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  for (int i = 0; i < 20; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
    TEST_ASSERT_TRUE(p[0] >= -s.boxExtent && p[0] <= s.boxExtent);
    TEST_ASSERT_TRUE(p[1] >= -s.boxExtent && p[1] <= s.boxExtent);
  }

  gvizFRPairwiseEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* An edge pulls its endpoints together when they start far apart. */
void test_frPairwise_edgeAttracts(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizFRPairwiseState s;
  TEST_ASSERT_EQUAL(0,
                    gvizFRPairwiseEmbedderInit(&s, makeFullSubgraph(&g), 2));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-50.0, 0.0};
  double p1[2] = {50.0, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  gvizFRPairwiseEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(after < before);

  gvizFRPairwiseEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* Two unconnected vertices push apart when they start close together. */
void test_frPairwise_nonEdgeRepels(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizFRPairwiseState s;
  TEST_ASSERT_EQUAL(0,
                    gvizFRPairwiseEmbedderInit(&s, makeFullSubgraph(&g), 2));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-1.0, 0.0};
  double p1[2] = {1.0, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  gvizFRPairwiseEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(after > before);

  gvizFRPairwiseEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_frPairwise_run_convergesOrStopsAtMaxIters(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 8; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int i = 0; i < 8; i++)
    gvizGraphAddEdge(&g, i, (i + 1) % 8);

  gvizFRPairwiseState s;
  TEST_ASSERT_EQUAL(0,
                    gvizFRPairwiseEmbedderInit(&s, makeFullSubgraph(&g), 2));
  gvizFRPairwiseEmbedderBegin(&s, 7);

  int rounds = gvizFRPairwiseEmbedderRun(&s, 500, 1e-6);
  TEST_ASSERT_TRUE(rounds > 0);
  TEST_ASSERT_TRUE(rounds <= 500);

  gvizFRPairwiseEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_frPairwise_begin_positionsWithinBox);
  RUN_TEST(test_frPairwise_edgeAttracts);
  RUN_TEST(test_frPairwise_nonEdgeRepels);
  RUN_TEST(test_frPairwise_run_convergesOrStopsAtMaxIters);
  return UNITY_END();
}
