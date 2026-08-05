// Port of tests/ds/gvizGraphTests.c against gviz::Graph.
//
// Scenarios intentionally dropped, each with the same underlying reason as
// the ThreadPoolTests.cpp precedent (M1): they exercised a state the new
// API doesn't have a way to reach.
//   - test_vertexInit_*/test_vertexCopy_*/test_vertexClone_* (6 tests):
//     gviz::Graph has no standalone Vertex-construction API -- a vertex is
//     just an entry gviz::Graph::AddVertex appends, and Vertex itself is a
//     private implementation detail. Their behavior is covered by the
//     AddVertex tests below (data round-trips) and by
//     test_graph_copyCtor_isIndependentDeepCopy (adjacency lists copy
//     independently).
//   - test_graphInit_WithNullPointer, test_vertexInit_WithNullPointer: no
//     null Graph*/Vertex* exists anymore -- Graph is a real object.
//   - test_graphInitAtCapacity_WithSpecificCapacity: the reserved vector
//     capacity isn't part of Graph's logical state and isn't exposed by
//     any accessor (unlike Subgraph::VertexCapacity(), which IS part of a
//     documented amortized-growth *contract*); asserting on it would test
//     an implementation detail with no public accessor to even name it.
//   - test_graphAddEdge_InvalidIndices, test_graphRemoveEdge_InvalidIndices,
//     test_graph_EdgeExistsInvalidIndices,
//     test_graphGetVertexNeighbors_InvalidIndex: per the port plan, an
//     out-of-range vertex index passed to any Graph method (checked or
//     unchecked) is now a precondition violation, the same as it always
//     was for Degree/Neighbor/NeighborWeight -- calling with one is UB,
//     not a value to assert against.
//   - test_graphLoadFromEdgesFile_* (3 tests): utils/graphLoader hasn't
//     been ported yet (that's M5); there is no gviz::Graph-based loader to
//     test against here.
//
// New coverage added beyond the old suite: InsertNeighborAt,
// ReorderNeighbors, and the bool-returning NeighborPosition had no
// dedicated tests in the old suite at all (grep turns up zero test call
// sites), so they're covered fresh here.

#include "Graph.hpp"

#include "unity/unity.h"

#include <vector>

void setUp(void) {}
void tearDown(void) {}

static bool neighbor_contains(const gviz::Graph &g, size_t vertex, size_t target) {
  size_t pos;
  return g.NeighborPosition(vertex, target, pos);
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

static void test_graph_construct_directed(void) {
  gviz::Graph g(true);
  TEST_ASSERT_TRUE(g.IsDirected());
  TEST_ASSERT_EQUAL_UINT64(0, g.Size());
}

static void test_graph_construct_undirected(void) {
  gviz::Graph g(false);
  TEST_ASSERT_FALSE(g.IsDirected());
}

// ============================================================================
// ADD VERTEX
// ============================================================================

static void test_graphAddVertex_SingleVertex(void) {
  gviz::Graph g(true);
  size_t idx = g.AddVertex(nullptr);
  TEST_ASSERT_EQUAL_UINT64(0, idx);
  TEST_ASSERT_EQUAL_UINT64(1, g.Size());
}

static void test_graphAddVertex_MultipleVertices(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 5; i++) {
    int data = i * 10;
    (void)data;
    g.AddVertex(nullptr);
  }
  TEST_ASSERT_EQUAL_UINT64(5, g.Size());
}

static void test_graphAddVertex_RetrieveVertexData(void) {
  gviz::Graph g(true);
  int data1 = 100, data2 = 200;
  g.AddVertex(&data1);
  g.AddVertex(&data2);

  TEST_ASSERT_EQUAL_INT(100, *(int *)g.GetVertexData(0));
  TEST_ASSERT_EQUAL_INT(200, *(int *)g.GetVertexData(1));
}

static void test_graphAddVertex_defaultDataIsNull(void) {
  gviz::Graph g(true);
  g.AddVertex();
  TEST_ASSERT_NULL(g.GetVertexData(0));
}

