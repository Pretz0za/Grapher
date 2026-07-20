#include "unity/unity.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "ds/gvizTree.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizEmbeddedTree.h"
#include <math.h>
#include <stdio.h>

void setUp(void) {}

void tearDown(void) {}

static void add_binaryTreeDepth2(gvizGraph *g) {
  // 7-vertex complete binary tree:
  //           0
  //         /   \
  //        1     2
  //       / \   / \
  //      3   4 5   6
  gvizGraphInit(g, 1);
  for (int i = 0; i < 7; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 0, 2);
  gvizGraphAddEdge(g, 1, 3);
  gvizGraphAddEdge(g, 1, 4);
  gvizGraphAddEdge(g, 2, 5);
  gvizGraphAddEdge(g, 2, 6);
}

static void add_directedCycle(gvizGraph *g) {
  gvizGraphInit(g, 1);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 0);
}

static void add_undirectedTriangle(gvizGraph *g) {
  gvizGraphInit(g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
}

static void add_rootWithTwoLeaves(gvizGraph *g) {
  gvizGraphInit(g, 1);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 0, 2);
}

static void add_path4(gvizGraph *g) {
  gvizGraphInit(g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 3);
}

static void add_widerTree(gvizGraph *g) {
  // root(0) -> 1, 2, 3 ; 2 -> 4, 5
  gvizGraphInit(g, 1);
  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 0, 2);
  gvizGraphAddEdge(g, 0, 3);
  gvizGraphAddEdge(g, 2, 4);
  gvizGraphAddEdge(g, 2, 5);
}

void test_RTInit_validBinaryTree_succeeds(void) {
  gvizGraph g;
  add_binaryTreeDepth2(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  int res = gvizEmbeddedTreeRTInit(&state, sg, 0);
  TEST_ASSERT_EQUAL_INT(0, res);

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_RTInit_directedCycle_fails(void) {
  gvizGraph g;
  add_directedCycle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  int res = gvizEmbeddedTreeRTInit(&state, sg, 0);
  TEST_ASSERT_EQUAL_INT(-1, res);

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_RTInit_undirectedGraph_fails(void) {
  gvizGraph g;
  add_undirectedTriangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  int res = gvizEmbeddedTreeRTInit(&state, sg, 0);
  TEST_ASSERT_EQUAL_INT(-1, res);

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_embed_rootWithTwoLeaves_symmetric(void) {
  gvizGraph g;
  add_rootWithTwoLeaves(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeRTInit(&state, sg, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeCalculateOffsets(&state, 0, 0));

  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(&state, 0, origin);

  double *root = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, 0);
  double *c1 = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, 1);
  double *c2 = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, 2);

  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, root[1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, c1[1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, c2[1]);

  TEST_ASSERT_DOUBLE_WITHIN(1e-6, root[0], (c1[0] + c2[0]) / 2.0);
  TEST_ASSERT_TRUE(fabs(c1[0] - c2[0]) >= 1.0);

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_embed_binaryTreeDepth2_allFiniteAndLevels(void) {
  gvizGraph g;
  add_binaryTreeDepth2(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeRTInit(&state, sg, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeCalculateOffsets(&state, 0, 0));

  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(&state, 0, origin);

  double *pos[7];
  for (size_t i = 0; i < 7; i++) {
    pos[i] = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, i);
    TEST_ASSERT_TRUE(isfinite(pos[i][0]));
    TEST_ASSERT_TRUE(isfinite(pos[i][1]));
  }

  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, pos[0][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, pos[1][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, pos[2][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2000.0, pos[3][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2000.0, pos[4][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2000.0, pos[5][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2000.0, pos[6][1]);

  // Leaves 3, 4, 5, 6 pairwise distinct x.
  size_t leaves[4] = {3, 4, 5, 6};
  for (size_t i = 0; i < 4; i++) {
    for (size_t j = i + 1; j < 4; j++) {
      TEST_ASSERT_TRUE(fabs(pos[leaves[i]][0] - pos[leaves[j]][0]) >= 1.0);
    }
  }

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_embed_path_constantX(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeRTInit(&state, sg, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeCalculateOffsets(&state, 0, 0));

  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(&state, 0, origin);

  double *pos[4];
  for (size_t i = 0; i < 4; i++)
    pos[i] = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, i);

  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, pos[0][0], pos[i][0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, (double)i * 1000.0, pos[i][1]);
  }

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_embed_widerTree_noOverlap(void) {
  gvizGraph g;
  add_widerTree(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeRTInit(&state, sg, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeCalculateOffsets(&state, 0, 0));

  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(&state, 0, origin);

  double *pos[6];
  for (size_t i = 0; i < 6; i++)
    pos[i] = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, i);

  // Direct children of root (1, 2, 3) pairwise distinct x.
  size_t children[3] = {1, 2, 3};
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = i + 1; j < 3; j++) {
      TEST_ASSERT_TRUE(
          fabs(pos[children[i]][0] - pos[children[j]][0]) >= 1.0);
    }
  }

  // Leaves of the whole tree: 1, 3, 4, 5 (vertex 2 is internal).
  size_t leaves[4] = {1, 3, 4, 5};
  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(1, gvizTreeIsLeaf(&g, leaves[i]));
    for (size_t j = i + 1; j < 4; j++) {
      TEST_ASSERT_TRUE(fabs(pos[leaves[i]][0] - pos[leaves[j]][0]) >= 1.0);
    }
  }

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

void test_countLeaves_matchesPositionScan(void) {
  gvizGraph g;
  add_widerTree(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizEmbeddedTree state;
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeRTInit(&state, sg, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizEmbeddedTreeCalculateOffsets(&state, 0, 0));

  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(&state, 0, origin);

  size_t scannedLeaves = 0;
  for (size_t i = 0; i < 6; i++) {
    double *p = gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, i);
    TEST_ASSERT_TRUE(isfinite(p[0]));
    TEST_ASSERT_TRUE(isfinite(p[1]));
    if (gvizTreeIsLeaf(&g, i))
      scannedLeaves++;
  }

  TEST_ASSERT_EQUAL_UINT64(scannedLeaves, gvizTreeCountLeaves(&g, 0));

  gvizEmbeddedTreeRTRelease(&state);
  gvizGraphRelease(&g);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  UNITY_BEGIN();
  RUN_TEST(test_RTInit_validBinaryTree_succeeds);
  RUN_TEST(test_RTInit_directedCycle_fails);
  RUN_TEST(test_RTInit_undirectedGraph_fails);
  RUN_TEST(test_embed_rootWithTwoLeaves_symmetric);
  RUN_TEST(test_embed_binaryTreeDepth2_allFiniteAndLevels);
  RUN_TEST(test_embed_path_constantX);
  RUN_TEST(test_embed_widerTree_noOverlap);
  RUN_TEST(test_countLeaves_matchesPositionScan);
  return UNITY_END();
}
