// Port of tests/embedders/gvizSpringTutteTests.c against
// gviz::layout::SpringTutte, plus coverage for behavior that had no
// old-C-test equivalent (SetBoundary()'s bool-return validation shape,
// FixOuterFace()'s no-highlight/not-planar-embedded cases, and velocity
// preservation across FixOuterFace(), which SpringTutte alone needs since
// plain Tutte has no velocity state). Mirrors tests/TutteTests.cpp
// structurally wherever the underlying behavior doesn't actually differ.
//
// Scenarios reshaped for the new API:
//   - gvizSpringTutteEmbedderInit's -1 return becomes std::bad_alloc (never
//     exercised directly here) or DimensionError for a bad dimension.
//   - gvizSpringTutteEmbedderBegin's -2/-1 return codes become,
//     respectively, a thrown PlanarNotPlanarError and a thrown LayoutError.
//   - gvizSpringTutteEmbedderRun's -1 "invalid state" return becomes a
//     thrown std::logic_error (no boundary pinned yet).
//   - Manual Init/Release pairs: RAII replaces them.

#include "SpringTutte.hpp"

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
using gviz::Subgraph;
using gviz::layout::FaceEnumerator;
using gviz::layout::FaceSubgraph;
using gviz::layout::PlanarNotPlanarError;
using gviz::layout::SpringTutte;

void setUp(void) {}
void tearDown(void) {}

static Subgraph MakeFullSubgraph(Graph &g) {
  g.BuildLayout();
  return Subgraph::CreateFull(g);
}

// K4: vertices 0-2 form the outer triangle, vertex 3 is the interior apex.
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

// L x W grid, row-major vertex ids (idx = i * W + j).
static Graph BuildGrid(size_t L, size_t W) {
  Graph g(false);
  for (size_t i = 0; i < L * W; i++)
    g.AddVertex();
  for (size_t i = 0; i < L; i++) {
    for (size_t j = 0; j < W; j++) {
      size_t idx = i * W + j;
      if (j + 1 < W)
        g.AddEdge(idx, i * W + j + 1, 1.0);
      if (i + 1 < L)
        g.AddEdge(idx, (i + 1) * W + j, 1.0);
    }
  }
  return g;
}

// K4 with the outer triangle pinned: after convergence vertex 3 must sit at
// the centroid of the triangle to within a loose tolerance. Never calls
// Begin() -- exercises SetBoundary()/FixConvexPolygon()+SeedInterior()+Run()
// directly, the same path the old C test used.
static void test_springTutte_k4_centroid(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2, 1e-8);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(st.FixConvexPolygon(boundary, 100.0));
  st.SeedInterior();

  st.Run(20000, 0.016);

  const double *p0 = st.GetVPosition(0);
  const double *p1 = st.GetVPosition(1);
  const double *p2 = st.GetVPosition(2);
  const double *p3 = st.GetVPosition(3);

  double cx = (p0[0] + p1[0] + p2[0]) / 3.0;
  double cy = (p0[1] + p1[1] + p2[1]) / 3.0;

  TEST_ASSERT_DOUBLE_WITHIN(1e-3, cx, p3[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-3, cy, p3[1]);
}

// Boundary vertex positions must be bit-exact after any number of steps.
static void test_springTutte_boundary_pinned(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2, 1e-8);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(st.FixConvexPolygon(boundary, 50.0));
  st.SeedInterior();

  double before[3][2];
  for (int i = 0; i < 3; i++) {
    const double *p = st.GetVPosition(static_cast<size_t>(i));
    before[i][0] = p[0];
    before[i][1] = p[1];
  }

  for (int i = 0; i < 50; i++)
    st.Step(0.016);

  for (int i = 0; i < 3; i++) {
    const double *p = st.GetVPosition(static_cast<size_t>(i));
    TEST_ASSERT_EQUAL_DOUBLE(before[i][0], p[0]);
    TEST_ASSERT_EQUAL_DOUBLE(before[i][1], p[1]);
  }
}

