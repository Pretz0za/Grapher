#include "unity/unity.h"
#include "ds/gvizGraph.h"
#include "ds/gvizTree.h"

void setUp(void) {}

void tearDown(void) {}

void test_isTree_undirected_returns_negative_two(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);

  TEST_ASSERT_EQUAL_INT(-2, gvizGraphIsTree(&g, NULL));

  gvizGraphRelease(&g);
}

void test_isTree_singleVertex(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  int parents[1];
  TEST_ASSERT_EQUAL_INT(1, gvizGraphIsTree(&g, parents));
  TEST_ASSERT_EQUAL_INT(-1, parents[0]);

  gvizGraphRelease(&g);
}

void test_isTree_path(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);

  int parents[3];
  TEST_ASSERT_EQUAL_INT(1, gvizGraphIsTree(&g, parents));
  TEST_ASSERT_EQUAL_INT(-1, parents[0]);
  TEST_ASSERT_EQUAL_INT(0, parents[1]);
  TEST_ASSERT_EQUAL_INT(1, parents[2]);

  gvizGraphRelease(&g);
}

void test_isTree_star(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 0, 3);

  int parents[4];
  TEST_ASSERT_EQUAL_INT(1, gvizGraphIsTree(&g, parents));
  TEST_ASSERT_EQUAL_INT(-1, parents[0]);
  TEST_ASSERT_EQUAL_INT(0, parents[1]);
  TEST_ASSERT_EQUAL_INT(0, parents[2]);
  TEST_ASSERT_EQUAL_INT(0, parents[3]);

  gvizGraphRelease(&g);
}

void test_isTree_twoRoots(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 2, 3);

  TEST_ASSERT_EQUAL_INT(-1, gvizGraphIsTree(&g, NULL));

  gvizGraphRelease(&g);
}

void test_isTree_inDegreeTwo(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 1, 2);

  TEST_ASSERT_EQUAL_INT(-1, gvizGraphIsTree(&g, NULL));

  gvizGraphRelease(&g);
}

void test_isTree_isolatedRootPlusCycle(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 1);

  TEST_ASSERT_EQUAL_INT(0, gvizGraphIsTree(&g, NULL));

  gvizGraphRelease(&g);
}

void test_isTree_parentsNull_noCrash(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);

  TEST_ASSERT_EQUAL_INT(1, gvizGraphIsTree(&g, NULL));

  gvizGraphRelease(&g);
}

void test_isLeaf(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);

  TEST_ASSERT_EQUAL_INT(0, gvizTreeIsLeaf(&g, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizTreeIsLeaf(&g, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizTreeIsLeaf(&g, 2));

  gvizGraphRelease(&g);
}

void test_countLeaves_smallTree(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  for (int i = 0; i < 5; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  // root(0) -> 1, 2 ; 1 -> 3, 4
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 1, 3);
  gvizGraphAddEdge(&g, 1, 4);

  TEST_ASSERT_EQUAL_INT(1, gvizGraphIsTree(&g, NULL));
  TEST_ASSERT_EQUAL_UINT64(3, gvizTreeCountLeaves(&g, 0));

  gvizGraphRelease(&g);
}

void test_countLeaves_singleLeaf(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  TEST_ASSERT_EQUAL_UINT64(1, gvizTreeCountLeaves(&g, 0));

  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_isTree_undirected_returns_negative_two);
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
