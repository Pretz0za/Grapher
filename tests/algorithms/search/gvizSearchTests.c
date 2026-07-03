#include "unity/unity.h"
#include "algorithms/search/gvizBreadthFirst.h"
#include "algorithms/search/gvizDepthFirst.h"
#include "algorithms/search/gvizKNearest.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void add_path4(gvizGraph *g) {
  gvizGraphInit(g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 3);
}

static void add_triangle(gvizGraph *g) {
  gvizGraphInit(g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 0, 2);
}

void test_searchBreadthFirst_path(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  size_t distances[4];
  TEST_ASSERT_EQUAL_INT(0, gvizSearchBreadthFirst(&sg, &tree, 0, 0, distances));
  TEST_ASSERT_EQUAL_UINT64(4, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(3, gvizSubgraphEdgeCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(0, distances[0]);
  TEST_ASSERT_EQUAL_UINT64(1, distances[1]);
  TEST_ASSERT_EQUAL_UINT64(2, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(3, distances[3]);

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchBreadthFirst_maxDepth(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  size_t distances[4];
  memset(distances, 0xFF, sizeof(distances));
  TEST_ASSERT_EQUAL_INT(0, gvizSearchBreadthFirst(&sg, &tree, 0, 1, distances));
  TEST_ASSERT_EQUAL_UINT64(2, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(1, gvizSubgraphEdgeCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(0, distances[0]);
  TEST_ASSERT_EQUAL_UINT64(1, distances[1]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[3]);

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchDepthFirst_path(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  TEST_ASSERT_EQUAL_INT(0, gvizSearchDepthFirst(&sg, &tree, 0));
  TEST_ASSERT_EQUAL_UINT64(4, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(3, gvizSubgraphEdgeCount(&tree));

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_triangle(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizFoundVertex found[2];

  int count = gvizSearchKNearest(&sg, found, 2, 0, NULL);
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].dist);
  TEST_ASSERT_EQUAL_UINT64(2, found[1].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[1].dist);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_withFilter(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizVertexSubset filter = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(filter, 2);

  gvizFoundVertex found[1];
  int count = gvizSearchKNearest(&sg, found, 1, 0, filter);
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_EQUAL_UINT64(2, found[0].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].dist);

  gvizVertexSubsetRelease(filter);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_searchBreadthFirst_path);
  RUN_TEST(test_searchBreadthFirst_maxDepth);
  RUN_TEST(test_searchDepthFirst_path);
  RUN_TEST(test_searchKNearest_triangle);
  RUN_TEST(test_searchKNearest_withFilter);
  return UNITY_END();
}
