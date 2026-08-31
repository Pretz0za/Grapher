// Port of the KNearest scenarios in tests/algorithms/search/gvizSearchTests.c
// against gviz::search::KNearest/KNearestFromVisibleBatch. KNearest lands
// separately from SearchTests.cpp/TreeTests.cpp per the M3 port (see
// CMakeLists.txt's "search/ C++ port" comment).
//
// Note on the old suite: test_searchKNearest_kLargerThanGraph,
// test_searchKNearest_zeroK, and test_searchKNearest_batchMatchesPerVertex
// were defined in gvizSearchTests.c but never registered in its RUN_TEST
// list (an apparent oversight -- they compiled and would have passed, they
// just never ran under ctest). All three are ported and registered here.
//
// New coverage beyond the old suite, following directly from the design
// decisions in KNearest.hpp:
//   - test_knearest_hiddenSource_returnsZero: the old C returned -1 for an
//     absent source. Per the precedent BreadthFirst.hpp/ConnectedComponents
//     already set in this namespace (routine, checkable preconditions on an
//     otherwise well-formed call don't throw -- exceptions are reserved for
//     fallible *construction*, see Error.hpp), and because
//     Subgraph::Neighbors already yields nothing for an absent vertex with
//     no extra check needed, this now quietly returns 0 instead of
//     signaling an error. A genuinely out-of-range source (>=
//     VertexCapacity()) is unchecked/UB instead, same as Graph::Neighbor --
//     not exercised here, matching how GraphTests.cpp/SubgraphTests.cpp
//     don't test their own UB paths either.
//   - test_batch_scratchTooSmall_returnsError /
//     test_batch_notApplicable_* : exercise all three BatchResult arms
//     (Applied is covered by the ported batchMatchesPerVertex test).
//   - test_knnProfile_countsQueries: the old suite had zero coverage of
//     GVIZ_KNN_PROFILE despite it being real, atomics-backed behavior;
//     this adds a minimal smoke test (conditional on the env var actually
//     being set, matching the old code's own env-gated behavior).

#include "KNearest.hpp"

#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cstdlib>
#include <vector>

using gviz::BitSet;
using gviz::Graph;
using gviz::Subgraph;
using gviz::search::BatchResult;
using gviz::search::FoundVertex;
using gviz::search::KNearest;
using gviz::search::KNearestFromVisibleBatch;
using gviz::search::KNearestPreferBatch;
using gviz::search::KNearestScratch;
using gviz::search::KnnBatchTarget;
using gviz::search::KnnProfileReset;
using gviz::search::KnnProfileSnapshot;

void setUp(void) {}
void tearDown(void) {}

static void add_path4(Graph &g) {
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
}

static void add_triangle(Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(0, 2, 1.0);
}

// ============================================================================
// BASIC SEARCH
// ============================================================================

static void test_searchKNearest_triangle(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  FoundVertex found[2];
  size_t count = KNearest(sg, found, 2, 0);
  TEST_ASSERT_EQUAL_UINT64(2, count);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].dist);
  TEST_ASSERT_EQUAL_UINT64(2, found[1].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[1].dist);
}

static void test_searchKNearest_withFilter(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  BitSet filter(g.Size());
  filter.Set(2);

  FoundVertex found[1];
  size_t count = KNearest(sg, found, 1, 0, &filter);
  TEST_ASSERT_EQUAL_UINT64(1, count);
  TEST_ASSERT_EQUAL_UINT64(2, found[0].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].dist);
}