static void test_graphAddVertex_OutOfBounds(void) {
  gviz::Graph g(true);
  int data = 42;
  g.AddVertex(&data);

  TEST_ASSERT_NULL(g.GetVertexData(1));
}

// ============================================================================
// ADD EDGE
// ============================================================================

static void test_graphAddEdge_SimpleEdge(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
}

static void test_graphAddEdge_UndirectedGraphBiDirectional(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 0));
}

static void test_graphAddEdge_SelfLoop(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddEdge(0, 0, 1.0);
  TEST_ASSERT_TRUE(g.EdgeExists(0, 0));
}

static void test_graphAddEdge_MultipleEdges(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();

  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(2, 3, 1.0);

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(0, 2));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 3));
  TEST_ASSERT_TRUE(g.EdgeExists(2, 3));

  TEST_ASSERT_FALSE(g.EdgeExists(1, 0));
  TEST_ASSERT_FALSE(g.EdgeExists(3, 0));
}

// ============================================================================
// EDGE WEIGHT
// ============================================================================

static void test_graphEdgeWeight_RoundTrip(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 2.5);

  double weight = 0.0;
  TEST_ASSERT_TRUE(g.GetEdgeWeight(0, 1, weight));
  TEST_ASSERT_EQUAL_DOUBLE(2.5, weight);
}

static void test_graphEdgeWeight_SetUpdatesWeight(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  TEST_ASSERT_TRUE(g.SetEdgeWeight(0, 1, 9.0));

  double weight = 0.0;
  g.GetEdgeWeight(0, 1, weight);
  TEST_ASSERT_EQUAL_DOUBLE(9.0, weight);
}

static void test_graphEdgeWeight_UndirectedMirrorsWeight(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 4.0);

  double forward = 0.0, backward = 0.0;
  TEST_ASSERT_TRUE(g.GetEdgeWeight(0, 1, forward));
  TEST_ASSERT_TRUE(g.GetEdgeWeight(1, 0, backward));
  TEST_ASSERT_EQUAL_DOUBLE(4.0, forward);
  TEST_ASSERT_EQUAL_DOUBLE(4.0, backward);

  TEST_ASSERT_TRUE(g.SetEdgeWeight(1, 0, 7.0));
  g.GetEdgeWeight(0, 1, forward);
  g.GetEdgeWeight(1, 0, backward);
  TEST_ASSERT_EQUAL_DOUBLE(7.0, forward);
  TEST_ASSERT_EQUAL_DOUBLE(7.0, backward);
}

static void test_graphEdgeWeight_MissingEdgeFails(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();

  double weight = 0.0;
  TEST_ASSERT_FALSE(g.GetEdgeWeight(0, 1, weight));
  TEST_ASSERT_FALSE(g.SetEdgeWeight(0, 1, 1.0));
}

static void test_graphEdgeWeight_updateDoesNotBumpMutationCount(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  uint64_t before = g.MutationCount();
  g.SetEdgeWeight(0, 1, 5.0);
  TEST_ASSERT_EQUAL_UINT64(before, g.MutationCount());
}

// ============================================================================
// REMOVE EDGE
// ============================================================================

static void test_graphRemoveEdge_Basic(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));

  TEST_ASSERT_TRUE(g.RemoveEdge(0, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(0, 1));
}

static void test_graphRemoveEdge_NonExistentEdge(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  TEST_ASSERT_FALSE(g.RemoveEdge(0, 1));
}

