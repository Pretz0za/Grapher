#include "ds/gvizQuadtree.h"
#include "unity/unity.h"
#include <stddef.h>

void setUp(void) {}

void tearDown(void) {}

// ============================================================================
// INITIALIZATION
// ============================================================================

void test_quadtreeInit_defaultNodesPerCell(void) {
  TEST_ASSERT_EQUAL_INT(1, GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT);
}

void test_quadtreeInit_empty(void) {
  gvizQuadtree tree;
  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, NULL, NULL, 0,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));
  TEST_ASSERT_NULL(gvizQuadtreeRoot(&tree));
  gvizQuadtreeRelease(&tree);
}

void test_quadtreeInit_singlePoint(void) {
  double points[] = {3.0, 4.0};
  double masses[] = {1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 1,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_TRUE(gvizQuadtreeNodeIsLeaf(root));
  TEST_ASSERT_EQUAL_INT(1, (int)gvizQuadtreeNodePointCount(root));
  TEST_ASSERT_EQUAL_INT(0, (int)gvizQuadtreeNodePointAt(root, 0));

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(3.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, comY);

  gvizQuadtreeRelease(&tree);
}

// ============================================================================
// SUBDIVISION
// ============================================================================

void test_quadtreeInit_subdividesPastCapacity(void) {
  double points[] = {
      -5.0, 5.0,  // NW
      5.0,  5.0,  // NE
      -5.0, -5.0, // SW
      5.0,  -5.0, // SE
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 4,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_FALSE(gvizQuadtreeNodeIsLeaf(root));
  TEST_ASSERT_EQUAL_INT(4, (int)root->mass);

  const gvizQuadtreeNode *nw = gvizQuadtreeNodeChild(root, GVIZ_QUAD_NW);
  const gvizQuadtreeNode *ne = gvizQuadtreeNodeChild(root, GVIZ_QUAD_NE);
  const gvizQuadtreeNode *sw = gvizQuadtreeNodeChild(root, GVIZ_QUAD_SW);
  const gvizQuadtreeNode *se = gvizQuadtreeNodeChild(root, GVIZ_QUAD_SE);

  TEST_ASSERT_NOT_NULL(nw);
  TEST_ASSERT_NOT_NULL(ne);
  TEST_ASSERT_NOT_NULL(sw);
  TEST_ASSERT_NOT_NULL(se);

  TEST_ASSERT_TRUE(gvizQuadtreeNodeIsLeaf(nw));
  TEST_ASSERT_EQUAL_INT(1, (int)gvizQuadtreeNodePointCount(nw));
  TEST_ASSERT_EQUAL_INT(0, (int)gvizQuadtreeNodePointAt(nw, 0));

  TEST_ASSERT_TRUE(gvizQuadtreeNodeIsLeaf(ne));
  TEST_ASSERT_EQUAL_INT(1, (int)gvizQuadtreeNodePointCount(ne));
  TEST_ASSERT_EQUAL_INT(1, (int)gvizQuadtreeNodePointAt(ne, 0));

  TEST_ASSERT_TRUE(gvizQuadtreeNodeIsLeaf(sw));
  TEST_ASSERT_EQUAL_INT(1, (int)gvizQuadtreeNodePointCount(sw));
  TEST_ASSERT_EQUAL_INT(2, (int)gvizQuadtreeNodePointAt(sw, 0));

  TEST_ASSERT_TRUE(gvizQuadtreeNodeIsLeaf(se));
  TEST_ASSERT_EQUAL_INT(1, (int)gvizQuadtreeNodePointCount(se));
  TEST_ASSERT_EQUAL_INT(3, (int)gvizQuadtreeNodePointAt(se, 0));

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, comX);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, comY);

  gvizQuadtreeRelease(&tree);
}

