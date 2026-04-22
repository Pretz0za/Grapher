#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizTutteEmbedding.h"
#include "unity/unity.h"
#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

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
 * K4 with the outer triangle pinned: after convergence vertex 3 must sit at
 * the centroid of the triangle to within a loose tolerance.
 */
void test_tutte_k4_centroid(void) {
    gvizGraph g;
    buildK4(&g);

    gvizTutteState s;
    TEST_ASSERT_EQUAL(0, gvizTutteEmbeddingInit(&s, &g, 2, 1e-8));

    size_t boundary[3] = {0, 1, 2};
    gvizTutteFixConvexPolygon(&s, boundary, 3, 100.0);
    gvizTutteEmbeddingSeedInterior(&s);

    gvizTutteEmbeddingRun(&s, 10000);

    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;
    double *p0 = gvizEmbeddedGraphGetVPosition(eg, 0);
    double *p1 = gvizEmbeddedGraphGetVPosition(eg, 1);
    double *p2 = gvizEmbeddedGraphGetVPosition(eg, 2);
    double *p3 = gvizEmbeddedGraphGetVPosition(eg, 3);

    double cx = (p0[0] + p1[0] + p2[0]) / 3.0;
    double cy = (p0[1] + p1[1] + p2[1]) / 3.0;

    TEST_ASSERT_DOUBLE_WITHIN(1e-4, cx, p3[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, cy, p3[1]);

    gvizTutteEmbeddingRelease(&s);
    gvizGraphRelease(&g);
}

/* Boundary vertex positions must be bit-exact after any number of steps. */
void test_tutte_boundary_pinned(void) {
    gvizGraph g;
    buildK4(&g);

    gvizTutteState s;
    TEST_ASSERT_EQUAL(0, gvizTutteEmbeddingInit(&s, &g, 2, 1e-8));

    size_t boundary[3] = {0, 1, 2};
    gvizTutteFixConvexPolygon(&s, boundary, 3, 50.0);
    gvizTutteEmbeddingSeedInterior(&s);

    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;

    /* Record pinned positions before any step. */
    double before[3][2];
    for (int i = 0; i < 3; i++) {
        double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
        before[i][0] = p[0];
        before[i][1] = p[1];
    }

    for (int i = 0; i < 50; i++)
        gvizTutteEmbeddingStep(&s, 0.016);

    for (int i = 0; i < 3; i++) {
        double *p = gvizEmbeddedGraphGetVPosition(eg, (size_t)i);
        TEST_ASSERT_EQUAL_DOUBLE(before[i][0], p[0]);
        TEST_ASSERT_EQUAL_DOUBLE(before[i][1], p[1]);
    }

    gvizTutteEmbeddingRelease(&s);
    gvizGraphRelease(&g);
}

/* A small grid should converge within a reasonable iteration budget. */
void test_tutte_convergence(void) {
    /* 5x5 grid, pin the 16 outer rim vertices. */
    gvizGraph g;
    gvizGraphInit(&g, 0);
    size_t L = 5, W = 5;
    for (size_t i = 0; i < L * W; i++)
        gvizGraphAddVertex(&g, NULL, NULL, NULL);
    for (size_t i = 0; i < L; i++)
        for (size_t j = 0; j < W; j++) {
            size_t idx = i * W + j;
            if (j + 1 < W) gvizGraphAddEdge(&g, idx, i * W + j + 1);
            if (i + 1 < L) gvizGraphAddEdge(&g, idx, (i + 1) * W + j);
        }

    gvizTutteState s;
    TEST_ASSERT_EQUAL(0, gvizTutteEmbeddingInit(&s, &g, 2, 1e-5));

    /* Collect rim vertices in CCW order: top, right, bottom (rev), left (rev). */
    size_t rim[16];
    size_t k = 0;
    for (size_t j = 0; j < W; j++) rim[k++] = j;           /* top */
    for (size_t i = 1; i < L; i++) rim[k++] = i * W + W - 1; /* right */
    for (size_t j = W - 1; j-- > 0;) rim[k++] = (L - 1) * W + j; /* bottom */
    for (size_t i = L - 1; i-- > 1;) rim[k++] = i * W;    /* left */

    gvizTutteFixConvexPolygon(&s, rim, k, 200.0);
    gvizTutteEmbeddingSeedInterior(&s);

    size_t iters = (size_t)gvizTutteEmbeddingRun(&s, 5000);

    TEST_ASSERT_EQUAL_INT(1, s.converged);
    TEST_ASSERT_LESS_THAN(5000, iters);

    gvizTutteEmbeddingRelease(&s);
    gvizGraphRelease(&g);
}

/*
 * Jacobi and Gauss-Seidel must converge to the same fixed point (within 1e-4).
 * GS should use fewer iterations.
 */
void test_tutte_jacobi_vs_gs(void) {
    gvizGraph g;
    buildK4(&g);

    gvizTutteState sJ, sGS;
    TEST_ASSERT_EQUAL(0, gvizTutteEmbeddingInit(&sJ, &g, 2, 1e-8));
    TEST_ASSERT_EQUAL(0, gvizTutteEmbeddingInit(&sGS, &g, 2, 1e-8));

    size_t boundary[3] = {0, 1, 2};
    gvizTutteFixConvexPolygon(&sJ, boundary, 3, 100.0);
    gvizTutteEmbeddingSeedInterior(&sJ);
    gvizTutteFixConvexPolygon(&sGS, boundary, 3, 100.0);
    gvizTutteEmbeddingSeedInterior(&sGS);

    sGS.useGaussSeidel = 1;

    gvizTutteEmbeddingRun(&sJ, 10000);
    gvizTutteEmbeddingRun(&sGS, 10000);

    gvizEmbeddedGraph *egJ = (gvizEmbeddedGraph *)&sJ;
    gvizEmbeddedGraph *egGS = (gvizEmbeddedGraph *)&sGS;

    /* Interior vertex 3 should be at the same place in both. */
    double *pJ = gvizEmbeddedGraphGetVPosition(egJ, 3);
    double *pGS = gvizEmbeddedGraphGetVPosition(egGS, 3);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, pJ[0], pGS[0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, pJ[1], pGS[1]);

    TEST_ASSERT_LESS_OR_EQUAL(sJ.iteration, sGS.iteration);

    gvizTutteEmbeddingRelease(&sJ);
    gvizTutteEmbeddingRelease(&sGS);
    gvizGraphRelease(&g);
}

/* SetBoundary with count<3 or out-of-range index must return -1. */
void test_tutte_init_validation(void) {
    gvizGraph g;
    buildK4(&g);

    gvizTutteState s;
    TEST_ASSERT_EQUAL(0, gvizTutteEmbeddingInit(&s, &g, 2, 0.0));

    /* Too few boundary vertices. */
    size_t tooFew[2] = {0, 1};
    double pos[4] = {0.0, 0.0, 1.0, 0.0};
    TEST_ASSERT_EQUAL(-1, gvizTutteEmbeddingSetBoundary(&s, tooFew, 2, pos));

    /* Out-of-range index. */
    size_t bad[3] = {0, 1, 99};
    double pos3[6] = {0};
    TEST_ASSERT_EQUAL(-1, gvizTutteEmbeddingSetBoundary(&s, bad, 3, pos3));

    gvizTutteEmbeddingRelease(&s);
    gvizGraphRelease(&g);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tutte_k4_centroid);
    RUN_TEST(test_tutte_boundary_pinned);
    RUN_TEST(test_tutte_convergence);
    RUN_TEST(test_tutte_jacobi_vs_gs);
    RUN_TEST(test_tutte_init_validation);
    return UNITY_END();
}
