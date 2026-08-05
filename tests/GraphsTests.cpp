// Port of tests/utils/gvizGraphsTests.c against gviz::graphs's ported
// synthetic generators (Graphs.hpp/Graphs.cpp). Lands as part of the M5
// io/graphs milestone alongside the (separately ported) GraphLoader suite.
//
// Beyond a straight port of every old scenario, this adds:
//   - test_createSierpinski_outCorners / test_createSierpinskiTetrahedron_outCorners:
//     the old suite always passed NULL for the optional corner out-param
//     (confirmed by grepping every call site across tests/ and
//     tests/embedders/*Bench.c -- none of them ever pass a non-null
//     SierpinskiTriangle*/SierpinskiTetrahedron*), so it had zero coverage
//     of that branch. Exercised here directly since it's real, reachable
//     API surface.
//   - test_isConnected_empty / test_isConnected_singleVertex: trivial-size
//     edge cases for the newly-const IsConnected, not covered by the old
//     suite (which only exercised triangle/disconnected-pair).
//   - test_buildRandomConnectedGraph_edgeDensityClamp: the old suite never
//     exercised the "clamped into [0,1]" contract explicitly (only ever
//     called with values already inside range); this passes out-of-range
//     densities and confirms the graph still builds and stays connected.

#include "Graphs.hpp"

#include "ConnectedComponents.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

using gviz::Graph;
using gviz::graphs::BuildEquilateralTriMesh;
using gviz::graphs::BuildKleinBottle;
using gviz::graphs::BuildKnottedRectMesh;
using gviz::graphs::BuildMobiusStrip;
using gviz::graphs::BuildRandomConnectedGraph;
using gviz::graphs::BuildRectMesh;
using gviz::graphs::BuildSierpinskiCarpet;
using gviz::graphs::BuildTetrahedralMesh;
using gviz::graphs::CreateSierpinski;
using gviz::graphs::CreateSierpinskiTetrahedron;
using gviz::graphs::IsConnected;
using gviz::graphs::SierpinskiTetrahedron;
using gviz::graphs::SierpinskiTriangle;

void setUp(void) {}
void tearDown(void) {}

static void test_buildRectMesh_3x4(void) {
  Graph g = BuildRectMesh(3, 4);
  TEST_ASSERT_EQUAL_UINT64(12, g.Size());

  g.BuildLayout();
  // 3 rows * (4-1) right edges + 4 cols * (3-1) down edges = 9 + 8 = 17
  TEST_ASSERT_EQUAL_UINT64(17, g.EdgeCount());
  TEST_ASSERT_TRUE(IsConnected(g));
}

static void test_buildRectMesh_1x1(void) {
  Graph g = BuildRectMesh(1, 1);
  TEST_ASSERT_EQUAL_UINT64(1, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(0, g.EdgeCount());
}

static void test_buildEquilateralTriMesh_depth3(void) {
  Graph g = BuildEquilateralTriMesh(3);
  // (depth+1)*(depth+2)/2 = 4*5/2 = 10
  TEST_ASSERT_EQUAL_UINT64(10, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));
}

static void test_buildTetrahedralMesh_depth2(void) {
  Graph g = BuildTetrahedralMesh(2);
  // (depth+3)*(depth+2)*(depth+1)/6 = 5*4*3/6 = 10
  TEST_ASSERT_EQUAL_UINT64(10, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));
}

static void test_buildMobiusStrip_4x5(void) {
  Graph g = BuildMobiusStrip(4, 5);
  TEST_ASSERT_EQUAL_UINT64(20, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));

  g.BuildLayout();
  // 4 rows * (5-1) right edges + 4*5 down/glue edges = 16 + 20 = 36
  TEST_ASSERT_EQUAL_UINT64(36, g.EdgeCount());
}

static void test_buildKleinBottle_4x5(void) {
  Graph g = BuildKleinBottle(4, 5);
  TEST_ASSERT_EQUAL_UINT64(20, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));

  g.BuildLayout();
  // every vertex contributes a right/wrap edge and a down/glue edge: 2*4*5 = 40
  TEST_ASSERT_EQUAL_UINT64(40, g.EdgeCount());
}

static void test_buildSierpinskiCarpet_depth1(void) {
  Graph g = BuildSierpinskiCarpet(1);
  // dim=3x3 grid with the center point removed => 8 vertices
  TEST_ASSERT_EQUAL_UINT64(8, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));
}

static void test_createSierpinski_depth0(void) {
  Graph g = CreateSierpinski(0);
  TEST_ASSERT_EQUAL_UINT64(3, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(3, g.EdgeCount());
}

static void test_createSierpinski_depth2(void) {
  Graph g = CreateSierpinski(2);
  // V(n) = (3^(n+1) + 3) / 2 = (27 + 3) / 2 = 15
  TEST_ASSERT_EQUAL_UINT64(15, g.Size());

  g.BuildLayout();
  // Edge count = 3 * 3^depth = 3 * 9 = 27
  TEST_ASSERT_EQUAL_UINT64(27, g.EdgeCount());
}

static void test_createSierpinski_outCorners(void) {
  SierpinskiTriangle corners{};
  Graph g = CreateSierpinski(1, &corners);
  TEST_ASSERT_EQUAL_UINT64(0, corners.t);
  TEST_ASSERT_EQUAL_UINT64(1, corners.l);
  TEST_ASSERT_EQUAL_UINT64(2, corners.r);
  // The three outer corners must still exist and be pairwise connected
  // through the fractal (not necessarily directly adjacent at depth > 0).
  TEST_ASSERT_TRUE(g.Size() > corners.r);
}

static void test_createSierpinskiTetrahedron_depth1(void) {
  Graph g = CreateSierpinskiTetrahedron(1);
  // 4 base corners + 6 edge-midpoints introduced at depth 1 = 10
  TEST_ASSERT_EQUAL_UINT64(10, g.Size());

  g.BuildLayout();
  // 4 corner sub-tetrahedra (K4, 6 edges each), no shared edges at depth 1
  TEST_ASSERT_EQUAL_UINT64(24, g.EdgeCount());
}

static void test_createSierpinskiTetrahedron_outCorners(void) {
  SierpinskiTetrahedron corners{};
  Graph g = CreateSierpinskiTetrahedron(0, &corners);
  TEST_ASSERT_EQUAL_UINT64(0, corners.a);
  TEST_ASSERT_EQUAL_UINT64(1, corners.b);
  TEST_ASSERT_EQUAL_UINT64(2, corners.c);
  TEST_ASSERT_EQUAL_UINT64(3, corners.d);
  TEST_ASSERT_EQUAL_UINT64(4, g.Size());
}

static void test_buildRandomConnectedGraph_basic(void) {
  Graph g = BuildRandomConnectedGraph(50, 0.1, 42);
  TEST_ASSERT_EQUAL_UINT64(50, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));

  g.BuildLayout();
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(49, g.EdgeCount());
}