static void test_graphRemoveEdge_UndirectedRemovesBoth(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  TEST_ASSERT_TRUE(g.RemoveEdge(0, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(0, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(1, 0));
}

// ============================================================================
// DEGREE / NEIGHBORS
// ============================================================================

static void test_graphGetVertexNeighbors_WithEdges(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);

  TEST_ASSERT_EQUAL_UINT64(2, g.Degree(0));
}

static void test_graphGetVertexNeighbors_NoNeighbors(void) {
  gviz::Graph g(true);
  g.AddVertex();
  TEST_ASSERT_EQUAL_UINT64(0, g.Degree(0));
}

static void test_graph_neighborsRangeFor(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 2.0);

  double sum = 0.0;
  size_t count = 0;
  for (const gviz::Edge &e : g.Neighbors(0)) {
    sum += e.weight;
    count++;
  }
  TEST_ASSERT_EQUAL_UINT64(2, count);
  TEST_ASSERT_EQUAL_DOUBLE(3.0, sum);
}

// ============================================================================
// NEIGHBOR POSITION / INSERT / REORDER (new coverage)
// ============================================================================

static void test_neighborPosition_foundAndNotFound(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 2, 1.0);

  size_t pos = 999;
  TEST_ASSERT_TRUE(g.NeighborPosition(0, 2, pos));
  TEST_ASSERT_EQUAL_UINT64(0, pos);
  TEST_ASSERT_FALSE(g.NeighborPosition(0, 1, pos));
}

static void test_insertNeighborAt_middle(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 3, 3.0);

  uint64_t before = g.MutationCount();
  TEST_ASSERT_TRUE(g.InsertNeighborAt(0, 2, 2.0, 1));
  TEST_ASSERT_TRUE(g.MutationCount() > before);

  TEST_ASSERT_EQUAL_UINT64(3, g.Degree(0));
  TEST_ASSERT_EQUAL_UINT64(1, g.Neighbor(0, 0));
  TEST_ASSERT_EQUAL_UINT64(2, g.Neighbor(0, 1));
  TEST_ASSERT_EQUAL_UINT64(3, g.Neighbor(0, 2));
}

static void test_insertNeighborAt_positionOutOfRangeFails(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  TEST_ASSERT_FALSE(g.InsertNeighborAt(0, 1, 1.0, 5));
  TEST_ASSERT_EQUAL_UINT64(0, g.Degree(0));
}

static void test_insertNeighborAt_doesNotMirror(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  TEST_ASSERT_TRUE(g.InsertNeighborAt(0, 1, 1.0, 0));
  TEST_ASSERT_EQUAL_UINT64(1, g.Degree(0));
  TEST_ASSERT_EQUAL_UINT64(0, g.Degree(1));
}

static void test_reorderNeighbors_permutesPreservingWeights(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 10.0);
  g.AddEdge(0, 2, 20.0);
  g.AddEdge(0, 3, 30.0);

  uint64_t before = g.MutationCount();
  std::vector<size_t> order = {3, 1, 2};
  TEST_ASSERT_TRUE(g.ReorderNeighbors(0, order));
  TEST_ASSERT_EQUAL_UINT64(before, g.MutationCount()); // reordering isn't structural

  TEST_ASSERT_EQUAL_UINT64(3, g.Neighbor(0, 0));
  TEST_ASSERT_EQUAL_UINT64(1, g.Neighbor(0, 1));
  TEST_ASSERT_EQUAL_UINT64(2, g.Neighbor(0, 2));
  TEST_ASSERT_EQUAL_DOUBLE(30.0, g.NeighborWeight(0, 0));
}

static void test_reorderNeighbors_wrongSizeFails(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  std::vector<size_t> order = {1, 0};
  TEST_ASSERT_FALSE(g.ReorderNeighbors(0, order));
  TEST_ASSERT_EQUAL_UINT64(1, g.Neighbor(0, 0)); // unchanged
}

static void test_reorderNeighbors_unknownIdFails(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 2.0);

  // order.size() matches the current degree (2), but 5 isn't a neighbor of
  // 0 at all -- this is the case the ported algorithm actually detects.
  // (A same-size order that repeats an existing id while omitting another,
  // e.g. {1, 1}, is NOT detected as invalid by this algorithm -- each
  // element is independently "found" by id, with no duplicate-usage or
  // full-coverage check. That's a preexisting gap in the old C
  // gvizGraphReorderNeighbors this is a straight port of, not something
  // introduced here, and not in scope to fix in a port.)
  std::vector<size_t> order = {1, 5};
  TEST_ASSERT_FALSE(g.ReorderNeighbors(0, order));
  TEST_ASSERT_EQUAL_UINT64(1, g.Neighbor(0, 0)); // unchanged
  TEST_ASSERT_EQUAL_UINT64(2, g.Neighbor(0, 1));
}

