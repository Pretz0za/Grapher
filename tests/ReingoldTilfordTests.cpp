// Port of tests/embedders/gvizEmbeddedTreeTests.c against
// gviz::layout::ReingoldTilford.
//
// Scenarios reshaped from the old suite:
//   - RTInit's 0/-1 return code becomes construction succeeding or throwing
//     NotATreeError -- checked here with manual try/catch since Unity has
//     no exception-assertion macro.
//   - Manual Init/Release pairs: RAII replaces them.
//
// New coverage beyond the old suite (this port closes a gap the old C
// RTInit had -- see ReingoldTilford.hpp's constructor doc comment):
//   - test_construct_wrongRoot_throws: passing a root that isn't the
//     tree's actual root now throws NotATreeError instead of silently
//     laying out only the subtree beneath the wrong root.
//   - test_construct_multiRoot_throws / test_construct_sharedParent_throws:
//     the other two ways gviz::search::IsTree reports InvalidStructure,
//     not exercised by the old suite (which only tried a directed cycle
//     and an undirected graph).
//   - test_construct_singleVertex / test_embed_skewedThenBranching: edge
//     cases called out explicitly in this port's task brief.

#include "ReingoldTilford.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Tree.hpp"
#include "unity/unity.h"

#include <cmath>
#include <cstdio>

using gviz::Graph;
using gviz::NotATreeError;
using gviz::layout::ReingoldTilford;
namespace search = gviz::search;

void setUp(void) {}
void tearDown(void) {}

// 7-vertex complete binary tree:
//           0
//         /   \
//        1     2
//       / \   / \
//      3   4 5   6
static void AddBinaryTreeDepth2(Graph &g) {
  for (int i = 0; i < 7; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(1, 4, 1.0);
  g.AddEdge(2, 5, 1.0);
  g.AddEdge(2, 6, 1.0);
}

static void AddDirectedCycle(Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 0, 1.0);
}

static void AddUndirectedTriangle(Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
}

static void AddRootWithTwoLeaves(Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
}

static void AddPath4(Graph &g) {
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
}

// root(0) -> 1, 2, 3 ; 2 -> 4, 5
static void AddWiderTree(Graph &g) {
  for (int i = 0; i < 6; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 3, 1.0);
  g.AddEdge(2, 4, 1.0);
  g.AddEdge(2, 5, 1.0);
}

// A long single-child chain (0->1->2->3) that only branches at its tail
// (3->4, 3->5) -- exercises both the "single-child chain keeps constant x"
// path and the "branching still separates leaves" path in one tree.
static void AddSkewedThenBranching(Graph &g) {
  for (int i = 0; i < 6; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 4, 1.0);
  g.AddEdge(3, 5, 1.0);
}

static void AddTwoRoots(Graph &g) {
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(2, 3, 1.0);
}

static void AddSharedParent(Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 2, 1.0);
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

static void test_construct_validBinaryTree_succeeds(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  TEST_ASSERT_EQUAL_UINT64(7, rt.PositionCount());
}

