// Port of tests/embedders/gvizTutteTests.c (plus a couple of scenarios from
// tests/embedders/gvizTuttePickTests.c and new coverage for the C++-specific
// exception/return-value shape) against gviz::layout::Tutte.
//
// Scenarios reshaped for the new API:
//   - gvizTutteEmbedderInit's -1 return becomes std::bad_alloc (never
//     exercised directly here) or DimensionError for a bad dimension.
//   - gvizTutteEmbedderBegin's -2/-1 return codes become, respectively, a
//     thrown PlanarNotPlanarError and a thrown LayoutError.
//   - gvizTutteEmbedderRun's -1 "invalid state" return becomes a thrown
//     std::logic_error (no boundary pinned yet).
//   - Manual Init/Release pairs: RAII replaces them.

#include "Tutte.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Planar.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cmath>
#include <stdexcept>
#include <vector>

using gviz::DimensionError;
using gviz::Graph;
using gviz::LayoutError;
using gviz::Subgraph;
using gviz::layout::PlanarNotPlanarError;
using gviz::layout::Tutte;

void setUp(void) {}
void tearDown(void) {}

static Subgraph MakeFullSubgraph(Graph &g) {
  g.BuildLayout();
  return Subgraph::CreateFull(g);
}

// K4: vertices 0-2 form the outer triangle, vertex 3 is the interior.
static Graph BuildK4() {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 3, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(2, 3, 1.0);
  return g;
}

// K4 with the outer triangle pinned: after convergence vertex 3 must sit at
// the centroid of the triangle to within a loose tolerance. Never calls
// Begin() -- exercises SetBoundary()/FixConvexPolygon()+SeedInterior()+Run()
// directly, the same path the old C test used.
static void test_tutte_k4_centroid(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2, 1e-8);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(t.FixConvexPolygon(boundary, 100.0));
  t.SeedInterior();

  t.Run(10000);

  const double *p0 = t.GetVPosition(0);
  const double *p1 = t.GetVPosition(1);
  const double *p2 = t.GetVPosition(2);
  const double *p3 = t.GetVPosition(3);

  double cx = (p0[0] + p1[0] + p2[0]) / 3.0;
  double cy = (p0[1] + p1[1] + p2[1]) / 3.0;

  TEST_ASSERT_DOUBLE_WITHIN(1e-4, cx, p3[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, cy, p3[1]);
}

// Boundary vertex positions must be bit-exact after any number of steps.
static void test_tutte_boundary_pinned(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2, 1e-8);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(t.FixConvexPolygon(boundary, 50.0));
  t.SeedInterior();

  double before[3][2];
  for (int i = 0; i < 3; i++) {
    const double *p = t.GetVPosition(static_cast<size_t>(i));
    before[i][0] = p[0];
    before[i][1] = p[1];
  }

  for (int i = 0; i < 50; i++)
    t.Step(0.016);

  for (int i = 0; i < 3; i++) {
    const double *p = t.GetVPosition(static_cast<size_t>(i));
    TEST_ASSERT_EQUAL_DOUBLE(before[i][0], p[0]);
    TEST_ASSERT_EQUAL_DOUBLE(before[i][1], p[1]);
  }
}

// A small grid should converge within a reasonable iteration budget.
static void test_tutte_convergence(void) {
  size_t L = 5, W = 5;
  Graph g(false);
  for (size_t i = 0; i < L * W; i++)
    g.AddVertex();
  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++) {
      size_t idx = i * W + j;
      if (j + 1 < W)
        g.AddEdge(idx, i * W + j + 1, 1.0);
      if (i + 1 < L)
        g.AddEdge(idx, (i + 1) * W + j, 1.0);
    }

  Tutte t(g, MakeFullSubgraph(g), 2, 1e-5);

  // Collect rim vertices in CCW order: top, right, bottom (rev), left (rev).
  std::vector<size_t> rim;
  for (size_t j = 0; j < W; j++)
    rim.push_back(j);
  for (size_t i = 1; i < L; i++)
    rim.push_back(i * W + W - 1);
  for (size_t j = W - 1; j-- > 0;)
    rim.push_back((L - 1) * W + j);
  for (size_t i = L - 1; i-- > 1;)
    rim.push_back(i * W);

  TEST_ASSERT_TRUE(t.FixConvexPolygon(rim, 200.0));
  t.SeedInterior();

  size_t iters = t.Run(5000);

  TEST_ASSERT_TRUE(t.Converged());
  TEST_ASSERT_LESS_THAN(5000, iters);
}

// Jacobi and Gauss-Seidel must converge to the same fixed point (within
// 1e-4). GS should use no more iterations than Jacobi.
static void test_tutte_jacobi_vs_gs(void) {
  Graph g = BuildK4();
  Tutte tJ(g, MakeFullSubgraph(g), 2, 1e-8);
  Tutte tGS(g, MakeFullSubgraph(g), 2, 1e-8);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(tJ.FixConvexPolygon(boundary, 100.0));
  tJ.SeedInterior();
  TEST_ASSERT_TRUE(tGS.FixConvexPolygon(boundary, 100.0));
  tGS.SeedInterior();

  TEST_ASSERT_FALSE(tGS.GaussSeidelEnabled());
  tGS.SetGaussSeidelEnabled(true);
  TEST_ASSERT_TRUE(tGS.GaussSeidelEnabled());

  tJ.Run(10000);
  tGS.Run(10000);

  const double *pJ = tJ.GetVPosition(3);
  const double *pGS = tGS.GetVPosition(3);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, pJ[0], pGS[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-4, pJ[1], pGS[1]);

  TEST_ASSERT_LESS_OR_EQUAL(tJ.Iteration(), tGS.Iteration());
}