// ============================================================================
// COPY / REVERSED
// ============================================================================

static void test_graphCopy_Basic(void) {
  gviz::Graph src(true);
  for (int i = 0; i < 3; i++)
    src.AddVertex();
  src.AddEdge(0, 1, 1.0);
  src.AddEdge(1, 2, 1.0);

  gviz::Graph dest(src);
  TEST_ASSERT_EQUAL_UINT64(3, dest.Size());
  TEST_ASSERT_TRUE(dest.IsDirected());
  TEST_ASSERT_TRUE(dest.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(dest.EdgeExists(1, 2));
}

static void test_graph_copyCtor_isIndependentDeepCopy(void) {
  gviz::Graph src(true);
  src.AddVertex();
  src.AddVertex();
  src.AddEdge(0, 1, 1.0);

  gviz::Graph dest(src);
  src.AddEdge(1, 0, 5.0); // mutate src after the copy
  TEST_ASSERT_FALSE(dest.EdgeExists(1, 0));
  TEST_ASSERT_TRUE(src.EdgeExists(1, 0));
}

static void test_graph_copyAssignment_isIndependentDeepCopy(void) {
  gviz::Graph src(true);
  src.AddVertex();
  src.AddVertex();
  src.AddEdge(0, 1, 1.0);

  gviz::Graph dest(false, 1);
  dest = src;
  TEST_ASSERT_EQUAL_UINT64(2, dest.Size());
  TEST_ASSERT_TRUE(dest.EdgeExists(0, 1));

  src.AddEdge(1, 0, 2.0);
  TEST_ASSERT_FALSE(dest.EdgeExists(1, 0));
}

static void test_graphCopyReversed_DirectedGraph(void) {
  gviz::Graph src(true);
  for (int i = 0; i < 3; i++)
    src.AddVertex();
  src.AddEdge(0, 1, 1.0);
  src.AddEdge(1, 2, 1.0);

  gviz::Graph dest = src.Reversed();

  TEST_ASSERT_TRUE(dest.EdgeExists(1, 0));
  TEST_ASSERT_TRUE(dest.EdgeExists(2, 1));
  TEST_ASSERT_FALSE(dest.EdgeExists(0, 1));
  TEST_ASSERT_FALSE(dest.EdgeExists(1, 2));
}

// Regression: the old gvizGraphCloneReversed used to loop forever on any
// vertex with at least one outgoing edge (loop condition never consulted
// the index). Kept as a Reversed() equivalent.
static void test_graphReversed_DirectedGraph_terminates(void) {
  gviz::Graph src(true);
  for (int i = 0; i < 4; i++)
    src.AddVertex();
  src.AddEdge(0, 1, 1.0);
  src.AddEdge(1, 2, 1.0);
  src.AddEdge(1, 3, 1.0);

  gviz::Graph dest = src.Reversed();

  TEST_ASSERT_EQUAL_UINT64(4, dest.Size());
  TEST_ASSERT_TRUE(dest.EdgeExists(1, 0));
  TEST_ASSERT_TRUE(dest.EdgeExists(2, 1));
  TEST_ASSERT_TRUE(dest.EdgeExists(3, 1));
  TEST_ASSERT_FALSE(dest.EdgeExists(0, 1));
  TEST_ASSERT_FALSE(dest.EdgeExists(1, 2));
}

static void test_graphReversed_UndirectedFallsBackToCopy(void) {
  gviz::Graph src(false);
  for (int i = 0; i < 3; i++)
    src.AddVertex();
  src.AddEdge(0, 1, 1.0);

  gviz::Graph dest = src.Reversed();
  TEST_ASSERT_TRUE(dest.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(dest.EdgeExists(1, 0));
}

// ============================================================================
// CLEAR
// ============================================================================

static void test_graphClear_RemovesAllVertices(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 5; i++)
    g.AddVertex();
  TEST_ASSERT_EQUAL_UINT64(5, g.Size());

  g.Clear();
  TEST_ASSERT_EQUAL_UINT64(0, g.Size());
}

// ============================================================================
// STRESS TESTS
// ============================================================================

static void test_graphAddVertex_LargeNumberOfVertices(void) {
  gviz::Graph g(true);
  size_t largeSize = 1000;
  for (size_t i = 0; i < largeSize; i++) {
    size_t idx = g.AddVertex();
    TEST_ASSERT_EQUAL_UINT64(i, idx);
  }
  TEST_ASSERT_EQUAL_UINT64(largeSize, g.Size());
}

static void test_graph_CompleteGraph(void) {
  gviz::Graph g(true);
  int vertexCount = 20;
  for (int i = 0; i < vertexCount; i++)
    g.AddVertex();

  for (int i = 0; i < vertexCount; i++)
    for (int j = 0; j < vertexCount; j++)
      if (i != j)
        g.AddEdge(i, j, 1.0);

  TEST_ASSERT_EQUAL_UINT64((size_t)vertexCount, g.Size());
  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(5, 10));
  TEST_ASSERT_TRUE(g.EdgeExists(vertexCount - 1, 0));
}