static void test_construct_directedCycle_throws(void) {
  Graph g(/*directed=*/true);
  AddDirectedCycle(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilford rt(g, 0);
    (void)rt;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_construct_undirectedGraph_throws(void) {
  Graph g(/*directed=*/false);
  AddUndirectedTriangle(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilford rt(g, 0);
    (void)rt;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_construct_multiRoot_throws(void) {
  Graph g(/*directed=*/true);
  AddTwoRoots(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilford rt(g, 0);
    (void)rt;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_construct_sharedParent_throws(void) {
  Graph g(/*directed=*/true);
  AddSharedParent(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilford rt(g, 0);
    (void)rt;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// New coverage: root must be the tree's actual root (see the constructor's
// doc comment -- the old C RTInit never checked this).
static void test_construct_wrongRoot_throws(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  bool threw = false;
  try {
    ReingoldTilford rt(g, 1); // vertex 1 has a parent (0); not the real root
    (void)rt;
  } catch (const NotATreeError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_construct_singleVertex_succeeds(void) {
  Graph g(/*directed=*/true);
  g.AddVertex();
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  double *pos = rt.GetVPosition(0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, pos[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, pos[1]);
}

// ============================================================================
// EMBEDDING
// ============================================================================

static void test_embed_rootWithTwoLeaves_symmetric(void) {
  Graph g(/*directed=*/true);
  AddRootWithTwoLeaves(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  double *root = rt.GetVPosition(0);
  double *c1 = rt.GetVPosition(1);
  double *c2 = rt.GetVPosition(2);

  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, root[1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, c1[1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1000.0, c2[1]);

  TEST_ASSERT_DOUBLE_WITHIN(1e-6, root[0], (c1[0] + c2[0]) / 2.0);
  TEST_ASSERT_TRUE(fabs(c1[0] - c2[0]) >= 1.0);
}

static void test_embed_binaryTreeDepth2_allFiniteAndLevels(void) {
  Graph g(/*directed=*/true);
  AddBinaryTreeDepth2(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  double *pos[7];
  for (size_t i = 0; i < 7; i++) {
    pos[i] = rt.GetVPosition(i);
    TEST_ASSERT_TRUE(std::isfinite(pos[i][0]));
    TEST_ASSERT_TRUE(std::isfinite(pos[i][1]));
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
    for (size_t j = i + 1; j < 4; j++)
      TEST_ASSERT_TRUE(fabs(pos[leaves[i]][0] - pos[leaves[j]][0]) >= 1.0);
  }
}

static void test_embed_path_constantX(void) {
  Graph g(/*directed=*/true);
  AddPath4(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  double *pos[4];
  for (size_t i = 0; i < 4; i++)
    pos[i] = rt.GetVPosition(i);

  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, pos[0][0], pos[i][0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, static_cast<double>(i) * 1000.0, pos[i][1]);
  }
}

static void test_embed_widerTree_noOverlap(void) {
  Graph g(/*directed=*/true);
  AddWiderTree(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  double *pos[6];
  for (size_t i = 0; i < 6; i++)
    pos[i] = rt.GetVPosition(i);

  // Direct children of root (1, 2, 3) pairwise distinct x.
  size_t children[3] = {1, 2, 3};
  for (size_t i = 0; i < 3; i++) {
    for (size_t j = i + 1; j < 3; j++)
      TEST_ASSERT_TRUE(fabs(pos[children[i]][0] - pos[children[j]][0]) >= 1.0);
  }

  // Leaves of the whole tree: 1, 3, 4, 5 (vertex 2 is internal).
  size_t leaves[4] = {1, 3, 4, 5};
  for (size_t i = 0; i < 4; i++) {
    TEST_ASSERT_TRUE(search::IsLeaf(g, leaves[i]));
    for (size_t j = i + 1; j < 4; j++)
      TEST_ASSERT_TRUE(fabs(pos[leaves[i]][0] - pos[leaves[j]][0]) >= 1.0);
  }
}

static void test_embed_skewedThenBranching(void) {
  Graph g(/*directed=*/true);
  AddSkewedThenBranching(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  double *pos[6];
  for (size_t i = 0; i < 6; i++)
    pos[i] = rt.GetVPosition(i);

  // The single-child chain 0-1-2-3 stays at constant x (each has exactly
  // one child, so its offset is always 0 -- see InitializeRTSubtreeRoot /
  // CombineSubtreeLeft's `if (i == 0) return`).
  for (size_t i = 0; i <= 3; i++) {
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, pos[0][0], pos[i][0]);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, static_cast<double>(i) * 1000.0, pos[i][1]);
  }

  // The two leaves under vertex 3 separate, symmetric around the chain's x.
  TEST_ASSERT_TRUE(fabs(pos[4][0] - pos[5][0]) >= 1.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, pos[3][0], (pos[4][0] + pos[5][0]) / 2.0);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4000.0, pos[4][1]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4000.0, pos[5][1]);
}

static void test_countLeaves_matchesPositionScan(void) {
  Graph g(/*directed=*/true);
  AddWiderTree(g);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  size_t scannedLeaves = 0;
  for (size_t i = 0; i < 6; i++) {
    double *p = rt.GetVPosition(i);
    TEST_ASSERT_TRUE(std::isfinite(p[0]));
    TEST_ASSERT_TRUE(std::isfinite(p[1]));
    if (search::IsLeaf(g, i))
      scannedLeaves++;
  }

  TEST_ASSERT_EQUAL_UINT64(scannedLeaves, search::CountLeaves(g, 0));
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_construct_validBinaryTree_succeeds);
  RUN_TEST(test_construct_directedCycle_throws);
  RUN_TEST(test_construct_undirectedGraph_throws);
  RUN_TEST(test_construct_multiRoot_throws);
  RUN_TEST(test_construct_sharedParent_throws);
  RUN_TEST(test_construct_wrongRoot_throws);
  RUN_TEST(test_construct_singleVertex_succeeds);

  RUN_TEST(test_embed_rootWithTwoLeaves_symmetric);
  RUN_TEST(test_embed_binaryTreeDepth2_allFiniteAndLevels);
  RUN_TEST(test_embed_path_constantX);
  RUN_TEST(test_embed_widerTree_noOverlap);
  RUN_TEST(test_embed_skewedThenBranching);
  RUN_TEST(test_countLeaves_matchesPositionScan);

  return UNITY_END();
}