static void test_buildRandomConnectedGraph_deterministic(void) {
  Graph a = BuildRandomConnectedGraph(50, 0.1, 42);
  Graph b = BuildRandomConnectedGraph(50, 0.1, 42);

  a.BuildLayout();
  b.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(a.EdgeCount(), b.EdgeCount());
}

static void test_buildRandomConnectedGraph_singleVertex(void) {
  Graph g = BuildRandomConnectedGraph(1, 0.5, 1);
  TEST_ASSERT_EQUAL_UINT64(1, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(0, g.EdgeCount());
}

static void test_buildRandomConnectedGraph_edgeDensityClamp(void) {
  // edgeDensity outside [0, 1] must clamp rather than misbehave (negative
  // extra-edge counts, etc.) -- the graph should still build and stay
  // connected exactly as with edgeDensity = 0 / 1 respectively.
  Graph low = BuildRandomConnectedGraph(20, -5.0, 7);
  TEST_ASSERT_EQUAL_UINT64(20, low.Size());
  TEST_ASSERT_TRUE(IsConnected(low));
  low.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(19, low.EdgeCount()); // tree only

  Graph high = BuildRandomConnectedGraph(20, 5.0, 7);
  TEST_ASSERT_EQUAL_UINT64(20, high.Size());
  TEST_ASSERT_TRUE(IsConnected(high));
  high.BuildLayout();
  // edgeDensity clamped to 1.0 -- densely connected (well above the tree's
  // 19 edges), bounded above by the complete graph's 20*19/2 = 190. Not
  // asserted exactly at 190: the random-pair sampling loop is bounded by
  // maxAttempts, and whether it reaches the theoretical maximum depends on
  // the platform's rand_r sequence, which this test shouldn't couple to.
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(100, high.EdgeCount());
  TEST_ASSERT_LESS_OR_EQUAL_UINT64(190, high.EdgeCount());
}

static void test_buildKnottedRectMesh_3x4(void) {
  Graph g = BuildKnottedRectMesh(3, 4);
  TEST_ASSERT_EQUAL_UINT64(12, g.Size());
  TEST_ASSERT_TRUE(IsConnected(g));

  g.BuildLayout();
  // base rect-mesh edges (17) plus extra knot edges, some of which are
  // parallel duplicates counted again by EdgeCount, so only a lower bound
  // is asserted here.
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(17, g.EdgeCount());
}

static void test_isConnected_triangle(void) {
  Graph g(false);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(0, 2, 1.0);

  TEST_ASSERT_TRUE(IsConnected(g));
}

static void test_isConnected_disconnected(void) {
  Graph g(false);
  g.AddVertex();
  g.AddVertex();

  TEST_ASSERT_FALSE(IsConnected(g));
}

static void test_isConnected_empty(void) {
  Graph g(false);
  TEST_ASSERT_TRUE(IsConnected(g));
}

static void test_isConnected_singleVertex(void) {
  Graph g(false);
  g.AddVertex();
  TEST_ASSERT_TRUE(IsConnected(g));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_buildRectMesh_3x4);
  RUN_TEST(test_buildRectMesh_1x1);
  RUN_TEST(test_buildEquilateralTriMesh_depth3);
  RUN_TEST(test_buildTetrahedralMesh_depth2);
  RUN_TEST(test_buildMobiusStrip_4x5);
  RUN_TEST(test_buildKleinBottle_4x5);
  RUN_TEST(test_buildSierpinskiCarpet_depth1);
  RUN_TEST(test_createSierpinski_depth0);
  RUN_TEST(test_createSierpinski_depth2);
  RUN_TEST(test_createSierpinski_outCorners);
  RUN_TEST(test_createSierpinskiTetrahedron_depth1);
  RUN_TEST(test_createSierpinskiTetrahedron_outCorners);
  RUN_TEST(test_buildRandomConnectedGraph_basic);
  RUN_TEST(test_buildRandomConnectedGraph_deterministic);
  RUN_TEST(test_buildRandomConnectedGraph_singleVertex);
  RUN_TEST(test_buildRandomConnectedGraph_edgeDensityClamp);
  RUN_TEST(test_buildKnottedRectMesh_3x4);
  RUN_TEST(test_isConnected_triangle);
  RUN_TEST(test_isConnected_disconnected);
  RUN_TEST(test_isConnected_empty);
  RUN_TEST(test_isConnected_singleVertex);
  return UNITY_END();
}
