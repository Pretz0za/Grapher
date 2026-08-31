// New coverage for gviz::layout::KamadaKawai (no old C counterpart existed --
// this is a from-scratch port of Kamada & Kawai's 1989 graph-theoretic-
// distance layout algorithm, following this codebase's established
// embedder-test shape, closest in spirit to TutteTests.cpp).

#include "KamadaKawai.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cmath>
#include <cstdint>
#include <stdexcept>

using gviz::DimensionError;
using gviz::Graph;
using gviz::NotConnectedError;
using gviz::Subgraph;
using gviz::layout::KamadaKawai;

void setUp(void) {}
void tearDown(void) {}

static Subgraph MakeInducedSubgraph(Graph &g) {
  // Deliberately vertex-induced (the preferred kind everywhere per
  // CLAUDE.md), never pre-built with a layout -- demonstrates that
  // KamadaKawai does not require the caller to call Graph::BuildLayout()
  // first, unlike Tutte's tests (which always build a full subgraph).
  // CreateVertexInduced starts with every vertex bit unset, so every
  // vertex must be shown explicitly (matches ForceAtlasTests.cpp's usage).
  Subgraph sg = Subgraph::CreateVertexInduced(g);
  for (size_t i = 0; i < g.Size(); i++)
    sg.ShowVertex(i);
  return sg;
}

static Graph BuildCycle(size_t n) {
  Graph g(false);
  for (size_t i = 0; i < n; i++)
    g.AddVertex();
  for (size_t i = 0; i < n; i++)
    g.AddEdge(i, (i + 1) % n, 1.0);
  return g;
}

static Graph BuildPath(size_t n) {
  Graph g(false);
  for (size_t i = 0; i < n; i++)
    g.AddVertex();
  for (size_t i = 0; i + 1 < n; i++)
    g.AddEdge(i, i + 1, 1.0);
  return g;
}

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

static Graph BuildTwoDisjointTriangles() {
  Graph g(false);
  for (int i = 0; i < 6; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 0, 1.0);
  g.AddEdge(3, 4, 1.0);
  g.AddEdge(4, 5, 1.0);
  g.AddEdge(5, 3, 1.0);
  return g;
}

static double Dist(const double *a, const double *b, size_t dim) {
  double sum = 0.0;
  for (size_t k = 0; k < dim; k++) {
    double d = a[k] - b[k];
    sum += d * d;
  }
  return std::sqrt(sum);
}