// SetBoundary with count<3 or an out-of-range index must return false and
// leave the object otherwise usable.
static void test_tutte_setBoundary_validation(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2);

  size_t tooFew[2] = {0, 1};
  double pos[4] = {0.0, 0.0, 1.0, 0.0};
  TEST_ASSERT_FALSE(t.SetBoundary(tooFew, pos));

  size_t bad[3] = {0, 1, 99};
  double pos3[6] = {0};
  TEST_ASSERT_FALSE(t.SetBoundary(bad, pos3));

  // The object must still be usable after both rejected calls.
  size_t good[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(t.FixConvexPolygon(good, 10.0));
}

// Dimension != 2 must be rejected at construction, not deferred to Begin().
static void test_tutte_dimension_validation(void) {
  Graph g = BuildK4();
  bool threw = false;
  try {
    Tutte t(g, MakeFullSubgraph(g), 3);
    (void)t;
  } catch (const DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// Run() before any boundary has ever been pinned is a usage-order error.
static void test_tutte_run_before_boundary_throws(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2);

  bool threw = false;
  try {
    t.Run(10);
  } catch (const std::logic_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// Begin() on a planar graph succeeds, installs a rotation system, and
// leaves the embedding ready to Run().
static void test_tutte_begin_planar_succeeds(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2, 1e-6);

  t.Begin();

  TEST_ASSERT_TRUE(t.IsPlanarEmbedded());
  TEST_ASSERT_TRUE(t.Begun());
  TEST_ASSERT_TRUE(t.Boundary().size() >= 3);

  size_t iters = t.Run(10000);
  TEST_ASSERT_TRUE(t.Converged());
  (void)iters;
}

// Begin() on a non-planar graph (K5) must throw PlanarNotPlanarError.
static void test_tutte_begin_nonplanar_throws(void) {
  Graph g(false);
  for (int i = 0; i < 5; i++)
    g.AddVertex();
  for (size_t u = 0; u < 5; u++)
    for (size_t v = u + 1; v < 5; v++)
      g.AddEdge(u, v, 1.0);

  Tutte t(g, MakeFullSubgraph(g), 2);

  bool threw = false;
  try {
    t.Begin();
  } catch (const PlanarNotPlanarError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
  // A rejected Begin() must not have left the embedding marked planar.
  TEST_ASSERT_FALSE(t.IsPlanarEmbedded());
}

// Begin() is safe to call a second time (documented as a deliberate reset,
// not guarded against) -- it re-tests planarity, re-pins the boundary, and
// re-seeds interior vertices from scratch.
static void test_tutte_begin_twice(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2, 1e-6);

  t.Begin();
  t.Run(10000);
  TEST_ASSERT_TRUE(t.Converged());

  t.Begin();
  TEST_ASSERT_EQUAL_UINT64(0, t.Iteration());
  TEST_ASSERT_FALSE(t.Converged());
  t.Run(10000);
  TEST_ASSERT_TRUE(t.Converged());
}

// FixOuterFace() with no highlight set must return false.
static void test_tutte_fixOuterFace_noHighlight(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2);
  t.Begin();

  TEST_ASSERT_FALSE(t.HasHighlight());
  TEST_ASSERT_FALSE(t.FixOuterFace());
}

// FixOuterFace() with a highlight covering real subgraph edges must succeed
// and re-pin the boundary to whichever face FaceWalk finds starting from an
// edge inside the highlight (K4's planar rotation has 4 triangular faces,
// each missing exactly one vertex -- edge (1,2) borders two of them, {1,2,3}
// and {0,1,2}, so which one gets picked depends on which direction the
// implementation happens to walk first; both are valid 3-vertex faces, so
// this test only checks the outcome shape, not a specific vertex set).
static void test_tutte_fixOuterFace_withHighlight(void) {
  Graph g = BuildK4();
  Tutte t(g, MakeFullSubgraph(g), 2);
  t.Begin();

  Subgraph highlight = Subgraph::CreateFull(g);
  highlight.HideVertex(0);
  t.SetHighlight(std::move(highlight));

  TEST_ASSERT_TRUE(t.HasHighlight());
  TEST_ASSERT_TRUE(t.FixOuterFace());
  TEST_ASSERT_EQUAL_UINT64(0, t.Iteration());
  TEST_ASSERT_FALSE(t.Converged());
  TEST_ASSERT_EQUAL_UINT64(3, t.Boundary().size());

  t.Run(10000);
  TEST_ASSERT_TRUE(t.Converged());
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_tutte_k4_centroid);
  RUN_TEST(test_tutte_boundary_pinned);
  RUN_TEST(test_tutte_convergence);
  RUN_TEST(test_tutte_jacobi_vs_gs);
  RUN_TEST(test_tutte_setBoundary_validation);
  RUN_TEST(test_tutte_dimension_validation);
  RUN_TEST(test_tutte_run_before_boundary_throws);
  RUN_TEST(test_tutte_begin_planar_succeeds);
  RUN_TEST(test_tutte_begin_nonplanar_throws);
  RUN_TEST(test_tutte_begin_twice);
  RUN_TEST(test_tutte_fixOuterFace_noHighlight);
  RUN_TEST(test_tutte_fixOuterFace_withHighlight);

  return UNITY_END();
}
