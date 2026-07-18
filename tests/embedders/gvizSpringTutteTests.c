#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizSpringTutteEmbedder.h"
#include "unity/unity.h"
#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static gvizSubgraph makeFullSubgraph(gvizGraph *g) {
    gvizGraphBuildLayout(g);
    return gvizSubgraphCreateFull(g);
}

/* Build K4: vertices 0-2 form the outer triangle, vertex 3 is the interior. */
static void buildK4(gvizGraph *g) {
    gvizGraphInit(g, 0);
    for (int i = 0; i < 4; i++)
        gvizGraphAddVertex(g, NULL, NULL, NULL);
    gvizGraphAddEdge(g, 0, 1);
    gvizGraphAddEdge(g, 0, 2);
    gvizGraphAddEdge(g, 0, 3);
    gvizGraphAddEdge(g, 1, 2);
    gvizGraphAddEdge(g, 1, 3);
    gvizGraphAddEdge(g, 2, 3);
}

/*
 * K4 with the outer triangle pinned: after convergence vertex 3 must settle
 * at the same barycentric fixed point as plain Tutte, to within a loose
 * tolerance.
 */
void test_spring_tutte_k4_centroid(void) {
    gvizGraph g;
    buildK4(&g);

    gvizSpringTutteState s;
    TEST_ASSERT_EQUAL(0, gvizSpringTutteEmbedderInit(&s, makeFullSubgraph(&g), 2, 1e-8));

    size_t boundary[3] = {0, 1, 2};
    gvizSpringTutteFixConvexPolygon(&s, boundary, 3, 100.0);
    gvizSpringTutteEmbedderSeedInterior(&s);

    gvizSpringTutteEmbedderRun(&s, 20000, 0.016);

    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
    double *p0 = gvizEmbeddedGraphGetVPosition(eg, 0);
    double *p1 = gvizEmbeddedGraphGetVPosition(eg, 1);
    double *p2 = gvizEmbeddedGraphGetVPosition(eg, 2);
    double *p3 = gvizEmbeddedGraphGetVPosition(eg, 3);

    double cx = (p0[0] + p1[0] + p2[0]) / 3.0;
    double cy = (p0[1] + p1[1] + p2[1]) / 3.0;

    TEST_ASSERT_DOUBLE_WITHIN(1e-3, cx, p3[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, cy, p3[1]);

    gvizSpringTutteEmbedderRelease(&s);
    gvizGraphRelease(&g);
}

/* Boundary vertex positions must be bit-exact after any number of steps. */
void test_spring_tutte_boundary_pinned(void) {
    gvizGraph g;
    buildK4(&g);

    gvizSpringTutteState s;
    TEST_ASSERT_EQUAL(0, gvizSpringTutteEmbedderInit(&s, makeFullSubgraph(&g), 2, 1e-8));

    size_t boundary[3] = {0, 1, 2};
    gvizSpringTutteFixConvexPolygon(&s, boundary, 3, 50.0);
    gvizSpringTutteEmbedderSeedInterior(&s);

    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;

    double before[3][2];
    for (int i = 0; i < 3; i++) {
        double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
        before[i][0] = p[0];
        before[i][1] = p[1];
    }

    for (int i = 0; i < 50; i++)
        gvizSpringTutteEmbedderStep(&s, 0.016);

    for (int i = 0; i < 3; i++) {
        double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
        TEST_ASSERT_EQUAL_DOUBLE(before[i][0], p[0]);
        TEST_ASSERT_EQUAL_DOUBLE(before[i][1], p[1]);
    }

    gvizSpringTutteEmbedderRelease(&s);
    gvizGraphRelease(&g);
}

/*
 * With default (underdamped) stiffness/damping, a vertex displaced away from
 * its equilibrium must overshoot past it at least once before settling --
 * the defining difference from plain Tutte's monotonic relaxation, which can
 * never cross its target.
 */
void test_spring_tutte_overshoots(void) {
    gvizGraph g;
    buildK4(&g);

    gvizSpringTutteState s;
    TEST_ASSERT_EQUAL(0, gvizSpringTutteEmbedderInit(&s, makeFullSubgraph(&g), 2, 1e-9));

    size_t boundary[3] = {0, 1, 2};
    gvizSpringTutteFixConvexPolygon(&s, boundary, 3, 100.0);
    gvizSpringTutteEmbedderSeedInterior(&s);

    /* Equilibrium for vertex 3 is the triangle centroid, (0, 0). Displace it
     * far away so relaxation toward equilibrium is non-trivial. */
    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
    double displaced[2] = {300.0, 0.0};
    gvizEmbeddedGraphSetVPosition(eg, 3, displaced);

    int crossedZero = 0;
    double prevX = displaced[0];
    for (int i = 0; i < 2000 && !crossedZero; i++) {
        gvizSpringTutteEmbedderStep(&s, 0.016);
        double x = gvizEmbeddedGraphGetVPosition(eg, 3)[0];
        if ((prevX > 0.0 && x < 0.0) || (prevX < 0.0 && x > 0.0))
            crossedZero = 1;
        prevX = x;
    }

    TEST_ASSERT_TRUE(crossedZero);

    gvizSpringTutteEmbedderRelease(&s);
    gvizGraphRelease(&g);
}

/* A small grid should still converge (decay to rest) within a generous
 * iteration budget despite the added inertia/oscillation. */
void test_spring_tutte_convergence(void) {
    gvizGraph g;
    size_t L = 5, W = 5;
    gvizGraphInit(&g, 0);
    for (size_t i = 0; i < L * W; i++)
        gvizGraphAddVertex(&g, NULL, NULL, NULL);
    for (size_t i = 0; i < L; i++)
        for (size_t j = 0; j < W; j++) {
            size_t idx = i * W + j;
            if (j + 1 < W) gvizGraphAddEdge(&g, idx, i * W + j + 1);
            if (i + 1 < L) gvizGraphAddEdge(&g, idx, (i + 1) * W + j);
        }

    gvizSpringTutteState s;
    TEST_ASSERT_EQUAL(0, gvizSpringTutteEmbedderInit(&s, makeFullSubgraph(&g), 2, 1e-4));
    /* Overdamped so this settles within a bounded number of steps. */
    gvizSpringTutteEmbedderConfigure(&s, 30.0, 40.0);

    size_t rim[16];
    size_t k = 0;
    for (size_t j = 0; j < W; j++) rim[k++] = j;
    for (size_t i = 1; i < L; i++) rim[k++] = i * W + W - 1;
    for (size_t j = W - 1; j-- > 0;) rim[k++] = (L - 1) * W + j;
    for (size_t i = L - 1; i-- > 1;) rim[k++] = i * W;

    gvizSpringTutteFixConvexPolygon(&s, rim, k, 200.0);
    gvizSpringTutteEmbedderSeedInterior(&s);

    size_t iters = (size_t)gvizSpringTutteEmbedderRun(&s, 20000, 0.016);

    TEST_ASSERT_EQUAL_INT(1, s.converged);
    TEST_ASSERT_LESS_THAN(20000, iters);

    gvizSpringTutteEmbedderRelease(&s);
    gvizGraphRelease(&g);
}

/* SetBoundary with count<3 or out-of-range index must return -1. */
void test_spring_tutte_init_validation(void) {
    gvizGraph g;
    buildK4(&g);

    gvizSpringTutteState s;
    TEST_ASSERT_EQUAL(0, gvizSpringTutteEmbedderInit(&s, makeFullSubgraph(&g), 2, 0.0));

    size_t tooFew[2] = {0, 1};
    double pos[4] = {0.0, 0.0, 1.0, 0.0};
    TEST_ASSERT_EQUAL(-1, gvizSpringTutteEmbedderSetBoundary(&s, tooFew, 2, pos));

    size_t bad[3] = {0, 1, 99};
    double pos3[6] = {0};
    TEST_ASSERT_EQUAL(-1, gvizSpringTutteEmbedderSetBoundary(&s, bad, 3, pos3));

    gvizSpringTutteEmbedderRelease(&s);
    gvizGraphRelease(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_spring_tutte_k4_centroid);
    RUN_TEST(test_spring_tutte_boundary_pinned);
    RUN_TEST(test_spring_tutte_overshoots);
    RUN_TEST(test_spring_tutte_convergence);
    RUN_TEST(test_spring_tutte_init_validation);
    return UNITY_END();
}