static void test_searchKNearest_scratch_matches_stateless(void) {
  Graph g(true);
  add_path4(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  KNearestScratch scratch(4);

  FoundVertex a[3];
  FoundVertex b[3];
  size_t countA = KNearest(sg, a, 3, 0);
  size_t countB = KNearest(sg, b, 3, 0, nullptr, scratch);
  TEST_ASSERT_EQUAL_UINT64(countA, countB);
  for (size_t i = 0; i < countA; i++) {
    TEST_ASSERT_EQUAL_UINT64(a[i].v, b[i].v);
    TEST_ASSERT_EQUAL_UINT64(a[i].dist, b[i].dist);
  }
}

static void test_searchKNearest_kLargerThanGraph(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  FoundVertex found[10];
  size_t count = KNearest(sg, found, 10, 0);
  TEST_ASSERT_EQUAL_UINT64(2, count); // only 2 other vertices exist
}

static void test_searchKNearest_zeroK(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  // k == 0 must short-circuit before touching out -- an empty span is fine.
  TEST_ASSERT_EQUAL_UINT64(0, KNearest(sg, std::span<FoundVertex>{}, 0, 0));
}

static void test_knearest_hiddenSource_returnsZero(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateEmpty(g); // source 0 deliberately not shown
  sg.ShowVertex(1);
  sg.ShowVertex(2);

  FoundVertex found[2];
  size_t count = KNearest(sg, found, 2, 0);
  TEST_ASSERT_EQUAL_UINT64(0, count);
}

// ============================================================================
// BATCH PATH
// ============================================================================

// The batched multi-source path must agree with per-vertex BFS results.
static void test_searchKNearest_batchMatchesPerVertex(void) {
  constexpr size_t N = 30;
  Graph g(false, N);
  for (size_t i = 0; i < N; i++)
    g.AddVertex();
  for (size_t i = 0; i + 1 < N; i++)
    g.AddEdge(i, i + 1, 1.0); // path graph
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  // 2 visible seeds, everyone else is a target.
  BitSet visible(N);
  visible.Set(0);
  visible.Set(N - 1);

  constexpr size_t K = 2;
  std::vector<KnnBatchTarget> targets;
  std::vector<std::vector<FoundVertex>> buffers(N - 2, std::vector<FoundVertex>(K));
  size_t t = 0;
  for (size_t v = 1; v + 1 < N; v++, t++)
    targets.push_back(KnnBatchTarget{v, buffers[t], 0});

  KNearestScratch scratch(N);
  BatchResult result = KNearestFromVisibleBatch(sg, &visible, K, targets, scratch);
  TEST_ASSERT_TRUE(result == BatchResult::Applied);

  for (t = 0; t < targets.size(); t++) {
    FoundVertex direct[K];
    size_t directCount = KNearest(sg, direct, K, targets[t].vertex, &visible, scratch);
    TEST_ASSERT_EQUAL_UINT64(directCount, targets[t].count);
    // Compare distances (vertex order may differ on ties).
    for (size_t i = 0; i < directCount; i++)
      TEST_ASSERT_EQUAL_UINT64(direct[i].dist, targets[t].out[i].dist);
  }
}

static void test_batch_notApplicable_nullVisible(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  std::vector<FoundVertex> buf(1);
  std::vector<KnnBatchTarget> targets{KnnBatchTarget{1, buf, 0}};
  KNearestScratch scratch(g.Size());

  BatchResult result = KNearestFromVisibleBatch(sg, nullptr, 1, targets, scratch);
  TEST_ASSERT_TRUE(result == BatchResult::NotApplicable);
}

static void test_batch_notApplicable_visibleNotSmallerThanTargets(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  BitSet visible(g.Size());
  visible.Set(0);
  visible.Set(1); // 2 visible, only 1 target -- not smaller, so NotApplicable

  std::vector<FoundVertex> buf(1);
  std::vector<KnnBatchTarget> targets{KnnBatchTarget{2, buf, 0}};
  KNearestScratch scratch(g.Size());

  BatchResult result = KNearestFromVisibleBatch(sg, &visible, 1, targets, scratch);
  TEST_ASSERT_TRUE(result == BatchResult::NotApplicable);
}

static void test_batch_scratchTooSmall_returnsError(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  BitSet visible(g.Size());
  visible.Set(0);

  std::vector<FoundVertex> buf(1);
  std::vector<KnnBatchTarget> targets{KnnBatchTarget{1, buf, 0}, KnnBatchTarget{2, buf, 0}};
  KNearestScratch scratch(1); // too small for a 3-vertex subgraph

  BatchResult result = KNearestFromVisibleBatch(sg, &visible, 1, targets, scratch);
  TEST_ASSERT_TRUE(result == BatchResult::Error);
}

// ============================================================================
// PREFER-BATCH HEURISTIC
// ============================================================================

static void test_preferBatch_manyTargetsSparseGraph(void) {
  constexpr size_t N = 30;
  Graph g(false, N);
  for (size_t i = 0; i < N; i++)
    g.AddVertex();
  for (size_t i = 0; i + 1 < N; i++)
    g.AddEdge(i, i + 1, 1.0);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  // 2 visible, 28 targets: targetCount >= kKnnBatchTargetRatio * visibleCount
  // (28 >= 8 * 2) even though average degree is low -- ratio path should win.
  TEST_ASSERT_TRUE(KNearestPreferBatch(sg, 2, 28));
  // Too few targets relative to visible seeds, and average degree is low:
  // neither condition holds.
  TEST_ASSERT_FALSE(KNearestPreferBatch(sg, 2, 5));
}

static void test_preferBatch_visibleNotSmallerThanTargets(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  TEST_ASSERT_FALSE(KNearestPreferBatch(sg, 3, 3));
}

// ============================================================================
// PROFILING COUNTERS
// ============================================================================

static void test_knnProfile_countsQueries(void) {
  Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  KnnProfileReset();
  unsigned long long queries = 999, visited = 999, maxVisited = 999;
  KnnProfileSnapshot(&queries, &visited, &maxVisited);
  TEST_ASSERT_EQUAL_UINT64(0, queries);
  TEST_ASSERT_EQUAL_UINT64(0, visited);
  TEST_ASSERT_EQUAL_UINT64(0, maxVisited);

  FoundVertex found[2];
  KNearest(sg, found, 2, 0);

  KnnProfileSnapshot(&queries, &visited, nullptr);
  if (std::getenv("GVIZ_KNN_PROFILE") != nullptr) {
    // Only asserts something when the harness actually enabled profiling --
    // KnnProfileEnabled() is gated on the env var, matching the old C.
    TEST_ASSERT_EQUAL_UINT64(1, queries);
    TEST_ASSERT_TRUE(visited >= 2);
  }

  // Nullable out-params: passing nullptr for a subset must not crash.
  KnnProfileSnapshot(nullptr, nullptr, nullptr);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_searchKNearest_triangle);
  RUN_TEST(test_searchKNearest_withFilter);
  RUN_TEST(test_searchKNearest_scratch_matches_stateless);
  RUN_TEST(test_searchKNearest_kLargerThanGraph);
  RUN_TEST(test_searchKNearest_zeroK);
  RUN_TEST(test_knearest_hiddenSource_returnsZero);

  RUN_TEST(test_searchKNearest_batchMatchesPerVertex);
  RUN_TEST(test_batch_notApplicable_nullVisible);
  RUN_TEST(test_batch_notApplicable_visibleNotSmallerThanTargets);
  RUN_TEST(test_batch_scratchTooSmall_returnsError);

  RUN_TEST(test_preferBatch_manyTargetsSparseGraph);
  RUN_TEST(test_preferBatch_visibleNotSmallerThanTargets);

  RUN_TEST(test_knnProfile_countsQueries);

  return UNITY_END();
}
