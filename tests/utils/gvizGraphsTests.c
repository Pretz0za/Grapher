#include "unity/unity.h"
#include "utils/graphs.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"

void setUp(void) {}
void tearDown(void) {}

void test_buildRectMesh_3x4(void) {
  gvizGraph g = build_rect_mesh(3, 4);
  TEST_ASSERT_EQUAL_UINT64(12, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  // 3 rows * (4-1) right edges + 4 cols * (3-1) down edges = 9 + 8 = 17
  TEST_ASSERT_EQUAL_UINT64(17, gvizGraphEdgeCount(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphRelease(&g);
}

void test_buildRectMesh_1x1(void) {
  gvizGraph g = build_rect_mesh(1, 1);
  TEST_ASSERT_EQUAL_UINT64(1, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(0, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_buildEquilateralTriMesh_depth3(void) {
  gvizGraph g = build_equilateral_tri_mesh(3);
  // (depth+1)*(depth+2)/2 = 4*5/2 = 10
  TEST_ASSERT_EQUAL_UINT64(10, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphRelease(&g);
}

void test_buildTetrahedralMesh_depth2(void) {
  gvizGraph g = build_tetrahedral_mesh(2);
  // (depth+3)*(depth+2)*(depth+1)/6 = 5*4*3/6 = 10
  TEST_ASSERT_EQUAL_UINT64(10, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphRelease(&g);
}

void test_buildMobiusStrip_4x5(void) {
  gvizGraph g = build_mobius_strip(4, 5);
  TEST_ASSERT_EQUAL_UINT64(20, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphBuildLayout(&g);
  // 4 rows * (5-1) right edges + 4*5 down/glue edges = 16 + 20 = 36
  TEST_ASSERT_EQUAL_UINT64(36, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_buildKleinBottle_4x5(void) {
  gvizGraph g = build_klein_bottle(4, 5);
  TEST_ASSERT_EQUAL_UINT64(20, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphBuildLayout(&g);
  // every vertex contributes a right/wrap edge and a down/glue edge: 2*4*5 = 40
  TEST_ASSERT_EQUAL_UINT64(40, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_buildSierpinskiCarpet_depth1(void) {
  gvizGraph g = build_sierpinski_carpet(1);
  // dim=3x3 grid with the center point removed => 8 vertices
  TEST_ASSERT_EQUAL_UINT64(8, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphRelease(&g);
}

void test_createSierpinski_depth0(void) {
  gvizGraph g = createSierpinski(0, NULL);
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_createSierpinski_depth2(void) {
  gvizGraph g = createSierpinski(2, NULL);
  // V(n) = (3^(n+1) + 3) / 2 = (27 + 3) / 2 = 15
  TEST_ASSERT_EQUAL_UINT64(15, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  // Edge count = 3 * 3^depth = 3 * 9 = 27
  TEST_ASSERT_EQUAL_UINT64(27, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_createSierpinskiTetrahedron_depth1(void) {
  gvizGraph g = createSierpinskiTetrahedron(1, NULL);
  // 4 base corners + 6 edge-midpoints introduced at depth 1 = 10
  TEST_ASSERT_EQUAL_UINT64(10, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  // 4 corner sub-tetrahedra (K4, 6 edges each), no shared edges at depth 1
  TEST_ASSERT_EQUAL_UINT64(24, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_buildRandomConnectedGraph_basic(void) {
  gvizGraph g = build_random_connected_graph(50, 0.1, 42);
  TEST_ASSERT_EQUAL_UINT64(50, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(49, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_buildRandomConnectedGraph_deterministic(void) {
  gvizGraph a = build_random_connected_graph(50, 0.1, 42);
  gvizGraph b = build_random_connected_graph(50, 0.1, 42);

  gvizGraphBuildLayout(&a);
  gvizGraphBuildLayout(&b);
  TEST_ASSERT_EQUAL_UINT64(gvizGraphEdgeCount(&a), gvizGraphEdgeCount(&b));

  gvizGraphRelease(&a);
  gvizGraphRelease(&b);
}

void test_buildRandomConnectedGraph_singleVertex(void) {
  gvizGraph g = build_random_connected_graph(1, 0.5, 1);
  TEST_ASSERT_EQUAL_UINT64(1, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(0, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_buildKnottedRectMesh_3x4(void) {
  gvizGraph g = build_knotted_rect_mesh(3, 4);
  TEST_ASSERT_EQUAL_UINT64(12, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphBuildLayout(&g);
  // base rect-mesh edges (17) plus extra knot edges, some of which are
  // parallel duplicates counted again by EdgeCount, so only a lower bound
  // is asserted here.
  TEST_ASSERT_GREATER_OR_EQUAL_UINT64(17, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_isConnected_triangle(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 0, 2);

  TEST_ASSERT_EQUAL_INT(1, isConnected(&g));

  gvizGraphRelease(&g);
}

void test_isConnected_disconnected(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(0, isConnected(&g));

  gvizGraphRelease(&g);
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
  RUN_TEST(test_createSierpinskiTetrahedron_depth1);
  RUN_TEST(test_buildRandomConnectedGraph_basic);
  RUN_TEST(test_buildRandomConnectedGraph_deterministic);
  RUN_TEST(test_buildRandomConnectedGraph_singleVertex);
  RUN_TEST(test_buildKnottedRectMesh_3x4);
  RUN_TEST(test_isConnected_triangle);
  RUN_TEST(test_isConnected_disconnected);
  return UNITY_END();
}
