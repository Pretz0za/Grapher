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
#include "Graphs.hpp"
#include "Tree.hpp"
#include "unity/unity.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <queue>
#include <utility>
#include <vector>

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

// ============================================================================
// STRESS: large random trees, no same-depth overlaps / order inversions
// ============================================================================
//
// Verifies a whole-tree invariant CalculateOffsets/Embed are supposed to
// guarantee: at any given depth, siblings-of-siblings (i.e. every vertex at
// that depth, taken in left-to-right tree order) end up strictly
// left-to-right in x with no two vertices coincident or crossed. BFS from
// the root produces exactly that left-to-right order per depth -- parents
// are dequeued in the previous level's left-to-right order, and each
// parent's own children are enqueued in their stored (already left-to-right)
// adjacency order -- so grouping GetVPosition results by BFS depth and
// checking strictly-increasing x within each group is a direct check of the
// invariant, without relying on dec_.depth (known broken -- see
// CalculateOffsets' doc comment; Embed's y, driven by real recursion depth,
// is used for the sanity cross-check instead).
static void CheckRandomTreeNoOverlap(size_t numVertices, unsigned int seed) {
  Graph g = gviz::graphs::BuildRandomConnectedGraph(numVertices, 0.0, seed, /*directed=*/true);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  std::vector<std::vector<size_t>> levels;
  std::queue<std::pair<size_t, size_t>> q; // (vertex, depth)
  q.push({0, 0});
  while (!q.empty()) {
    auto [v, depth] = q.front();
    q.pop();
    if (levels.size() <= depth)
      levels.resize(depth + 1);
    levels[depth].push_back(v);
    size_t degree = g.Degree(v);
    for (size_t i = 0; i < degree; i++)
      q.push({g.Neighbor(v, i), depth + 1});
  }

  char msg[256];
  for (size_t depth = 0; depth < levels.size(); depth++) {
    const auto &level = levels[depth];
    double prevX = 0.0;
    for (size_t i = 0; i < level.size(); i++) {
      double *p = rt.GetVPosition(level[i]);

      std::snprintf(msg, sizeof(msg), "seed=%u N=%zu depth=%zu vertex=%zu x=%f y=%f not finite",
                    seed, numVertices, depth, level[i], p[0], p[1]);
      TEST_ASSERT_TRUE_MESSAGE(std::isfinite(p[0]) && std::isfinite(p[1]), msg);

      std::snprintf(msg, sizeof(msg),
                    "seed=%u N=%zu depth=%zu vertex=%zu y=%f expected=%f "
                    "(BFS-depth grouping disagrees with Embed's own recursion depth)",
                    seed, numVertices, depth, level[i], p[1], static_cast<double>(depth) * 1000.0);
      TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(1e-6, static_cast<double>(depth) * 1000.0, p[1], msg);

      if (i > 0) {
        std::snprintf(msg, sizeof(msg),
                      "seed=%u N=%zu depth=%zu overlap/inversion between vertex=%zu (x=%f) "
                      "and next vertex=%zu (x=%f), dx=%f",
                      seed, numVertices, depth, level[i - 1], prevX, level[i], p[0],
                      p[0] - prevX);
        TEST_ASSERT_TRUE_MESSAGE(p[0] - prevX > 0.0, msg);
      }
      prevX = p[0];
    }
  }
}

static void test_stress_randomTree_1000_noOverlap(void) {
  unsigned int seed = static_cast<unsigned int>(time(NULL));
  CheckRandomTreeNoOverlap(1000, seed);
}

static void test_stress_randomTree_manySeeds_noOverlap(void) {
  unsigned int base = static_cast<unsigned int>(time(NULL));
  for (unsigned int trial = 0; trial < 30; trial++)
    CheckRandomTreeNoOverlap(1000, base + trial);
}