// Dimension < 1 must be rejected at construction.
static void test_kk_dimension_validation(void) {
  Graph g = BuildK4();
  bool threw = false;
  try {
    KamadaKawai kk(MakeInducedSubgraph(g), 0);
    (void)kk;
  } catch (const DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// Begin() on a disconnected graph must throw NotConnectedError, and must
// not leave the object "begun" (Begun() stays false, Run() still throws
// the usage-order error afterward).
static void test_kk_begin_disconnected_throws(void) {
  Graph g = BuildTwoDisjointTriangles();
  KamadaKawai kk(MakeInducedSubgraph(g), 2);

  bool threw = false;
  try {
    kk.Begin();
  } catch (const NotConnectedError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
  TEST_ASSERT_FALSE(kk.Begun());
}

// Run() before Begin() is a usage-order error.
static void test_kk_run_before_begin_throws(void) {
  Graph g = BuildK4();
  KamadaKawai kk(MakeInducedSubgraph(g), 2);

  bool threw = false;
  try {
    kk.Run(10);
  } catch (const std::logic_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// GraphDistance() must match hand-computed hop counts on a path graph, and
// report SIZE_MAX before Begin() has run.
static void test_kk_graphDistance_matches_bfs(void) {
  Graph g = BuildPath(4); // 0-1-2-3
  KamadaKawai kk(MakeInducedSubgraph(g), 2);

  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, kk.GraphDistance(0, 3));

  kk.Begin();
  TEST_ASSERT_EQUAL_UINT64(0, kk.GraphDistance(0, 0));
  TEST_ASSERT_EQUAL_UINT64(1, kk.GraphDistance(0, 1));
  TEST_ASSERT_EQUAL_UINT64(2, kk.GraphDistance(0, 2));
  TEST_ASSERT_EQUAL_UINT64(3, kk.GraphDistance(0, 3));
  TEST_ASSERT_EQUAL_UINT64(1, kk.GraphDistance(2, 3));
}

// A small cycle must converge within a reasonable iteration budget, with
// the max-gradient stat trending down to below Epsilon().
static void test_kk_cycle_converges(void) {
  Graph g = BuildCycle(8);
  KamadaKawai kk(MakeInducedSubgraph(g), 2, 50.0, 1e-3);

  kk.Begin();
  size_t iters = kk.Run(20000);

  TEST_ASSERT_TRUE(kk.Converged());
  TEST_ASSERT_LESS_THAN(20000, iters);
  TEST_ASSERT_TRUE(kk.LastMaxGradient() < kk.Epsilon());

  // Regularity check: every vertex should end up roughly equidistant from
  // the centroid (a converged cycle lays out close to a regular polygon).
  double centroid[2] = {0.0, 0.0};
  for (size_t u = 0; u < 8; u++) {
    const double *p = kk.GetVPosition(u);
    centroid[0] += p[0];
    centroid[1] += p[1];
  }
  centroid[0] /= 8.0;
  centroid[1] /= 8.0;

  double radii[8];
  double meanRadius = 0.0;
  for (size_t u = 0; u < 8; u++) {
    const double *p = kk.GetVPosition(u);
    radii[u] = Dist(p, centroid, 2);
    meanRadius += radii[u];
  }
  meanRadius /= 8.0;

  for (size_t u = 0; u < 8; u++)
    TEST_ASSERT_DOUBLE_WITHIN(0.25 * meanRadius, meanRadius, radii[u]);
}

// A path graph's exact global energy minimum is a straight line with
// vertices spaced EdgeLength() apart (|P_i - P_j| == L * |i - j| holds
// simultaneously for every pair on such a line) -- Begin()+Run() should
// converge close to that configuration.
static void test_kk_path_roughly_collinear(void) {
  size_t n = 6;
  Graph g = BuildPath(n);
  KamadaKawai kk(MakeInducedSubgraph(g), 2, 40.0, 1e-3);

  kk.Begin();
  size_t iters = kk.Run(20000);

  TEST_ASSERT_TRUE(kk.Converged());
  TEST_ASSERT_LESS_THAN(20000, iters);

  const double *p0 = kk.GetVPosition(0);
  for (size_t i = 1; i < n; i++) {
    const double *pi = kk.GetVPosition(i);
    double expected = kk.EdgeLength() * static_cast<double>(i);
    double actual = Dist(p0, pi, 2);
    TEST_ASSERT_DOUBLE_WITHIN(0.15 * expected, expected, actual);
  }
}

// A path graph's collinear zero-energy optimum is dimension-agnostic (an
// exact Euclidean path metric only embeds isometrically as a straight
// line, in any ambient dimension), so this re-runs the 2D collinearity
// check in 3D specifically to exercise the Dim() != 2 initial-placement
// fallback (RandomizePositions) -- Begin()'s circular seed only applies at
// Dim() == 2. (K4 was tried here first: it also has a zero-energy regular
// -tetrahedron optimum, but K4's extra symmetry means Newton-Raphson's
// per-vertex greedy descent can just as validly settle into a different,
// non-regular stationary point -- e.g. an isosceles "digonal disphenoid"
// with two opposite edges longer than the other four -- so it isn't a
// reliable invariant to assert on; a path's optimum has no such
// degeneracy.)
static void test_kk_path_3d_dimension_fallback(void) {
  size_t n = 5;
  Graph g = BuildPath(n);
  KamadaKawai kk(MakeInducedSubgraph(g), 3, 30.0, 1e-3);

  kk.Begin();
  size_t iters = kk.Run(20000);

  TEST_ASSERT_TRUE(kk.Converged());
  TEST_ASSERT_LESS_THAN(20000, iters);

  const double *p0 = kk.GetVPosition(0);
  for (size_t i = 1; i < n; i++) {
    const double *pi = kk.GetVPosition(i);
    double expected = kk.EdgeLength() * static_cast<double>(i);
    double actual = Dist(p0, pi, 3);
    TEST_ASSERT_DOUBLE_WITHIN(0.15 * expected, expected, actual);
  }
}

// Begin() is safe to call a second time: resets Iteration()/Converged() and
// rebuilds the distance table from scratch.
static void test_kk_begin_twice(void) {
  Graph g = BuildCycle(6);
  KamadaKawai kk(MakeInducedSubgraph(g), 2, 50.0, 1e-3);

  kk.Begin();
  kk.Run(20000);
  TEST_ASSERT_TRUE(kk.Converged());

  kk.Begin();
  TEST_ASSERT_EQUAL_UINT64(0, kk.Iteration());
  TEST_ASSERT_FALSE(kk.Converged());

  kk.Run(20000);
  TEST_ASSERT_TRUE(kk.Converged());
}

// SetStiffness()/Stiffness() is a plain post-construction knob independent
// of EdgeLength()/Epsilon() (both fixed at construction).
static void test_kk_stiffness_accessor(void) {
  Graph g = BuildK4();
  KamadaKawai kk(MakeInducedSubgraph(g), 2);

  TEST_ASSERT_EQUAL_DOUBLE(KamadaKawai<Subgraph>::kDefaultStiffness, kk.Stiffness());
  kk.SetStiffness(2.5);
  TEST_ASSERT_EQUAL_DOUBLE(2.5, kk.Stiffness());
}

// An empty/trivial (single-vertex) subgraph is trivially connected and
// must not throw; Step() on it is a well-defined no-op that reports
// immediate convergence.
static void test_kk_single_vertex_trivially_connected(void) {
  Graph g(false);
  g.AddVertex();
  KamadaKawai kk(MakeInducedSubgraph(g), 2);

  kk.Begin();
  TEST_ASSERT_TRUE(kk.Begun());

  double grad = kk.Step();
  TEST_ASSERT_EQUAL_DOUBLE(0.0, grad);
  TEST_ASSERT_TRUE(kk.Converged());
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_kk_dimension_validation);
  RUN_TEST(test_kk_begin_disconnected_throws);
  RUN_TEST(test_kk_run_before_begin_throws);
  RUN_TEST(test_kk_graphDistance_matches_bfs);
  RUN_TEST(test_kk_cycle_converges);
  RUN_TEST(test_kk_path_roughly_collinear);
  RUN_TEST(test_kk_path_3d_dimension_fallback);
  RUN_TEST(test_kk_begin_twice);
  RUN_TEST(test_kk_stiffness_accessor);
  RUN_TEST(test_kk_single_vertex_trivially_connected);

  return UNITY_END();
}
