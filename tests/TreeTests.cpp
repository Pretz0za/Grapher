// Port of tests/ds/gvizTreeTests.c against gviz::search::IsTree/IsLeaf/
// CountLeaves. The old int-coded return (1/0/-1/-2) becomes the checked
// TreeCheckResult enum -- see Tree.hpp's class comment for why "Undirected"
// stays a checked outcome instead of a thrown exception (IsTree is a
// predicate query, not a constructor, so it doesn't fit this port's
// constructor-failure exception convention).

#include "Graph.hpp"
#include "Tree.hpp"

#include "unity/unity.h"

#include <vector>

using gviz::Graph;
namespace search = gviz::search;

void setUp(void) {}
void tearDown(void) {}

static void test_isTree_undirected_returnsUndirected(void) {
  Graph g(/*directed=*/false);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);

  TEST_ASSERT_TRUE(search::IsTree(g) == search::TreeCheckResult::Undirected);
}

static void test_isTree_singleVertex(void) {
  Graph g(/*directed=*/true);
  g.AddVertex();

  std::vector<int> parents;
  TEST_ASSERT_TRUE(search::IsTree(g, &parents) == search::TreeCheckResult::IsTree);
  TEST_ASSERT_EQUAL_INT(-1, parents[0]);
}

static void test_isTree_path(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);

  std::vector<int> parents;
  TEST_ASSERT_TRUE(search::IsTree(g, &parents) == search::TreeCheckResult::IsTree);
  TEST_ASSERT_EQUAL_INT(-1, parents[0]);
  TEST_ASSERT_EQUAL_INT(0, parents[1]);
  TEST_ASSERT_EQUAL_INT(1, parents[2]);
}

static void test_isTree_star(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 3, 1.0);

  std::vector<int> parents;
  TEST_ASSERT_TRUE(search::IsTree(g, &parents) == search::TreeCheckResult::IsTree);
  TEST_ASSERT_EQUAL_INT(-1, parents[0]);
  TEST_ASSERT_EQUAL_INT(0, parents[1]);
  TEST_ASSERT_EQUAL_INT(0, parents[2]);
  TEST_ASSERT_EQUAL_INT(0, parents[3]);
}

static void test_isTree_twoRoots(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(2, 3, 1.0);

  TEST_ASSERT_TRUE(search::IsTree(g) == search::TreeCheckResult::InvalidStructure);
}

static void test_isTree_inDegreeTwo(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 2, 1.0);

  TEST_ASSERT_TRUE(search::IsTree(g) == search::TreeCheckResult::InvalidStructure);
}

static void test_isTree_isolatedRootPlusCycle(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 1, 1.0);

  TEST_ASSERT_TRUE(search::IsTree(g) == search::TreeCheckResult::NotATree);
}

static void test_isTree_parentsNull_noCrash(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);

  TEST_ASSERT_TRUE(search::IsTree(g, nullptr) == search::TreeCheckResult::IsTree);
}

static void test_isLeaf(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);

  TEST_ASSERT_FALSE(search::IsLeaf(g, 0));
  TEST_ASSERT_FALSE(search::IsLeaf(g, 1));
  TEST_ASSERT_TRUE(search::IsLeaf(g, 2));
}

static void test_countLeaves_smallTree(void) {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 5; i++)
    g.AddVertex();
  // root(0) -> 1, 2 ; 1 -> 3, 4
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(1, 4, 1.0);

  TEST_ASSERT_TRUE(search::IsTree(g) == search::TreeCheckResult::IsTree);
  TEST_ASSERT_EQUAL_UINT64(3, search::CountLeaves(g, 0));
}

static void test_countLeaves_singleLeaf(void) {
  Graph g(/*directed=*/true);
  g.AddVertex();

  TEST_ASSERT_EQUAL_UINT64(1, search::CountLeaves(g, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isTree_undirected_returnsUndirected);
  RUN_TEST(test_isTree_singleVertex);
  RUN_TEST(test_isTree_path);
  RUN_TEST(test_isTree_star);
  RUN_TEST(test_isTree_twoRoots);
  RUN_TEST(test_isTree_inDegreeTwo);
  RUN_TEST(test_isTree_isolatedRootPlusCycle);
  RUN_TEST(test_isTree_parentsNull_noCrash);
  RUN_TEST(test_isLeaf);
  RUN_TEST(test_countLeaves_smallTree);
  RUN_TEST(test_countLeaves_singleLeaf);
  return UNITY_END();
}
