// Port of tests/embedders/gvizPlanarTests.c (the planarity portion; the
// Schnyder-wood tests live in SchnyderWoodTests.cpp) against
// gviz::layout::Planar / the free functions in Planar.hpp.
//
// Scenarios reshaped for the new API:
//   - gvizPlanarEmbedderInit's 0/-2/-1 return codes become, respectively:
//     normal return, a thrown PlanarNotPlanarError (with a
//     kuratowskiSubdivision member replacing the old out-param), and a
//     thrown LayoutError.
//   - Manual Init/Release pairs: RAII replaces them.
//   - gvizFaceSearchState/gvizPlanarNextFace: not ported (see Planar.hpp's
//     top-of-file note) -- FaceEnumerator::Faces() is already the fully
//     computed vector these walked a cursor over, so tests use it directly.

#include "Planar.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <vector>

using gviz::Graph;
using gviz::LayoutError;
using gviz::Subgraph;
using gviz::layout::ApplyPlanarRotation;
using gviz::layout::FaceEnumerator;
using gviz::layout::FaceSubgraphAt;
using gviz::layout::FaceWalk;
using gviz::layout::HalfEdge;
using gviz::layout::HalfEdgeTwin;
using gviz::layout::LargestFaceBoundary;
using gviz::layout::Planar;
using gviz::layout::PlanarNotPlanarError;
using gviz::layout::Triangulate;

void setUp(void) {}
void tearDown(void) {}

static Subgraph MakeFullSubgraph(Graph &g) {
  g.BuildLayout();
  return Subgraph::CreateFull(g);
}

// Cyclic "what comes after `curr` in this CCW order" helper, matching the
// old indexOf/getNextVertex pair.
static int NextInCycle(const std::vector<size_t> &order, size_t curr) {
  for (size_t i = 0; i < order.size(); i++) {
    if (order[i] == curr)
      return static_cast<int>(order[(i + 1) % order.size()]);
  }
  return -1;
}

static void test_planar_rotationInstalled(void) {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();

  // Added in the "wrong" rotational order on purpose -- Planar's job is to
  // reorder these into a CCW rotation system.
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 3, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);

  Planar p(g);

  std::vector<size_t> correctOrder = {3, 2, 1};
  size_t prev = g.Neighbor(0, 0);
  for (size_t i = 1; i < 3; i++) {
    size_t curr = g.Neighbor(0, i);
    int expected = NextInCycle(correctOrder, prev);
    TEST_ASSERT_NOT_EQUAL(-1, expected);
    TEST_ASSERT_EQUAL_UINT64(static_cast<size_t>(expected), curr);
    prev = curr;
  }
}

