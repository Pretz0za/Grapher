// Port of tests/embedders/gvizForceEmbedderTests.c's scenarios against
// gviz::layout::ForceAtlas, adapted to the RAII/exception shape: construction
// either produces a valid object or throws (DimensionError for the old -2
// "wrong dimension" return; std::bad_alloc propagates on its own for
// allocation failure, nothing to assert there), and Run() throws
// std::logic_error in place of the old -1 "Begin not called" sentinel.
//
// The dynamic-graph Sync() test (test_sync_placesNewVertexNearNeighborAnd-
// PreservesExisting) is dropped: dynamic graph growth during an active
// embedding is out of scope for this library now (see CLAUDE.md's
// EmbeddedGraph section) -- ForceAtlas's physics arrays are built once, at
// construction, from GraphLike<G>::structure_ as it stood then, and there
// is no Sync() left to test.
//
// ForceAtlas is generic over GraphLike (`template <GraphLike G> class
// ForceAtlas`); every construction below deduces G = Subgraph via CTAD from
// the Subgraph argument, same call syntax as before the refactor.

#include "ForceAtlas.hpp"

#include "ForceModel.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cmath>
#include <memory>

using gviz::Graph;
using gviz::Subgraph;
using gviz::layout::Action;
using gviz::DimensionError;
using gviz::layout::EmbeddedGraph;
using gviz::layout::ForceAtlas;
using gviz::layout::ForceModel;
using gviz::layout::FruchtermanReingold;

void setUp(void) {}
void tearDown(void) {}

// A vertex-induced subgraph with every vertex of @p g shown -- the shape
// ForceAtlas expects for dynamic (growing) embeddings; see
// EmbeddedGraph.hpp's GROWTH & SYNC section.
static Subgraph MakeInduced(Graph &g, size_t nvertices) {
  for (size_t i = 0; i < nvertices; i++)
    g.AddVertex();
  Subgraph sg = Subgraph::CreateVertexInduced(g);
  for (size_t i = 0; i < nvertices; i++)
    sg.ShowVertex(i);
  return sg;
}

static void AddCycle(Graph &g, size_t n) {
  for (size_t i = 0; i < n; i++)
    g.AddEdge(i, (i + 1) % n, 1.0);
}

// ============================================================================
// CONSTRUCTION
// ============================================================================

static void test_construction_dimensionErrorOnBadDimension(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 3);

  bool threw = false;
  try {
    ForceAtlas fa(std::move(sg), 3, std::make_unique<FruchtermanReingold>());
  } catch (const DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_construction_dimension2Succeeds(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 3);
  AddCycle(g, 3);

  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());
  TEST_ASSERT_EQUAL_UINT64(2, fa.Dim());
  TEST_ASSERT_EQUAL_UINT64(3, fa.PositionCount());
  TEST_ASSERT_FALSE(fa.Begun());
}

static void test_construction_registersActionsAndStats(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 3);
  AddCycle(g, 3);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  TEST_ASSERT_NOT_NULL(fa.FindAction("forceEmbedder.step"));
  TEST_ASSERT_NOT_NULL(fa.FindAction("forceEmbedder.toggleOverlapPrevention"));
  TEST_ASSERT_EQUAL_UINT64(2, fa.ActionCount());

  TEST_ASSERT_NOT_NULL(fa.FindStatSeries("forceEmbedder.maxDisp"));
  TEST_ASSERT_NOT_NULL(fa.FindStatSeries("forceEmbedder.speed"));
  TEST_ASSERT_NOT_NULL(fa.FindStatSeries("forceEmbedder.attractiveForce"));
  TEST_ASSERT_NOT_NULL(fa.FindStatSeries("forceEmbedder.repulsiveForce"));
  TEST_ASSERT_NOT_NULL(fa.FindStatSeries("forceEmbedder.gravityForce"));
  TEST_ASSERT_EQUAL_UINT64(5, fa.StatSeriesCount());
}

// ============================================================================
// BEGIN
// ============================================================================

static void test_begin_placesVerticesInBox(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 10);
  AddCycle(g, 10);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  fa.Configure(/*edgeLength=*/10.0, /*boxExtent=*/25.0);
  fa.Begin(1234);
  TEST_ASSERT_TRUE(fa.Begun());

  bool anyNonZero = false;
  for (size_t v = 0; v < 10; v++) {
    double *p = fa.GetVPosition(v);
    TEST_ASSERT_TRUE(p[0] >= -25.0 && p[0] <= 25.0);
    TEST_ASSERT_TRUE(p[1] >= -25.0 && p[1] <= 25.0);
    if (p[0] != 0.0 || p[1] != 0.0)
      anyNonZero = true;
  }
  TEST_ASSERT_TRUE(anyNonZero);
}

// ============================================================================
// STEP CONVERGENCE
// ============================================================================

static void test_step_reducesDisplacementOverRounds(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 8);
  AddCycle(g, 8);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());
  fa.Begin(42);

  double first = fa.Step();
  TEST_ASSERT_TRUE(std::isfinite(first));
  TEST_ASSERT_TRUE(first > 0.0);

  size_t rounds = fa.Run(300, 1e-9);
  TEST_ASSERT_TRUE(rounds > 0);
  TEST_ASSERT_TRUE(std::isfinite(fa.LastMaxDisplacement()));

  // A small cycle under FR with no gravity settles: the layout should be
  // moving much less after 300 rounds of relaxation than it was on the very
  // first round.
  TEST_ASSERT_TRUE(fa.LastMaxDisplacement() < first);
}