// Boundary pinning must also seed interior vertices to the boundary
// centroid and zero every vertex's velocity (SpringTutte-only check: Tutte
// has no velocity state).
static void test_springTutte_seedsInteriorAndZeroesVelocity(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(st.FixConvexPolygon(boundary, 100.0));
  st.SeedInterior();

  for (size_t u : boundary) {
    TEST_ASSERT_TRUE(st.IsBoundaryVertex(u));
    const double *v = st.GetVelocity(u);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, v[0]);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, v[1]);
  }

  TEST_ASSERT_FALSE(st.IsBoundaryVertex(3));
  const double *v3 = st.GetVelocity(3);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, v3[0]);
  TEST_ASSERT_EQUAL_DOUBLE(0.0, v3[1]);

  // Interior vertex must have been seeded to the boundary centroid: for an
  // equilateral triangle pinned around the origin, that's the origin.
  const double *p3 = st.GetVPosition(3);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, p3[0]);
  TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, p3[1]);
}

// With default (underdamped) stiffness/damping, a vertex displaced away from
// its equilibrium must overshoot past it at least once before settling --
// the defining difference from plain Tutte's monotonic relaxation, which can
// never cross its target.
static void test_springTutte_underdamped_overshoots(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2, 1e-9);

  size_t boundary[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(st.FixConvexPolygon(boundary, 100.0));
  st.SeedInterior();

  // Equilibrium for vertex 3 is the triangle centroid, (0, 0). Displace it
  // far away so relaxation toward equilibrium is non-trivial.
  double displaced[2] = {300.0, 0.0};
  st.SetVPosition(3, displaced);

  bool crossedZero = false;
  double prevX = displaced[0];
  for (int i = 0; i < 2000 && !crossedZero; i++) {
    st.Step(0.016);
    double x = st.GetVPosition(3)[0];
    if ((prevX > 0.0 && x < 0.0) || (prevX < 0.0 && x > 0.0))
      crossedZero = true;
    prevX = x;
  }

  TEST_ASSERT_TRUE(crossedZero);
}

// A small grid should still converge (decay to rest) within a generous
// iteration budget despite the added inertia/oscillation, when overdamped.
static void test_springTutte_grid_convergesOverdamped(void) {
  size_t L = 5, W = 5;
  Graph g = BuildGrid(L, W);

  SpringTutte st(g, MakeFullSubgraph(g), 2, 1e-4);
  st.Configure(30.0, 40.0); // overdamped so this settles within a bounded budget

  std::vector<size_t> rim;
  for (size_t j = 0; j < W; j++)
    rim.push_back(j);
  for (size_t i = 1; i < L; i++)
    rim.push_back(i * W + W - 1);
  for (size_t j = W - 1; j-- > 0;)
    rim.push_back((L - 1) * W + j);
  for (size_t i = L - 1; i-- > 1;)
    rim.push_back(i * W);

  TEST_ASSERT_TRUE(st.FixConvexPolygon(rim, 200.0));
  st.SeedInterior();

  size_t iters = st.Run(20000, 0.016);

  TEST_ASSERT_TRUE(st.Converged());
  TEST_ASSERT_LESS_THAN(20000, iters);
}

// SetBoundary with count<3 or an out-of-range index must return false and
// leave the object otherwise usable.
static void test_springTutte_setBoundary_validation(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);

  size_t tooFew[2] = {0, 1};
  double pos[4] = {0.0, 0.0, 1.0, 0.0};
  TEST_ASSERT_FALSE(st.SetBoundary(tooFew, pos));

  size_t bad[3] = {0, 1, 99};
  double pos3[6] = {0};
  TEST_ASSERT_FALSE(st.SetBoundary(bad, pos3));

  // The object must still be usable after both rejected calls.
  size_t good[3] = {0, 1, 2};
  TEST_ASSERT_TRUE(st.FixConvexPolygon(good, 10.0));
}