static void test_graph_LinearChain(void) {
  gviz::Graph g(true);
  int chainLength = 100;
  for (int i = 0; i < chainLength; i++)
    g.AddVertex();
  for (int i = 0; i < chainLength - 1; i++)
    g.AddEdge(i, i + 1, 1.0);

  for (int i = 0; i < chainLength - 1; i++)
    TEST_ASSERT_TRUE(g.EdgeExists(i, i + 1));
}

static void test_graph_StarTopology(void) {
  gviz::Graph g(true);
  int vertexCount = 50;
  int centerVertex = 0;
  for (int i = 0; i < vertexCount; i++)
    g.AddVertex();
  for (int i = 1; i < vertexCount; i++)
    g.AddEdge(centerVertex, i, 1.0);

  TEST_ASSERT_EQUAL_UINT64((size_t)(vertexCount - 1), g.Degree(centerVertex));
}

// ============================================================================
// DIRECTED VS UNDIRECTED
// ============================================================================

static void test_graph_DirectedAsymmetry(void) {
  gviz::Graph g(true);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(1, 0));
}

static void test_graph_UndirectedSymmetry(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 0));
}

// ============================================================================
// MIXED OPERATIONS
// ============================================================================

static void test_graph_BuildAndModify(void) {
  gviz::Graph g(true);
  for (int i = 0; i < 5; i++)
    g.AddVertex();

  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 4, 1.0);

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(3, 4));

  g.RemoveEdge(2, 3);
  TEST_ASSERT_FALSE(g.EdgeExists(2, 3));

  g.AddEdge(0, 4, 1.0);
  TEST_ASSERT_TRUE(g.EdgeExists(0, 4));
}

// ============================================================================
// LAYOUT / EDGE COUNT
// ============================================================================

static void test_graph_hasLayout_falseInitially(void) {
  gviz::Graph g(false);
  g.AddVertex();
  TEST_ASSERT_FALSE(g.HasLayout());
  TEST_ASSERT_EQUAL_UINT64(0, g.EdgeCount());
}

static void test_graph_buildLayout_populatesEdgeCount(void) {
  gviz::Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);

  g.BuildLayout();
  TEST_ASSERT_TRUE(g.HasLayout());
  TEST_ASSERT_EQUAL_UINT64(3, g.EdgeCount());
}

