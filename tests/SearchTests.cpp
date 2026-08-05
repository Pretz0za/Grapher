// Port of tests/algorithms/search/gvizSearchTests.c's BreadthFirst/
// DepthFirst/ConnectedComponents coverage against gviz::search's C++ port.
// KNearest is being ported separately (a concurrent, unrelated task) and is
// intentionally not touched here.
//
// Two scenarios from the old suite are not ported 1:1:
// test_searchBreadthFirst_invalidInputs and
// test_searchDepthFirst_invalidSource each included a NULL-subgraph/
// NULL-output assertion. gviz::Subgraph has no null/unbound state (see
// Subgraph.hpp's class comment) -- a Subgraph& simply cannot be null in
// valid C++, so that failure mode no longer exists to test. The "source not
// in sg" case from both tests is kept below.

#include "BreadthFirst.hpp"
#include "ConnectedComponents.hpp"
#include "DepthFirst.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"

#include "unity/unity.h"

#include <cstdint>
#include <vector>

using gviz::Graph;
using gviz::Subgraph;
namespace search = gviz::search;

void setUp(void) {}
void tearDown(void) {}

namespace {

Graph MakePath4() {
  Graph g(/*directed=*/true);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.BuildLayout();
  return g;
}

Graph MakeTriangle() {
  Graph g(/*directed=*/false);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.BuildLayout();
  return g;
}

} // namespace

// ============================================================================
// BREADTH-FIRST
// ============================================================================

static void test_searchBreadthFirst_path(void) {
  Graph g = MakePath4();
  Subgraph sg = Subgraph::CreateFull(g);
  Subgraph tree = Subgraph::CreateEmpty(g);

  std::vector<size_t> distances;
  TEST_ASSERT_TRUE(search::BreadthFirst(sg, tree, 0, 0, &distances));
  TEST_ASSERT_EQUAL_UINT64(4, tree.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(3, tree.EdgeCount());
  TEST_ASSERT_EQUAL_UINT64(0, distances[0]);
  TEST_ASSERT_EQUAL_UINT64(1, distances[1]);
  TEST_ASSERT_EQUAL_UINT64(2, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(3, distances[3]);
}

static void test_searchBreadthFirst_maxDepth(void) {
  Graph g = MakePath4();
  Subgraph sg = Subgraph::CreateFull(g);
  Subgraph tree = Subgraph::CreateEmpty(g);

  std::vector<size_t> distances;
  TEST_ASSERT_TRUE(search::BreadthFirst(sg, tree, 0, 1, &distances));
  TEST_ASSERT_EQUAL_UINT64(2, tree.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(1, tree.EdgeCount());
  TEST_ASSERT_EQUAL_UINT64(0, distances[0]);
  TEST_ASSERT_EQUAL_UINT64(1, distances[1]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[3]);
}

static void test_searchBreadthFirst_invalidSource(void) {
  Graph g = MakePath4();
  Subgraph sg = Subgraph::CreateFull(g);
  Subgraph tree = Subgraph::CreateEmpty(g);

  sg.HideVertex(2); // source outside the subgraph
  TEST_ASSERT_FALSE(search::BreadthFirst(sg, tree, 2, 0, nullptr));
}

// BFS in a subgraph must not walk through hidden vertices.
static void test_searchBreadthFirst_respectsHiddenVertices(void) {
  Graph g = MakePath4(); // 0-1-2-3
  Subgraph sg = Subgraph::CreateFull(g);
  sg.HideVertex(1); // cuts the path

  Subgraph tree = Subgraph::CreateEmpty(g);
  std::vector<size_t> distances;
  TEST_ASSERT_TRUE(search::BreadthFirst(sg, tree, 0, 0, &distances));
  TEST_ASSERT_EQUAL_UINT64(1, tree.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[3]);
}

// ============================================================================
// DEPTH-FIRST
// ============================================================================

static void test_searchDepthFirst_path(void) {
  Graph g = MakePath4();
  Subgraph sg = Subgraph::CreateFull(g);
  Subgraph tree = Subgraph::CreateEmpty(g);

  TEST_ASSERT_TRUE(search::DepthFirst(sg, tree, 0));
  TEST_ASSERT_EQUAL_UINT64(4, tree.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(3, tree.EdgeCount());
}

static void test_searchDepthFirst_invalidSource(void) {
  Graph g = MakePath4();
  Subgraph sg = Subgraph::CreateFull(g);
  Subgraph tree = Subgraph::CreateEmpty(g);
  sg.HideVertex(3);

  TEST_ASSERT_FALSE(search::DepthFirst(sg, tree, 3));
}

// ============================================================================
// CONNECTED COMPONENTS
// ============================================================================

static void test_connectedComponents_singleComponent(void) {
  Graph g = MakeTriangle();
  Subgraph sg = Subgraph::CreateFull(g);

  search::Components result = search::ConnectedComponents(sg);
  TEST_ASSERT_EQUAL_UINT64(1, result.count);
  TEST_ASSERT_EQUAL_UINT64(0, result.labels[0]);
  TEST_ASSERT_EQUAL_UINT64(0, result.labels[1]);
  TEST_ASSERT_EQUAL_UINT64(0, result.labels[2]);
}

static void test_connectedComponents_disconnected(void) {
  Graph g(/*directed=*/false);
  for (int i = 0; i < 5; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.BuildLayout();

  Subgraph sg = Subgraph::CreateFull(g);
  search::Components result = search::ConnectedComponents(sg);

  TEST_ASSERT_EQUAL_UINT64(3, result.count);
  TEST_ASSERT_EQUAL_UINT64(0, result.labels[0]);
  TEST_ASSERT_EQUAL_UINT64(0, result.labels[1]);
  TEST_ASSERT_EQUAL_UINT64(1, result.labels[2]);
  TEST_ASSERT_EQUAL_UINT64(1, result.labels[3]);
  TEST_ASSERT_EQUAL_UINT64(2, result.labels[4]);

  std::vector<size_t> sizes = search::ConnectedComponentSizes(result.labels, result.count);
  TEST_ASSERT_EQUAL_UINT64(2, sizes[0]);
  TEST_ASSERT_EQUAL_UINT64(2, sizes[1]);
  TEST_ASSERT_EQUAL_UINT64(1, sizes[2]);
}

static void test_connectedComponents_emptySubgraph(void) {
  Graph g = MakeTriangle();
  // vertex-induced subgraph with no vertices shown
  Subgraph sg = Subgraph::CreateVertexInduced(g);

  search::Components result = search::ConnectedComponents(sg);
  TEST_ASSERT_EQUAL_UINT64(0, result.count);
  TEST_ASSERT_EQUAL_UINT64(3, result.labels.size());
  for (size_t i = 0; i < 3; i++)
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, result.labels[i]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_searchBreadthFirst_path);
  RUN_TEST(test_searchBreadthFirst_maxDepth);
  RUN_TEST(test_searchBreadthFirst_invalidSource);
  RUN_TEST(test_searchBreadthFirst_respectsHiddenVertices);
  RUN_TEST(test_searchDepthFirst_path);
  RUN_TEST(test_searchDepthFirst_invalidSource);
  RUN_TEST(test_connectedComponents_singleComponent);
  RUN_TEST(test_connectedComponents_disconnected);
  RUN_TEST(test_connectedComponents_emptySubgraph);
  return UNITY_END();
}