// Dimension != 2 must be rejected at construction, not deferred to Begin().
static void test_springTutte_dimension_validation(void) {
  Graph g = BuildK4();
  bool threw = false;
  try {
    SpringTutte st(g, MakeFullSubgraph(g), 3);
    (void)st;
  } catch (const DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// Configure(0, 0) must keep both current values; each nonzero argument
// overrides only its own field.
static void test_springTutte_configure_zeroMeansKeepCurrent(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);

  TEST_ASSERT_EQUAL_DOUBLE(SpringTutte::kDefaultStiffness, st.Stiffness());
  TEST_ASSERT_EQUAL_DOUBLE(SpringTutte::kDefaultDamping, st.Damping());

  st.Configure(0.0, 0.0);
  TEST_ASSERT_EQUAL_DOUBLE(SpringTutte::kDefaultStiffness, st.Stiffness());
  TEST_ASSERT_EQUAL_DOUBLE(SpringTutte::kDefaultDamping, st.Damping());

  st.Configure(50.0, 0.0);
  TEST_ASSERT_EQUAL_DOUBLE(50.0, st.Stiffness());
  TEST_ASSERT_EQUAL_DOUBLE(SpringTutte::kDefaultDamping, st.Damping());

  st.Configure(0.0, 12.0);
  TEST_ASSERT_EQUAL_DOUBLE(50.0, st.Stiffness());
  TEST_ASSERT_EQUAL_DOUBLE(12.0, st.Damping());
}

// Run() before any boundary has ever been pinned is a usage-order error.
static void test_springTutte_run_before_boundary_throws(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);

  bool threw = false;
  try {
    st.Run(10, 0.016);
  } catch (const std::logic_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// Begin() on a planar graph succeeds, installs a rotation system, and
// leaves the embedding ready to Run().
static void test_springTutte_begin_planar_succeeds(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2, 1e-6);

  st.Begin();

  TEST_ASSERT_TRUE(st.IsPlanarEmbedded());
  TEST_ASSERT_TRUE(st.Begun());
  TEST_ASSERT_TRUE(st.Boundary().size() >= 3);

  st.Run(20000, 0.016);
  TEST_ASSERT_TRUE(st.Converged());
}

// Begin() on a non-planar graph (K3,3) must throw PlanarNotPlanarError.
static void test_springTutte_begin_nonplanar_throws(void) {
  Graph g(false);
  for (int i = 0; i < 6; i++)
    g.AddVertex();
  for (size_t u = 0; u < 3; u++)
    for (size_t v = 3; v < 6; v++)
      g.AddEdge(u, v, 1.0);

  SpringTutte st(g, MakeFullSubgraph(g), 2);

  bool threw = false;
  try {
    st.Begin();
  } catch (const PlanarNotPlanarError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
  TEST_ASSERT_FALSE(st.IsPlanarEmbedded());
}

// Begin() is safe to call a second time (deliberate reset, not guarded
// against, matching Tutte::Begin()) -- it re-tests planarity, re-pins the
// boundary, and re-seeds interior vertices (and velocities) from scratch.
static void test_springTutte_begin_twice(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2, 1e-6);

  st.Begin();
  st.Run(20000, 0.016);
  TEST_ASSERT_TRUE(st.Converged());

  st.Begin();
  TEST_ASSERT_EQUAL_UINT64(0, st.Iteration());
  TEST_ASSERT_FALSE(st.Converged());
  st.Run(20000, 0.016);
  TEST_ASSERT_TRUE(st.Converged());
}

// FixOuterFace() with no highlight set must return false.
static void test_springTutte_fixOuterFace_noHighlight(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);
  st.Begin();

  TEST_ASSERT_FALSE(st.HasHighlight());
  TEST_ASSERT_FALSE(st.FixOuterFace());
}

// FixOuterFace() before any rotation system is installed (Begin() never
// called) must return false rather than walking an undefined rotation.
static void test_springTutte_fixOuterFace_notPlanarEmbedded(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);

  TEST_ASSERT_FALSE(st.IsPlanarEmbedded());
  TEST_ASSERT_FALSE(st.FixOuterFace());
}

// FixOuterFace() with a highlight covering real subgraph edges must succeed
// and re-pin the boundary to whichever face FaceWalk finds starting from an
// edge inside the highlight (K4's planar rotation has 4 triangular faces,
// each missing exactly one vertex -- which one gets picked depends on which
// direction the implementation happens to walk first, so this test only
// checks the outcome shape, not a specific vertex set).
static void test_springTutte_fixOuterFace_withHighlight(void) {
  Graph g = BuildK4();
  SpringTutte st(g, MakeFullSubgraph(g), 2);
  st.Begin();

  Subgraph highlight = Subgraph::CreateFull(g);
  highlight.HideVertex(0);
  st.SetHighlight(std::move(highlight));

  TEST_ASSERT_TRUE(st.HasHighlight());
  TEST_ASSERT_TRUE(st.FixOuterFace());
  TEST_ASSERT_EQUAL_UINT64(0, st.Iteration());
  TEST_ASSERT_FALSE(st.Converged());
  TEST_ASSERT_EQUAL_UINT64(3, st.Boundary().size());

  st.Run(20000, 0.016);
  TEST_ASSERT_TRUE(st.Converged());
}