// ============================================================================
// STRESS: sibling gaps stay proportional to actual subtree size
// ============================================================================
//
// test_stress_randomTree_manySeeds_noOverlap (above) only asserts strictly-
// increasing x per BFS depth level -- excess/spurious separation trivially
// satisfies that, so a bug that inflates a gap by orders of magnitude (a
// real regression once shipped undetected here: CreateThreads' thread-offset
// formula dropping two frame-conversion terms, causing a huge subtree's own
// internal thread corrections to leak into an unrelated sibling gap at a
// shallower merge) would pass every existing test. This check instead
// bounds *every* parent's sibling gaps against that parent's own total leaf
// count -- an upper bound on demand for width regardless of *which* pair of
// children a given gap sits between, since a gap between children i-1 and i
// has to accommodate not just those two children's own subtrees but
// whatever was already merged into the blob to their left (see the
// calibration note below for why "just the two adjacent children's own leaf
// counts" is NOT a safe per-pair bound on its own).
//
// Bound derivation: kMinSeparation keeps every pair of leaf contours at
// least 1 offset unit apart, so a subtree's own width is always O(leaf
// count) in offset units, and kXSeparation converts offset units to
// position units (must track ReingoldTilford.cpp's private kXSeparation --
// same "no shared header for a production constant" tradeoff as the
// kYSeparation=1000.0 literal in CheckRandomTreeNoOverlap above). No single
// gap between any two of a vertex's children can exceed the width needed
// for *all* of that vertex's children combined, i.e.
// totalLeaves(vertex) * kXSeparation -- calibrated empirically at up to
// ~0.64x that bound across 600 random trees (50-2000 vertices) with the
// fix in place, vs. ~8.2x at the root alone with the regression this test
// guards against still present -- kGapSlackFactor leaves a wide margin
// above the former and well below the latter.
static void CheckRandomTreeSiblingGapsProportional(size_t numVertices, unsigned int seed) {
  constexpr double kXSeparation = 500.0;
  constexpr double kGapSlackFactor = 2.0;

  Graph g = gviz::graphs::BuildRandomConnectedGraph(numVertices, 0.0, seed, /*directed=*/true);
  g.BuildLayout();

  ReingoldTilford rt(g, 0);
  rt.CalculateOffsets(0, 0);

  double origin[2] = {0.0, 0.0};
  rt.Embed(0, origin);

  char msg[320];
  for (size_t v = 0; v < numVertices; v++) {
    size_t degree = g.Degree(v);
    if (degree < 2)
      continue;

    size_t totalLeaves = search::IsLeaf(g, v) ? 1 : search::CountLeaves(g, v);
    double bound = static_cast<double>(totalLeaves) * kXSeparation * kGapSlackFactor;

    for (size_t i = 1; i < degree; i++) {
      size_t a = g.Neighbor(v, i - 1);
      size_t b = g.Neighbor(v, i);
      double gap = rt.GetVPosition(b)[0] - rt.GetVPosition(a)[0];

      std::snprintf(msg, sizeof(msg),
                    "seed=%u N=%zu parent=%zu childIndex=%zu gap=%f exceeds "
                    "generous bound=%f (parent's totalLeaves=%zu)",
                    seed, numVertices, v, i, gap, bound, totalLeaves);
      TEST_ASSERT_TRUE_MESSAGE(gap <= bound, msg);
    }
  }
}

static void test_stress_randomTree_1000_siblingGapsProportional(void) {
  unsigned int seed = static_cast<unsigned int>(time(NULL));
  CheckRandomTreeSiblingGapsProportional(1000, seed);
}

static void test_stress_randomTree_manySeeds_siblingGapsProportional(void) {
  unsigned int base = static_cast<unsigned int>(time(NULL));
  for (unsigned int trial = 0; trial < 30; trial++)
    CheckRandomTreeSiblingGapsProportional(1000, base + trial);
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

  RUN_TEST(test_stress_randomTree_1000_noOverlap);
  RUN_TEST(test_stress_randomTree_manySeeds_noOverlap);
  RUN_TEST(test_stress_randomTree_1000_siblingGapsProportional);
  RUN_TEST(test_stress_randomTree_manySeeds_siblingGapsProportional);

  return UNITY_END();
}
