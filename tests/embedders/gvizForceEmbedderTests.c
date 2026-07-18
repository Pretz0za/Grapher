#include "ds/gvizGraph.h"
#include "ds/gvizQuadtree.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizFRPairwiseEmbedder.h"
#include "embedders/gvizForceEmbedder.h"
#include "unity/unity.h"
#include "utils/graphs.h"
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

void test_forceEmbedder_init_release_lifecycle(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 10; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int i = 0; i < 9; i++)
    gvizGraphAddEdge(&g, i, i + 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(10, (int)s.vertexCount);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_init_rejectsNonDimension2(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(-2, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 3));

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_configure_keepsOrOverrides(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));

  double defaultEdgeLength = s.edgeLength;
  double defaultBoxExtent = s.boxExtent;

  gvizForceEmbedderConfigure(&s, 0, 0);
  TEST_ASSERT_EQUAL_DOUBLE(defaultEdgeLength, s.edgeLength);
  TEST_ASSERT_EQUAL_DOUBLE(defaultBoxExtent, s.boxExtent);

  gvizForceEmbedderConfigure(&s, 25.0, 100.0);
  TEST_ASSERT_EQUAL_DOUBLE(25.0, s.edgeLength);
  TEST_ASSERT_EQUAL_DOUBLE(100.0, s.boxExtent);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_configureHeat_keepsOrOverrides(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));

  double defaultR = s.heatR;
  double defaultS = s.heatS;

  gvizForceEmbedderConfigureHeat(&s, 0, 0);
  TEST_ASSERT_EQUAL_DOUBLE(defaultR, s.heatR);
  TEST_ASSERT_EQUAL_DOUBLE(defaultS, s.heatS);

  gvizForceEmbedderConfigureHeat(&s, 0.3, 2.0);
  TEST_ASSERT_EQUAL_DOUBLE(0.3, s.heatR);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, s.heatS);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_configureBarnesHut_keepsOrOverrides(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));

  TEST_ASSERT_EQUAL_DOUBLE(GVIZ_FORCE_EMBEDDER_THETA_DEFAULT, s.theta);
  TEST_ASSERT_EQUAL(GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT, (int)s.nodesPerCell);

  gvizForceEmbedderConfigureBarnesHut(&s, 0, 0);
  TEST_ASSERT_EQUAL_DOUBLE(GVIZ_FORCE_EMBEDDER_THETA_DEFAULT, s.theta);
  TEST_ASSERT_EQUAL(GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT, (int)s.nodesPerCell);

  gvizForceEmbedderConfigureBarnesHut(&s, 0.3, 4);
  TEST_ASSERT_EQUAL_DOUBLE(0.3, s.theta);
  TEST_ASSERT_EQUAL(4, (int)s.nodesPerCell);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_begin_positionsWithinBoxAndTreeBuilt(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 20; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int i = 0; i < 19; i++)
    gvizGraphAddEdge(&g, i, i + 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 42));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  for (int i = 0; i < 20; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
    TEST_ASSERT_TRUE(p[0] >= -s.boxExtent && p[0] <= s.boxExtent);
    TEST_ASSERT_TRUE(p[1] >= -s.boxExtent && p[1] <= s.boxExtent);
  }

  TEST_ASSERT_NOT_NULL(gvizQuadtreeRoot(&s.quadtree));

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* An edge pulls its endpoints together when they start far apart. */
void test_forceEmbedder_edgeAttracts(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-50.0, 0.0};
  double p1[2] = {50.0, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  gvizForceEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(after < before);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* Two unconnected vertices push apart when they start close together. */
void test_forceEmbedder_nonEdgeRepels(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-1.0, 0.0};
  double p1[2] = {1.0, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  gvizForceEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(after > before);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* An edge pair placed much closer than edgeLength still nets repulsion: the
 * corrected standard model always applies repulsion, and at short range
 * f_r(d)=k^2/d dominates f_a(d)=d^2/k. Under the old exclusive model this
 * pair would only ever feel attraction, so this fails against that code. */
void test_forceEmbedder_edgeRepelsWhenVeryClose(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-0.5, 0.0};
  double p1[2] = {0.5, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  gvizForceEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(after > before);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_run_convergesOrStopsAtMaxIters(void) {
  gvizGraph g = build_random_connected_graph(40, 0.05, 123);
  gvizGraphBuildLayout(&g);
  gvizSubgraph sg = gvizSubgraphCreateFull(&g);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, sg, 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 7));

  int rounds = gvizForceEmbedderRun(&s, 300, 1e-6);
  TEST_ASSERT_TRUE(rounds > 0);
  TEST_ASSERT_TRUE(rounds <= 300);

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  for (size_t i = 0; i < s.vertexCount; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, s.vertices[i]);
    TEST_ASSERT_TRUE(isfinite(p[0]));
    TEST_ASSERT_TRUE(isfinite(p[1]));
  }
  TEST_ASSERT_TRUE(isfinite(s.lastMaxDisplacement));

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

void test_forceEmbedder_begin_restartRebuildsQuadtree(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 12; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int i = 0; i < 11; i++)
    gvizGraphAddEdge(&g, i, i + 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double firstPositions[12 * 2];
  for (int i = 0; i < 12; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
    firstPositions[2 * i] = p[0];
    firstPositions[2 * i + 1] = p[1];
  }

  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 2));

  int anyDifferent = 0;
  for (int i = 0; i < 12; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
    if (fabs(p[0] - firstPositions[2 * i]) > 1e-9 ||
        fabs(p[1] - firstPositions[2 * i + 1]) > 1e-9)
      anyDifferent = 1;
  }
  TEST_ASSERT_TRUE(anyDifferent);
  TEST_ASSERT_NOT_NULL(gvizQuadtreeRoot(&s.quadtree));

  double maxDisp = gvizForceEmbedderStep(&s);
  TEST_ASSERT_TRUE(isfinite(maxDisp));

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* Required cross-check against gvizFRPairwiseEmbedder: with the same seed and
 * a tiny opening angle (nodesPerCell = 1), the Barnes-Hut approximation
 * should closely reproduce brute-force FR's raw force *direction* for one
 * round on a small graph. Catches mass-weighting/opening-angle-convention
 * bugs that would otherwise silently produce a plausible-looking but wrong
 * layout. Compares direction rather than the applied displacement itself,
 * since gvizForceEmbedder's per-vertex heat and gvizFRPairwiseEmbedder's
 * global temperature scale a round's step to unrelated magnitudes. */
