// Port of tests/ds/gvizQuadtreeTests.c against gviz::QuadTree.
//
// The old C tests reach directly into gvizQuadtree's public struct fields
// (root->mass, tree.nodeBlocks.count, tree.overflowBuffers.count) since it's
// a plain C struct. gviz::QuadTree keeps those as private implementation
// detail, so the equivalent coverage here goes through the accessors added
// for exactly this purpose: Node::Mass() and QuadTree::NodeBlockCount()/
// OverflowBufferCount() (mirroring the precedent set by
// Subgraph::VertexCapacity() for exposing an amortized-growth contract).
//
// One scenario is new, not a 1:1 port: test_quadtreeRebuild_nodePointersStableAcrossRebuilds.
// The old suite verifies arena reuse only indirectly (block counts don't
// grow across repeated rebuilds); it never asserts that a specific Node*
// handed out by one build is still the same address -- and still describes
// the same node of the tree -- after subsequent rebuilds. That is exactly
// the invariant the arena design exists to guarantee (see QuadTree.hpp's
// class comment), so it's worth asserting directly rather than only
// inferring it from block counts.

#include "QuadTree.hpp"

#include "unity/unity.h"

using gviz::QuadTree;

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// INITIALIZATION
// ============================================================================

static void test_quadtreeInit_defaultNodesPerCell(void) {
  TEST_ASSERT_EQUAL_INT(1, (int)QuadTree::kNodesPerCellDefault);
}

static void test_quadtreeInit_empty(void) {
  QuadTree tree(nullptr, nullptr, 0, QuadTree::kNodesPerCellDefault);
  TEST_ASSERT_NULL(tree.Root());
}

static void test_quadtreeInit_singlePoint(void) {
  double points[] = {3.0, 4.0};
  double masses[] = {1.0};

  QuadTree tree(points, masses, 1, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_TRUE(root->IsLeaf());
  TEST_ASSERT_EQUAL_INT(1, (int)root->PointCount());
  TEST_ASSERT_EQUAL_INT(0, (int)root->PointAt(0));

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(3.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, comY);
}

// ============================================================================
// SUBDIVISION
// ============================================================================

static void test_quadtreeInit_subdividesPastCapacity(void) {
  double points[] = {
      -5.0, 5.0,  // NW
      5.0,  5.0,  // NE
      -5.0, -5.0, // SW
      5.0,  -5.0, // SE
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};

  QuadTree tree(points, masses, 4, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_FALSE(root->IsLeaf());
  TEST_ASSERT_EQUAL_INT(4, (int)root->Mass());

  const QuadTree::Node *nw = root->Child(QuadTree::Quadrant::NW);
  const QuadTree::Node *ne = root->Child(QuadTree::Quadrant::NE);
  const QuadTree::Node *sw = root->Child(QuadTree::Quadrant::SW);
  const QuadTree::Node *se = root->Child(QuadTree::Quadrant::SE);

  TEST_ASSERT_NOT_NULL(nw);
  TEST_ASSERT_NOT_NULL(ne);
  TEST_ASSERT_NOT_NULL(sw);
  TEST_ASSERT_NOT_NULL(se);

  TEST_ASSERT_TRUE(nw->IsLeaf());
  TEST_ASSERT_EQUAL_INT(1, (int)nw->PointCount());
  TEST_ASSERT_EQUAL_INT(0, (int)nw->PointAt(0));

  TEST_ASSERT_TRUE(ne->IsLeaf());
  TEST_ASSERT_EQUAL_INT(1, (int)ne->PointCount());
  TEST_ASSERT_EQUAL_INT(1, (int)ne->PointAt(0));

  TEST_ASSERT_TRUE(sw->IsLeaf());
  TEST_ASSERT_EQUAL_INT(1, (int)sw->PointCount());
  TEST_ASSERT_EQUAL_INT(2, (int)sw->PointAt(0));

  TEST_ASSERT_TRUE(se->IsLeaf());
  TEST_ASSERT_EQUAL_INT(1, (int)se->PointCount());
  TEST_ASSERT_EQUAL_INT(3, (int)se->PointAt(0));

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, comX);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, comY);
}

static void test_quadtreeInit_respectsNodesPerCell(void) {
  double points[] = {
      1.0, 1.0,
      2.0, 2.0,
      3.0, 3.0,
  };
  double masses[] = {1.0, 1.0, 1.0};

  QuadTree tree(points, masses, 3, 3);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_TRUE(root->IsLeaf());
  TEST_ASSERT_EQUAL_INT(3, (int)root->PointCount());
  TEST_ASSERT_EQUAL_INT(3, (int)root->Mass());
}

static void test_quadtreeInit_duplicatePointsDoNotInfiniteLoop(void) {
  double points[] = {
      2.0, 2.0,
      2.0, 2.0,
      2.0, 2.0,
  };
  double masses[] = {1.0, 1.0, 1.0};

  QuadTree tree(points, masses, 3, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_INT(3, (int)root->Mass());

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comY);
}

// ============================================================================
// CENTER OF MASS
// ============================================================================

