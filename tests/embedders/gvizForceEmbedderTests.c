#include "ds/gvizGraph.h"
#include "ds/gvizQuadtree.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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
  TEST_ASSERT_EQUAL(-2, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 3, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));

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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));

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

void test_forceEmbedder_configureSpeed_keepsOrOverrides(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));

  double defaultTolerance = s.jitterTolerance;

  gvizForceEmbedderConfigureSpeed(&s, 0);
  TEST_ASSERT_EQUAL_DOUBLE(defaultTolerance, s.jitterTolerance);

  gvizForceEmbedderConfigureSpeed(&s, 2.5);
  TEST_ASSERT_EQUAL_DOUBLE(2.5, s.jitterTolerance);

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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));

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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, sg, 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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

/* Cross-check within gvizForceEmbedder itself: with the same seed and model,
 * a tiny opening angle (nodesPerCell = 1) should make the Barnes-Hut
 * approximation closely reproduce exact all-pairs repulsion's raw force
 * *direction* for one round on a small graph. Catches mass-weighting/
 * opening-angle-convention bugs that would otherwise silently produce a
 * plausible-looking but wrong layout. Compares direction rather than the
 * applied displacement itself, since a few subtrees can still legitimately
 * get approximated even at a tiny theta, perturbing magnitude slightly more
 * than direction. */
void test_forceEmbedder_barnesHutApproximatesExactForTinyTheta(void) {
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

  gvizForceEmbedderState exact;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&exact, makeFullSubgraph(&g), 2,
                                             GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
  gvizForceEmbedderSetBarnesHutEnabled(&exact, 0);
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&exact, 99));
  gvizEmbeddedGraph *exactEg = (gvizEmbeddedGraph *)&exact;
  double before[6][2];
  for (int i = 0; i < 6; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(exactEg, (size_t)i);
    before[i][0] = p[0];
    before[i][1] = p[1];
  }
  gvizForceEmbedderStep(&exact);

  gvizForceEmbedderState bh;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&bh, makeFullSubgraph(&g), 2,
                                             GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
  gvizForceEmbedderConfigureBarnesHut(&bh, 0.05, 1);
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&bh, 99));
  gvizEmbeddedGraph *bhEg = (gvizEmbeddedGraph *)&bh;
  for (int i = 0; i < 6; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(bhEg, (size_t)i);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, before[i][0], p[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, before[i][1], p[1]);
  }
  gvizForceEmbedderStep(&bh);

  double tolerance = 0.1;
  for (int i = 0; i < 6; i++) {
    double *ep = gvizEmbeddedGraphGetVPosition(exactEg, (size_t)i);
    double *bp = gvizEmbeddedGraphGetVPosition(bhEg, (size_t)i);

    double exDx = ep[0] - before[i][0], exDy = ep[1] - before[i][1];
    double bhDx = bp[0] - before[i][0], bhDy = bp[1] - before[i][1];
    double exNrm = sqrt(exDx * exDx + exDy * exDy);
    double bhNrm = sqrt(bhDx * bhDx + bhDy * bhDy);
    TEST_ASSERT_TRUE(exNrm > 1e-9);
    TEST_ASSERT_TRUE(bhNrm > 1e-9);

    TEST_ASSERT_DOUBLE_WITHIN(tolerance, exDx / exNrm, bhDx / bhNrm);
    TEST_ASSERT_DOUBLE_WITHIN(tolerance, exDy / exNrm, bhDy / bhNrm);
  }

  gvizForceEmbedderRelease(&exact);
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
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2, GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
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

/* LinLog sanity check: two vertices unconnected to each other, but each with
 * nonzero degree via a shared third neighbor (so LinLog's degree-based mass
 * makes their mutual repulsion nonzero, unlike two isolated degree-0
 * vertices), placed close together should push apart. Uses a tiny opening
 * angle to keep the Barnes-Hut walk close to exact, mirroring
 * test_forceEmbedder_barnesHutApproximatesExactForTinyTheta. */