void test_forceEmbedder_matchesFRPairwiseForTinyTheta(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 4);
  gvizGraphAddEdge(&g, 4, 5);
  gvizGraphAddEdge(&g, 5, 0);
  gvizGraphAddEdge(&g, 0, 3);

  gvizFRPairwiseState pw;
  TEST_ASSERT_EQUAL(0, gvizFRPairwiseEmbedderInit(&pw, makeFullSubgraph(&g), 2));
  gvizFRPairwiseEmbedderBegin(&pw, 99);
  gvizEmbeddedGraph *pwEg = (gvizEmbeddedGraph *)&pw;
  double before[6][2];
  for (int i = 0; i < 6; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(pwEg, (size_t)i);
    before[i][0] = p[0];
    before[i][1] = p[1];
  }
  gvizFRPairwiseEmbedderStep(&pw);

  gvizForceEmbedderState bh;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&bh, makeFullSubgraph(&g), 2));
  gvizForceEmbedderConfigureBarnesHut(&bh, 0.05, 1);
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&bh, 99));
  gvizEmbeddedGraph *bhEg = (gvizEmbeddedGraph *)&bh;
  for (int i = 0; i < 6; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(bhEg, (size_t)i);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, before[i][0], p[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, before[i][1], p[1]);
  }
  gvizForceEmbedderStep(&bh);

  // Loose tolerance: gvizForceEmbedderStep also subtracts the mean
  // displacement across all vertices (removeNetTranslation), which
  // gvizFRPairwiseEmbedder does not do, so individual directions are only
  // approximately comparable, not identical.
  double tolerance = 0.15;
  for (int i = 0; i < 6; i++) {
    double *pp = gvizEmbeddedGraphGetVPosition(pwEg, (size_t)i);
    double *bp = gvizEmbeddedGraphGetVPosition(bhEg, (size_t)i);

    double pwDx = pp[0] - before[i][0], pwDy = pp[1] - before[i][1];
    double bhDx = bp[0] - before[i][0], bhDy = bp[1] - before[i][1];
    double pwNrm = sqrt(pwDx * pwDx + pwDy * pwDy);
    double bhNrm = sqrt(bhDx * bhDx + bhDy * bhDy);
    TEST_ASSERT_TRUE(pwNrm > 1e-9);
    TEST_ASSERT_TRUE(bhNrm > 1e-9);

    TEST_ASSERT_DOUBLE_WITHIN(tolerance, pwDx / pwNrm, bhDx / bhNrm);
    TEST_ASSERT_DOUBLE_WITHIN(tolerance, pwDy / pwNrm, bhDy / bhNrm);
  }

  gvizFRPairwiseEmbedderRelease(&pw);
  gvizForceEmbedderRelease(&bh);
  gvizGraphRelease(&g);
}

/* Exercises the quadtree's coincident-point overflow path through a full
 * Barnes-Hut round without crashing. */
void test_forceEmbedder_step_handlesCoincidentPositions(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 4, 5);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2));
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 3));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double origin[2] = {0.0, 0.0};
  for (int i = 0; i < 6; i++)
    gvizEmbeddedGraphSetVPosition(eg, (size_t)i, origin);

  double maxDisp = gvizForceEmbedderStep(&s);
  TEST_ASSERT_TRUE(isfinite(maxDisp));

  for (int i = 0; i < 6; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
    TEST_ASSERT_TRUE(isfinite(p[0]));
    TEST_ASSERT_TRUE(isfinite(p[1]));
  }

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_forceEmbedder_init_release_lifecycle);
  RUN_TEST(test_forceEmbedder_init_rejectsNonDimension2);
  RUN_TEST(test_forceEmbedder_configure_keepsOrOverrides);
  RUN_TEST(test_forceEmbedder_configureHeat_keepsOrOverrides);
  RUN_TEST(test_forceEmbedder_configureBarnesHut_keepsOrOverrides);
  RUN_TEST(test_forceEmbedder_begin_positionsWithinBoxAndTreeBuilt);
  RUN_TEST(test_forceEmbedder_edgeAttracts);
  RUN_TEST(test_forceEmbedder_nonEdgeRepels);
  RUN_TEST(test_forceEmbedder_edgeRepelsWhenVeryClose);
  RUN_TEST(test_forceEmbedder_run_convergesOrStopsAtMaxIters);
  RUN_TEST(test_forceEmbedder_begin_restartRebuildsQuadtree);
  RUN_TEST(test_forceEmbedder_matchesFRPairwiseForTinyTheta);
  RUN_TEST(test_forceEmbedder_step_handlesCoincidentPositions);
  return UNITY_END();
}