static void test_graph_ensureLayout_rebuildsOnlyWhenStale(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.EnsureLayout();
  TEST_ASSERT_EQUAL_UINT64(0, g.EdgeCount());

  g.AddEdge(0, 1, 1.0);
  g.EnsureLayout(); // mutationCount changed -> rebuilds
  TEST_ASSERT_EQUAL_UINT64(1, g.EdgeCount());
}

static void test_graph_clear_dropsLayout(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.BuildLayout();
  TEST_ASSERT_TRUE(g.HasLayout());

  g.Clear();
  TEST_ASSERT_FALSE(g.HasLayout());
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_graph_construct_directed);
  RUN_TEST(test_graph_construct_undirected);

  RUN_TEST(test_graphAddVertex_SingleVertex);
  RUN_TEST(test_graphAddVertex_MultipleVertices);
  RUN_TEST(test_graphAddVertex_RetrieveVertexData);
  RUN_TEST(test_graphAddVertex_defaultDataIsNull);
  RUN_TEST(test_graphAddVertex_OutOfBounds);

  RUN_TEST(test_graphAddEdge_SimpleEdge);
  RUN_TEST(test_graphAddEdge_UndirectedGraphBiDirectional);
  RUN_TEST(test_graphAddEdge_SelfLoop);
  RUN_TEST(test_graphAddEdge_MultipleEdges);

  RUN_TEST(test_graphEdgeWeight_RoundTrip);
  RUN_TEST(test_graphEdgeWeight_SetUpdatesWeight);
  RUN_TEST(test_graphEdgeWeight_UndirectedMirrorsWeight);
  RUN_TEST(test_graphEdgeWeight_MissingEdgeFails);
  RUN_TEST(test_graphEdgeWeight_updateDoesNotBumpMutationCount);

  RUN_TEST(test_graphRemoveEdge_Basic);
  RUN_TEST(test_graphRemoveEdge_NonExistentEdge);
  RUN_TEST(test_graphRemoveEdge_UndirectedRemovesBoth);

  RUN_TEST(test_graphGetVertexNeighbors_WithEdges);
  RUN_TEST(test_graphGetVertexNeighbors_NoNeighbors);
  RUN_TEST(test_graph_neighborsRangeFor);

  RUN_TEST(test_neighborPosition_foundAndNotFound);
  RUN_TEST(test_insertNeighborAt_middle);
  RUN_TEST(test_insertNeighborAt_positionOutOfRangeFails);
  RUN_TEST(test_insertNeighborAt_doesNotMirror);
  RUN_TEST(test_reorderNeighbors_permutesPreservingWeights);
  RUN_TEST(test_reorderNeighbors_wrongSizeFails);
  RUN_TEST(test_reorderNeighbors_unknownIdFails);

  RUN_TEST(test_graphCopy_Basic);
  RUN_TEST(test_graph_copyCtor_isIndependentDeepCopy);
  RUN_TEST(test_graph_copyAssignment_isIndependentDeepCopy);
  RUN_TEST(test_graphCopyReversed_DirectedGraph);
  RUN_TEST(test_graphReversed_DirectedGraph_terminates);
  RUN_TEST(test_graphReversed_UndirectedFallsBackToCopy);

  RUN_TEST(test_graphClear_RemovesAllVertices);

  RUN_TEST(test_graphAddVertex_LargeNumberOfVertices);
  RUN_TEST(test_graph_CompleteGraph);
  RUN_TEST(test_graph_LinearChain);
  RUN_TEST(test_graph_StarTopology);

  RUN_TEST(test_graph_DirectedAsymmetry);
  RUN_TEST(test_graph_UndirectedSymmetry);

  RUN_TEST(test_graph_BuildAndModify);

  RUN_TEST(test_graph_hasLayout_falseInitially);
  RUN_TEST(test_graph_buildLayout_populatesEdgeCount);
  RUN_TEST(test_graph_ensureLayout_rebuildsOnlyWhenStale);
  RUN_TEST(test_graph_clear_dropsLayout);

  return UNITY_END();
}