static void test_nonPlanar_throwsPlanarNotPlanarError(void) {
  Graph g(false);
  for (int i = 0; i < 6; i++)
    g.AddVertex();

  // K3,3
  for (size_t u = 0; u < 3; u++)
    for (size_t v = 3; v < 6; v++)
      g.AddEdge(u, v, 1.0);

  bool threw = false;
  try {
    Planar p(g);
    (void)p;
  } catch (const PlanarNotPlanarError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_nonPlanar_kuratowskiWitness(void) {
  Graph g(false);
  for (int i = 0; i < 5; i++)
    g.AddVertex();

  // K5
  for (size_t u = 0; u < 5; u++)
    for (size_t v = u + 1; v < 5; v++)
      g.AddEdge(u, v, 1.0);

  Subgraph sg = MakeFullSubgraph(g);

  bool threw = false;
  try {
    ApplyPlanarRotation(g, sg, /*captureWitness=*/true);
  } catch (const PlanarNotPlanarError &e) {
    threw = true;
    // The witness must be a subgraph of K5 with some edges (a K5 subdivision).
    TEST_ASSERT_EQUAL_UINT64(5, e.kuratowskiSubdivision.Size());
    e.kuratowskiSubdivision.BuildLayout();
    TEST_ASSERT_TRUE(e.kuratowskiSubdivision.EdgeCount() > 0);
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_nonPlanar_captureWitnessFalseThrowsPlainError(void) {
  Graph g(false);
  for (int i = 0; i < 6; i++)
    g.AddVertex();
  for (size_t u = 0; u < 3; u++)
    for (size_t v = 3; v < 6; v++)
      g.AddEdge(u, v, 1.0);

  Subgraph sg = MakeFullSubgraph(g);

  bool threw = false;
  try {
    ApplyPlanarRotation(g, sg, /*captureWitness=*/false);
  } catch (const PlanarNotPlanarError &) {
    TEST_FAIL_MESSAGE("expected plain NotPlanarError, not PlanarNotPlanarError");
  } catch (const gviz::NotPlanarError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_largestFaceBoundary_square(void) {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  // 4-cycle: both faces are the square itself.
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 0, 1.0);

  Subgraph sg = MakeFullSubgraph(g);
  ApplyPlanarRotation(g, sg);

  std::vector<size_t> boundary = LargestFaceBoundary(g, sg);
  TEST_ASSERT_EQUAL_UINT64(4, boundary.size());

  int seen[4] = {0, 0, 0, 0};
  for (size_t v : boundary) {
    TEST_ASSERT_TRUE(v < 4);
    seen[v]++;
  }
  for (int i = 0; i < 4; i++)
    TEST_ASSERT_EQUAL_INT(1, seen[i]);
}

static void test_halfEdge_twin(void) {
  HalfEdge e{3, 7};
  HalfEdge t = HalfEdgeTwin(e);
  TEST_ASSERT_EQUAL_UINT64(7, t.u);
  TEST_ASSERT_EQUAL_UINT64(3, t.v);
}

static void test_triangulation_hexagon(void) {
  Graph g(false);
  for (int i = 0; i < 6; i++)
    g.AddVertex();

  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 4, 1.0);
  g.AddEdge(4, 5, 1.0);
  g.AddEdge(5, 0, 1.0);
  g.AddEdge(5, 3, 1.0);

  Planar p(g);
  (void)p;

  Subgraph sg = MakeFullSubgraph(g);
  FaceEnumerator faces(g, sg);
  Triangulate(g, sg, faces);

  for (const auto &face : faces.Faces())
    TEST_ASSERT_EQUAL_UINT64(3, face.size());

  // Euler's formula for a connected planar embedding: V - E + F == 2, where
  // E == DartCount() / 2 (each edge contributes two darts).
  TEST_ASSERT_EQUAL_UINT64(
      2, g.Size() - faces.DartCount() / 2 + faces.Faces().size());
}

static void test_subgraph_neighbor_ccw_order(void) {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();

  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 3, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);

  Planar p(g);
  (void)p;

  std::vector<size_t> correctOrder = {3, 2, 1};
  std::vector<size_t> nbrs;
  for (size_t v : g.Neighbors(0))
    nbrs.push_back(v);
  TEST_ASSERT_EQUAL_UINT64(3, nbrs.size());

  size_t prev = nbrs[0];
  for (size_t i = 1; i < nbrs.size(); i++) {
    int expected = NextInCycle(correctOrder, prev);
    TEST_ASSERT_NOT_EQUAL(-1, expected);
    TEST_ASSERT_EQUAL_UINT64(static_cast<size_t>(expected), nbrs[i]);
    prev = nbrs[i];
  }
}

static void test_faceWalk_triangle(void) {
  Graph g(false);
  for (int i = 0; i < 3; i++)
    g.AddVertex();

  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 0, 1.0);

  Planar p(g);
  (void)p;

  Subgraph sg = MakeFullSubgraph(g);
  FaceWalk walk(g, sg, HalfEdge{0, 1});
  std::vector<size_t> seen;
  for (size_t v : walk)
    seen.push_back(v);

  TEST_ASSERT_EQUAL_UINT64(3, seen.size());
}

static void test_faceWalk_rejectsNonSubgraphEdge(void) {
  Graph g(false);
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 0, 1.0);

  Planar p(g);
  (void)p;
  Subgraph sg = MakeFullSubgraph(g);

  // 0-1 IS a real edge -- must construct without throwing.
  FaceWalk ok(g, sg, HalfEdge{0, 1});
  (void)ok;

  // (0, 99) references a vertex that doesn't even exist in g -- a dart that
  // is by construction never a subgraph edge -- must throw.
  bool threw = false;
  try {
    FaceWalk bad(g, sg, HalfEdge{0, 99});
    (void)bad;
  } catch (const LayoutError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_faceSubgraphAt_squareCenter(void) {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 0, 1.0);

  Planar p(g);
  double pos0[2] = {0.0, 0.0};
  double pos1[2] = {1.0, 0.0};
  double pos2[2] = {1.0, 1.0};
  double pos3[2] = {0.0, 1.0};
  p.SetVPosition(0, pos0);
  p.SetVPosition(1, pos1);
  p.SetVPosition(2, pos2);
  p.SetVPosition(3, pos3);

  auto found = FaceSubgraphAt(g, p, 0.5, 0.5);
  TEST_ASSERT_TRUE(found.has_value());
  TEST_ASSERT_EQUAL_UINT64(4, found->VertexCount());
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_planar_rotationInstalled);
  RUN_TEST(test_nonPlanar_throwsPlanarNotPlanarError);
  RUN_TEST(test_nonPlanar_kuratowskiWitness);
  RUN_TEST(test_nonPlanar_captureWitnessFalseThrowsPlainError);
  RUN_TEST(test_largestFaceBoundary_square);
  RUN_TEST(test_halfEdge_twin);
  RUN_TEST(test_triangulation_hexagon);
  RUN_TEST(test_subgraph_neighbor_ccw_order);
  RUN_TEST(test_faceWalk_triangle);
  RUN_TEST(test_faceWalk_rejectsNonSubgraphEdge);  RUN_TEST(test_faceSubgraphAt_squareCenter);

  return UNITY_END();
}