void test_forceEmbedder_linLog_nonEdgeRepelsWithNonzeroDegree(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 1, 2);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2,
                                             GVIZ_FORCE_MODEL_LINLOG));
  gvizForceEmbedderConfigureBarnesHut(&s, 0.05, 1);
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-1.0, 0.0};
  double p1[2] = {1.0, 0.0};
  double p2[2] = {0.0, 1000.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);
  gvizEmbeddedGraphSetVPosition(eg, 2, p2);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  gvizForceEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(after > before);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* With Barnes-Hut disabled, repulsion is computed exactly instead of via a
 * quadtree walk: no quadtree is ever built, and the FR edge-attracts
 * behavior should still hold. */
void test_forceEmbedder_barnesHutDisabled_stillProducesSensibleResults(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2,
                                             GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
  gvizForceEmbedderSetBarnesHutEnabled(&s, 0);
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  TEST_ASSERT_NULL(gvizQuadtreeRoot(&s.quadtree));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {-50.0, 0.0};
  double p1[2] = {50.0, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                        gvizEmbeddedGraphGetVPosition(eg, 1));
  double maxDisp = gvizForceEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0),
                       gvizEmbeddedGraphGetVPosition(eg, 1));

  TEST_ASSERT_TRUE(isfinite(maxDisp));
  TEST_ASSERT_TRUE(after < before);
  TEST_ASSERT_NULL(gvizQuadtreeRoot(&s.quadtree));

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

/* Gravity pulls a vertex toward the origin even with zero degree (gravity
 * uses raw degree, not model mass). Two isolated (degree-0) vertices are
 * placed symmetrically opposite the origin so their gravity pulls are equal
 * and opposite -- removeNetTranslation only cancels a *common* drift, so an
 * antisymmetric pull like this survives it -- and gravityK is set large
 * enough to dominate FR's default repulsion between them. */
void test_forceEmbedder_gravity_pullsVertexTowardOrigin(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizForceEmbedderState s;
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderInit(&s, makeFullSubgraph(&g), 2,
                                             GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD));
  gvizForceEmbedderConfigureGravity(&s, 100.0);
  TEST_ASSERT_EQUAL(0, gvizForceEmbedderBegin(&s, 1));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
  double p0[2] = {100.0, 0.0};
  double p1[2] = {-100.0, 0.0};
  gvizEmbeddedGraphSetVPosition(eg, 0, p0);
  gvizEmbeddedGraphSetVPosition(eg, 1, p1);

  double origin[2] = {0.0, 0.0};
  double before = dist2(gvizEmbeddedGraphGetVPosition(eg, 0), origin);
  gvizForceEmbedderStep(&s);
  double after = dist2(gvizEmbeddedGraphGetVPosition(eg, 0), origin);

  TEST_ASSERT_TRUE(after < before);

  gvizForceEmbedderRelease(&s);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_forceEmbedder_init_release_lifecycle);
  RUN_TEST(test_forceEmbedder_init_rejectsNonDimension2);
  RUN_TEST(test_forceEmbedder_configure_keepsOrOverrides);
  RUN_TEST(test_forceEmbedder_configureSpeed_keepsOrOverrides);
  RUN_TEST(test_forceEmbedder_configureBarnesHut_keepsOrOverrides);
  RUN_TEST(test_forceEmbedder_begin_positionsWithinBoxAndTreeBuilt);
  RUN_TEST(test_forceEmbedder_edgeAttracts);
  RUN_TEST(test_forceEmbedder_nonEdgeRepels);
  RUN_TEST(test_forceEmbedder_edgeRepelsWhenVeryClose);
  RUN_TEST(test_forceEmbedder_run_convergesOrStopsAtMaxIters);
  RUN_TEST(test_forceEmbedder_begin_restartRebuildsQuadtree);
  RUN_TEST(test_forceEmbedder_barnesHutApproximatesExactForTinyTheta);
  RUN_TEST(test_forceEmbedder_step_handlesCoincidentPositions);
  RUN_TEST(test_forceEmbedder_linLog_nonEdgeRepelsWithNonzeroDegree);
  RUN_TEST(test_forceEmbedder_barnesHutDisabled_stillProducesSensibleResults);
  RUN_TEST(test_forceEmbedder_gravity_pullsVertexTowardOrigin);
  return UNITY_END();
}