static void test_run_throwsIfBeginNotCalled(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 3);
  AddCycle(g, 3);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  bool threw = false;
  try {
    fa.Run(10, 1e-6);
  } catch (const std::logic_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// ============================================================================
// GRAVITY
// ============================================================================

// A single, edgeless vertex feels no attraction and no repulsion (there is
// no other vertex to repel it) -- with gravity configured, the only force
// acting on it is a constant pull toward the origin, so its distance from
// the origin must shrink by exactly gravityK_ per round while it hasn't yet
// crossed the origin.
static void test_gravity_pullsTowardOrigin(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 1);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  fa.ConfigureGravity(0.5);
  fa.Begin(1);

  double start[2] = {5.0, 0.0};
  fa.SetVPosition(0, start);

  double distBefore = std::hypot(fa.GetVPosition(0)[0], fa.GetVPosition(0)[1]);
  for (int i = 0; i < 3; i++) {
    fa.Step();
    double distAfter = std::hypot(fa.GetVPosition(0)[0], fa.GetVPosition(0)[1]);
    TEST_ASSERT_TRUE(distAfter < distBefore);
    distBefore = distAfter;
  }
}

// ============================================================================
// PREVENT OVERLAP / RADIUS
// ============================================================================

static void test_radius_formula(void) {
  Graph g(false);
  // A star: vertex 0 has degree 4, leaves have degree 1.
  Subgraph sg = MakeInduced(g, 5);
  for (size_t i = 1; i < 5; i++)
    g.AddEdge(0, i, 1.0);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  fa.ConfigureRadius(/*base=*/2.0, /*perDegree=*/0.5);

  // Compact index 0 is raw vertex 0 (construction order preserves ascending
  // subgraph iteration order), degree 4: r = 2 * (1 + 0.5*sqrt(4)) = 4.
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 4.0, fa.VertexRadius(0));
  // A leaf, degree 1: r = 2 * (1 + 0.5*sqrt(1)) = 3.0.
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 3.0, fa.VertexRadius(1));
}

static void test_preventOverlap_separatesCoincidentVertices(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 2); // no edges: pure repulsion
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  fa.ConfigureRadius(1.0, 0.0);
  fa.SetPreventOverlapEnabled(true);
  TEST_ASSERT_TRUE(fa.PreventOverlapEnabled());
  fa.Begin(7);

  double coincident[2] = {3.0, -2.0};
  fa.SetVPosition(0, coincident);
  fa.SetVPosition(1, coincident);

  fa.Step();

  double *p0 = fa.GetVPosition(0);
  double *p1 = fa.GetVPosition(1);
  double dx = p0[0] - p1[0], dy = p0[1] - p1[1];
  double dist = std::hypot(dx, dy);
  TEST_ASSERT_TRUE(std::isfinite(dist));
  TEST_ASSERT_TRUE(dist > 1e-6); // no longer coincident
}

// ============================================================================
// ACTIONS
// ============================================================================

static void test_actions_stepActionAdvancesIterationOnlyWhenBegun(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 3);
  AddCycle(g, 3);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  // Before Begin(): the action is registered and safe to invoke, but the C
  // original's forceEmbedderActionStep contract (and this port's) is a
  // silent no-op until begun_.
  TEST_ASSERT_TRUE(fa.InvokeAction("forceEmbedder.step"));
  TEST_ASSERT_EQUAL_UINT64(0, fa.Iteration());

  fa.Begin(5);
  TEST_ASSERT_EQUAL_UINT64(0, fa.Iteration());
  TEST_ASSERT_TRUE(fa.InvokeAction("forceEmbedder.step"));
  TEST_ASSERT_EQUAL_UINT64(1, fa.Iteration());
}

static void test_actions_toggleOverlapPrevention(void) {
  Graph g(false);
  Subgraph sg = MakeInduced(g, 3);
  AddCycle(g, 3);
  ForceAtlas fa(std::move(sg), 2, std::make_unique<FruchtermanReingold>());

  TEST_ASSERT_FALSE(fa.PreventOverlapEnabled());
  TEST_ASSERT_TRUE(fa.InvokeAction("forceEmbedder.toggleOverlapPrevention"));
  TEST_ASSERT_TRUE(fa.PreventOverlapEnabled());
  TEST_ASSERT_TRUE(fa.InvokeAction("forceEmbedder.toggleOverlapPrevention"));
  TEST_ASSERT_FALSE(fa.PreventOverlapEnabled());
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_construction_dimensionErrorOnBadDimension);
  RUN_TEST(test_construction_dimension2Succeeds);
  RUN_TEST(test_construction_registersActionsAndStats);

  RUN_TEST(test_begin_placesVerticesInBox);

  RUN_TEST(test_step_reducesDisplacementOverRounds);
  RUN_TEST(test_run_throwsIfBeginNotCalled);

  RUN_TEST(test_gravity_pullsTowardOrigin);

  RUN_TEST(test_radius_formula);
  RUN_TEST(test_preventOverlap_separatesCoincidentVertices);

  RUN_TEST(test_actions_stepActionAdvancesIterationOnlyWhenBegun);
  RUN_TEST(test_actions_toggleOverlapPrevention);

  return UNITY_END();
}