// Picking a different face as the new outer boundary must leave every vertex
// NOT on the new boundary -- including ones off in another part of the
// graph that were never touched -- with its position AND velocity exactly
// as they were, which is the one place SpringTutte's FixOuterFace contract
// differs from plain Tutte's (Tutte has no velocity to preserve).
static void test_springTutte_fixOuterFace_preservesVelocityOffBoundary(void) {
  size_t L = 5, W = 5;
  Graph g = BuildGrid(L, W);

  SpringTutte st(g, MakeFullSubgraph(g), 2);
  st.Begin(); // largest face (the grid's rim, 16 vertices) becomes the boundary

  size_t center = 2 * W + 2; // (2,2): far interior, degree 4
  TEST_ASSERT_FALSE(st.IsBoundaryVertex(center));

  // Displace the center vertex and run a few underdamped steps so it
  // accumulates a genuinely nonzero velocity to test preservation of.
  double displaced[2] = {40.0, -15.0};
  st.SetVPosition(center, displaced);
  for (int i = 0; i < 5; i++)
    st.Step(0.016);

  const double *p = st.GetVPosition(center);
  const double *v = st.GetVelocity(center);
  double centerPosBefore[2] = {p[0], p[1]};
  double centerVelBefore[2] = {v[0], v[1]};
  TEST_ASSERT_TRUE(centerVelBefore[0] != 0.0 || centerVelBefore[1] != 0.0);

  // A real unit-square face (per the graph's actual installed rotation,
  // found via FaceEnumerator rather than hand-guessed -- FaceWalk direction
  // is orientation-sensitive, so an arbitrarily-ordered 4-cycle isn't
  // guaranteed to reproduce the same face FixOuterFace() would walk to) that
  // does not touch the center vertex, so center is guaranteed to land off
  // the new boundary.
  FaceEnumerator faces(g, st.Structure());
  std::vector<size_t> smallFace;
  for (const auto &face : faces.Faces()) {
    if (face.size() != 4)
      continue;
    bool touchesCenter = false;
    for (size_t v : face)
      if (v == center)
        touchesCenter = true;
    if (!touchesCenter) {
      smallFace = face;
      break;
    }
  }
  TEST_ASSERT_FALSE(smallFace.empty());

  Subgraph highlight = FaceSubgraph(g, smallFace);
  st.SetHighlight(std::move(highlight));

  TEST_ASSERT_TRUE(st.FixOuterFace());
  TEST_ASSERT_EQUAL_UINT64(4, st.Boundary().size());
  TEST_ASSERT_FALSE(st.IsBoundaryVertex(center));

  const double *pAfter = st.GetVPosition(center);
  const double *vAfter = st.GetVelocity(center);
  TEST_ASSERT_EQUAL_DOUBLE(centerPosBefore[0], pAfter[0]);
  TEST_ASSERT_EQUAL_DOUBLE(centerPosBefore[1], pAfter[1]);
  TEST_ASSERT_EQUAL_DOUBLE(centerVelBefore[0], vAfter[0]);
  TEST_ASSERT_EQUAL_DOUBLE(centerVelBefore[1], vAfter[1]);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_springTutte_k4_centroid);
  RUN_TEST(test_springTutte_boundary_pinned);
  RUN_TEST(test_springTutte_seedsInteriorAndZeroesVelocity);
  RUN_TEST(test_springTutte_underdamped_overshoots);
  RUN_TEST(test_springTutte_grid_convergesOverdamped);
  RUN_TEST(test_springTutte_setBoundary_validation);
  RUN_TEST(test_springTutte_dimension_validation);
  RUN_TEST(test_springTutte_configure_zeroMeansKeepCurrent);
  RUN_TEST(test_springTutte_run_before_boundary_throws);
  RUN_TEST(test_springTutte_begin_planar_succeeds);
  RUN_TEST(test_springTutte_begin_nonplanar_throws);
  RUN_TEST(test_springTutte_begin_twice);
  RUN_TEST(test_springTutte_fixOuterFace_noHighlight);
  RUN_TEST(test_springTutte_fixOuterFace_notPlanarEmbedded);
  RUN_TEST(test_springTutte_fixOuterFace_withHighlight);
  RUN_TEST(test_springTutte_fixOuterFace_preservesVelocityOffBoundary);

  return UNITY_END();
}