static void test_quadtreeCenterOfMass_isMeanOfAllPoints(void) {
  double points[] = {
      0.0,  0.0,
      10.0, 0.0,
      0.0,  10.0,
      10.0, 10.0,
      4.0,  4.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0, 1.0};

  QuadTree tree(points, masses, 5, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  double comX, comY;
  root->CenterOfMass(&comX, &comY);

  TEST_ASSERT_EQUAL_DOUBLE(24.0 / 5.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(24.0 / 5.0, comY);
  TEST_ASSERT_EQUAL_INT(5, (int)root->Mass());
}

// ============================================================================
// REBUILD (arena reuse)
// ============================================================================

static void test_quadtreeRebuild_reflectsNewPositions(void) {
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

  QuadTree tree(before, masses, 4, QuadTree::kNodesPerCellDefault);
  TEST_ASSERT_FALSE(tree.Root()->IsLeaf());

  tree.Rebuild(after, masses, 4);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_INT(4, (int)root->Mass());

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(100.5, comX);
  TEST_ASSERT_EQUAL_DOUBLE(100.5, comY);
}

static void test_quadtreeRebuild_reusesArenaBlocks(void) {
  double points[] = {
      -5.0, 5.0,
      5.0,  5.0,
      -5.0, -5.0,
      5.0,  -5.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};

  QuadTree tree(points, masses, 4, QuadTree::kNodesPerCellDefault);

  size_t blocksAfterInit = tree.NodeBlockCount();
  TEST_ASSERT_GREATER_THAN(0, (int)blocksAfterInit);

  for (int i = 0; i < 5; i++) {
    tree.Rebuild(points, masses, 4);
    TEST_ASSERT_EQUAL_INT((int)blocksAfterInit, (int)tree.NodeBlockCount());
  }
}

static void test_quadtreeRebuild_overflowBuffersDoNotAccumulate(void) {
  double duplicates[] = {
      2.0, 2.0,
      2.0, 2.0,
      2.0, 2.0,
      2.0, 2.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};

  QuadTree tree(duplicates, masses, 4, QuadTree::kNodesPerCellDefault);
  size_t overflowAfterFirstBuild = tree.OverflowBufferCount();
  TEST_ASSERT_GREATER_THAN(0, (int)overflowAfterFirstBuild);

  tree.Rebuild(duplicates, masses, 4);
  TEST_ASSERT_EQUAL_INT((int)overflowAfterFirstBuild, (int)tree.OverflowBufferCount());
}

static void test_quadtreeRebuild_nodePointersStableAcrossRebuilds(void) {
  double points[] = {
      -5.0, 5.0,
      5.0,  5.0,
      -5.0, -5.0,
      5.0,  -5.0,
  };
  double masses[] = {1.0, 1.0, 1.0, 1.0};

  QuadTree tree(points, masses, 4, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  const QuadTree::Node *nw = root->Child(QuadTree::Quadrant::NW);
  const QuadTree::Node *se = root->Child(QuadTree::Quadrant::SE);
  TEST_ASSERT_NOT_NULL(nw);
  TEST_ASSERT_NOT_NULL(se);

  // Same point count/topology every time, so ArenaAlloc hands out nodes in
  // the exact same block/offset sequence on every rebuild -- root, NW, and
  // SE must therefore come back at the very same addresses, not merely
  // equal-valued nodes at new addresses.
  for (int i = 0; i < 5; i++) {
    tree.Rebuild(points, masses, 4);
    TEST_ASSERT_EQUAL_PTR(root, tree.Root());
    TEST_ASSERT_EQUAL_PTR(nw, tree.Root()->Child(QuadTree::Quadrant::NW));
    TEST_ASSERT_EQUAL_PTR(se, tree.Root()->Child(QuadTree::Quadrant::SE));
  }

  // The stable addresses still describe the correct, current tree -- not
  // stale data left over from a previous build.
  TEST_ASSERT_EQUAL_INT(4, (int)tree.Root()->Mass());
  TEST_ASSERT_TRUE(nw->IsLeaf());
  TEST_ASSERT_EQUAL_INT(0, (int)nw->PointAt(0));
  TEST_ASSERT_TRUE(se->IsLeaf());
  TEST_ASSERT_EQUAL_INT(3, (int)se->PointAt(0));
}

// ============================================================================
// WEIGHTED MASS
// ============================================================================

static void test_quadtreeMass_weightedCenterOfMassSkewsTowardHeavierPoint(void) {
  double points[] = {
      0.0,  0.0,
      10.0, 0.0,
  };
  double masses[] = {1.0, 3.0};

  QuadTree tree(points, masses, 2, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, root->Mass());

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(7.5, comX);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, comY);
}

static void test_quadtreeMass_isSumOfWeightsNotPointCount(void) {
  double points[] = {
      2.0, 2.0,
      2.0, 2.0,
  };
  double masses[] = {5.0, 10.0};

  QuadTree tree(points, masses, 2, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_DOUBLE(15.0, root->Mass());

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, comY);
}

static void test_quadtreeMass_zeroMassPointsDoNotDivideByZero(void) {
  double points[] = {
      0.0,  0.0,
      5.0,  5.0,
      10.0, 10.0,
  };
  double masses[] = {0.0, 0.0, 2.0};

  QuadTree tree(points, masses, 3, QuadTree::kNodesPerCellDefault);

  const QuadTree::Node *root = tree.Root();
  TEST_ASSERT_NOT_NULL(root);
  TEST_ASSERT_EQUAL_DOUBLE(2.0, root->Mass());

  double comX, comY;
  root->CenterOfMass(&comX, &comY);
  TEST_ASSERT_EQUAL_DOUBLE(10.0, comX);
  TEST_ASSERT_EQUAL_DOUBLE(10.0, comY);
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
  RUN_TEST(test_quadtreeRebuild_nodePointersStableAcrossRebuilds);

  RUN_TEST(test_quadtreeMass_weightedCenterOfMassSkewsTowardHeavierPoint);
  RUN_TEST(test_quadtreeMass_isSumOfWeightsNotPointCount);
  RUN_TEST(test_quadtreeMass_zeroMassPointsDoNotDivideByZero);

  return UNITY_END();
}