void test_quadtreeInit_respectsNodesPerCell(void) {
  double points[] = {
      1.0, 1.0,
      2.0, 2.0,
      3.0, 3.0,
  };
  double masses[] = {1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(0, gvizQuadtreeInit(&tree, points, masses, 3, 3));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_TRUE(gvizQuadtreeNodeIsLeaf(root));
  TEST_ASSERT_EQUAL_INT(3, (int)gvizQuadtreeNodePointCount(root));
  TEST_ASSERT_EQUAL_INT(3, (int)root->mass);

  gvizQuadtreeRelease(&tree);
}

void test_quadtreeInit_duplicatePointsDoNotInfiniteLoop(void) {
  double points[] = {
      2.0, 2.0,
      2.0, 2.0,
      2.0, 2.0,
  };
  double masses[] = {1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 3,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_INT(3, (int)root->mass);

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comY);

  gvizQuadtreeRelease(&tree);
}

// ============================================================================
// CENTER OF MASS
// ============================================================================

void test_quadtreeCenterOfMass_isMeanOfAllPoints(void) {
  double points[] = {
      0.0, 0.0,
      10.0, 0.0,
      0.0, 10.0,
      10.0, 10.0,
      4.0, 4.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 5,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);

  TEST_ASSERT_EQUAL_DOUBLE(24.0 / 5.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(24.0 / 5.0, comY);
  TEST_ASSERT_EQUAL_INT(5, (int)root->mass);

  gvizQuadtreeRelease(&tree);
}

// ============================================================================
// REBUILD (arena reuse)
// ============================================================================

void test_quadtreeRebuild_reflectsNewPositions(void) {
  double before[] = {
      -5.0, 5.0,
      5.0,  5.0,
      -5.0, -5.0,
      5.0,  -5.0,
  };
  double after[] = {
      100.0, 100.0,
      101.0, 100.0,
      100.0, 101.0,
      101.0, 101.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, before, masses, 4,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));
  TEST_ASSERT_FALSE(gvizQuadtreeNodeIsLeaf(gvizQuadtreeRoot(&tree)));

  TEST_ASSERT_EQUAL_INT(0, gvizQuadtreeRebuild(&tree, after, masses, 4));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_INT(4, (int)root->mass);

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(100.5, comX);
  TEST_ASSERT_EQUAL_DOUBLE(100.5, comY);

  gvizQuadtreeRelease(&tree);
}

void test_quadtreeRebuild_reusesArenaBlocks(void) {
  double points[] = {
      -5.0, 5.0,
      5.0,  5.0,
      -5.0, -5.0,
      5.0,  -5.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 4,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  size_t blocksAfterInit = tree.nodeBlocks.count;
  TEST_ASSERT_GREATER_THAN(0, (int)blocksAfterInit);

  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_EQUAL_INT(0, gvizQuadtreeRebuild(&tree, points, masses, 4));
    TEST_ASSERT_EQUAL_INT((int)blocksAfterInit, (int)tree.nodeBlocks.count);
    TEST_ASSERT_EQUAL_INT((int)blocksAfterInit, (int)tree.pointBlocks.count);
  }

  gvizQuadtreeRelease(&tree);
}

void test_quadtreeRebuild_overflowBuffersDoNotAccumulate(void) {
  double duplicates[] = {
      2.0, 2.0,
      2.0, 2.0,
      2.0, 2.0,
      2.0, 2.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, duplicates, masses, 4,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));
  size_t overflowAfterFirstBuild = tree.overflowBuffers.count;
  TEST_ASSERT_GREATER_THAN(0, (int)overflowAfterFirstBuild);

  TEST_ASSERT_EQUAL_INT(0, gvizQuadtreeRebuild(&tree, duplicates, masses, 4));
  TEST_ASSERT_EQUAL_INT((int)overflowAfterFirstBuild,
                        (int)tree.overflowBuffers.count);

  gvizQuadtreeRelease(&tree);
}

// ============================================================================
// WEIGHTED MASS
// ============================================================================

void test_quadtreeMass_weightedCenterOfMassSkewsTowardHeavierPoint(void) {
  double points[] = {
      0.0, 0.0,
      10.0, 0.0,
  };
  double masses[] = {1.0, 3.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 2,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, root->mass);

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(7.5, comX);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, comY);

  gvizQuadtreeRelease(&tree);
}

void test_quadtreeMass_isSumOfWeightsNotPointCount(void) {
  double points[] = {
      2.0, 2.0,
      2.0, 2.0,
  };
  double masses[] = {5.0, 10.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 2,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_DOUBLE(15.0, root->mass);

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comY);

  gvizQuadtreeRelease(&tree);
}

void test_quadtreeMass_zeroMassPointsDoNotDivideByZero(void) {
  double points[] = {
      0.0, 0.0,
      5.0, 5.0,
      10.0, 10.0,
  };
  double masses[] = {0.0, 0.0, 2.0};
  gvizQuadtree tree;

  TEST_ASSERT_EQUAL_INT(
      0, gvizQuadtreeInit(&tree, points, masses, 3,
                         GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT));

  const gvizQuadtreeNode *root = gvizQuadtreeRoot(&tree);
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, root->mass);

  double comX, comY;
  gvizQuadtreeNodeCenterOfMass(root, &comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(10.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(10.0, comY);

  gvizQuadtreeRelease(&tree);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_quadtreeInit_defaultNodesPerCell);
  RUN_TEST(test_quadtreeInit_empty);
  RUN_TEST(test_quadtreeInit_singlePoint);

  RUN_TEST(test_quadtreeInit_subdividesPastCapacity);
  RUN_TEST(test_quadtreeInit_respectsNodesPerCell);
  RUN_TEST(test_quadtreeInit_duplicatePointsDoNotInfiniteLoop);

  RUN_TEST(test_quadtreeCenterOfMass_isMeanOfAllPoints);

  RUN_TEST(test_quadtreeRebuild_reflectsNewPositions);
  RUN_TEST(test_quadtreeRebuild_reusesArenaBlocks);
  RUN_TEST(test_quadtreeRebuild_overflowBuffersDoNotAccumulate);

  RUN_TEST(test_quadtreeMass_weightedCenterOfMassSkewsTowardHeavierPoint);
  RUN_TEST(test_quadtreeMass_isSumOfWeightsNotPointCount);
  RUN_TEST(test_quadtreeMass_zeroMassPointsDoNotDivideByZero);

  return UNITY_END();
}
