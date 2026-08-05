// Port of tests/embedders/gvizGRIPTests.c against gviz::layout::GRIP.
//
// Scenarios intentionally dropped or reshaped:
//   - The old suite's white-box filtration tests (test_filtration_*,
//     test_configureK_clampsToCapacity's direct struct-field reads) reached
//     into gvizGRIPInternal.h / gvizGRIPState's public fields directly (e.g.
//     state.misFiltration, state.misBorder, state.currLayer). GRIP's
//     internals (MakeFirstMISPartition, IterMISFiltration,
//     CreateMISFiltration, misFiltration_/misBorder_ storage) are private
//     class members with no C++ equivalent of a second "internal" header --
//     unlike C, which needed gvizGRIPInternal.h to give white-box tests/
//     benches access to free functions operating on the state struct, C++
//     private members already get that encapsulation for free, so no
//     GRIPInternal.hpp was added (see GRIP.hpp's class doc). Coverage of the
//     filtration's *observable* behavior (coarsest layer placed first,
//     layers progress from coarse to fine, K policy clamps correctly) is
//     kept below through the public API (LayerCount/CurrentLayer/
//     PlacementKMax/RefinementKMax), matching the categories the porting
//     plan called out explicitly.
//   - Manual Init/Release pairs and the 0/-1 Init return code: replaced by
//     RAII (constructor throws or succeeds) -- see test_init_* below.
//
// New coverage added beyond the old suite: dimension validation
// (DimensionError, not checked by the old C Init at all -- see GRIP.hpp's
// Config doc comment on this being a deliberate strengthening for the
// C++ port), an explicit stats-disabled-registers-nothing test, and an
// end-to-end run in 3D and 4D (the old suite only exercised 2D).

#include "GRIP.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cmath>
#include <vector>

using gviz::DimensionError;
using gviz::Graph;
using gviz::InsufficientVerticesError;
using gviz::Subgraph;
using gviz::layout::GRIP;

void setUp(void) {}
void tearDown(void) {}

static constexpr size_t kSmallMeshW = 10;
static constexpr size_t kSmallMeshH = 10;

static Graph BuildRectMesh(size_t h, size_t w) {
  Graph g(false, h * w);
  for (size_t i = 0; i < h; i++)
    for (size_t j = 0; j < w; j++)
      g.AddVertex();

  for (size_t i = 0; i < h; i++)
    for (size_t j = 0; j < w; j++) {
      size_t idx = i * w + j;
      if (j + 1 < w)
        g.AddEdge(idx, i * w + (j + 1), 1.0);
      if (i + 1 < h)
        g.AddEdge(idx, (i + 1) * w + j, 1.0);
    }

  return g;
}

static GRIP MakeMeshGRIP(Graph &g, size_t dim, GRIP::Config config = {}) {
  g.BuildLayout();
  return GRIP(Subgraph::CreateFull(g), kSmallMeshW + kSmallMeshH, dim, config);
}

// ============================================================================
// CONSTRUCTION / VALIDATION
// ============================================================================

static void test_init_rejectsBadDimension(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  bool threw = false;
  try {
    GRIP grip(std::move(sg), 0, 1); // dimension 1 is invalid
    (void)grip;
  } catch (const DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_init_rejectsBadDimensionTooHigh(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  bool threw = false;
  try {
    GRIP grip(std::move(sg), 0, 5); // dimension 5 is invalid
    (void)grip;
  } catch (const DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

// Init must reject subgraphs too small to place the coarsest simplex.
static void test_init_rejectsTooFewVertices(void) {
  Graph g(false, 2);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  bool threw = false;
  try {
    GRIP grip(std::move(sg), 1, 2); // only 2 vertices, need dim+1 = 3
    (void)grip;
  } catch (const InsufficientVerticesError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_init_exactlyDimPlusOneVerticesSucceeds(void) {
  Graph g(false, 3);
  g.AddVertex();
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);

  GRIP grip(std::move(sg), 2, 2); // exactly dim + 1 = 3 vertices
  TEST_ASSERT_EQUAL_UINT64(2, grip.Dim());
}

// ============================================================================
// BEGIN
// ============================================================================

static void test_begin_placesCoarsestSimplexWithFinitePositions(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);

  grip.Begin();

  TEST_ASSERT_TRUE(grip.LayerCount() > 0);
  TEST_ASSERT_EQUAL_UINT64(grip.LayerCount() - 1, grip.CurrentLayer());

  // The coarsest layer places exactly dim + 1 vertices, each at a finite,
  // non-origin position (a real simplex, not all collapsed to zero). We
  // don't expose misFiltration_, so find them via the public draw mask /
  // position accessors instead.
  bool anyNonZero = false;
  size_t visibleCount = 0;
  for (size_t v = 0; v < grip.PositionCount(); v++) {
    if (grip.IsVertexVisible(v)) {
      visibleCount++;
      const double *p = grip.GetVPosition(v);
      for (size_t d = 0; d < grip.Dim(); d++) {
        TEST_ASSERT_TRUE(std::isfinite(p[d]));
        if (p[d] != 0.0)
          anyNonZero = true;
      }
    }
  }
  TEST_ASSERT_EQUAL_UINT64(grip.Dim() + 1, visibleCount);
  TEST_ASSERT_TRUE(anyNonZero);
}

// ============================================================================
// NEXT STAGE
// ============================================================================

static void test_nextStage_progressesTowardFinerLayers(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);
  grip.Begin();

  size_t startLayer = grip.CurrentLayer();
  TEST_ASSERT_TRUE(startLayer > 0);

  grip.NextStage();
  TEST_ASSERT_EQUAL_UINT64(startLayer - 1, grip.CurrentLayer());
  TEST_ASSERT_EQUAL_UINT64(0, grip.CurrentRound());

  // More vertices become visible as coarser layers place their vertices.
  size_t visibleAfterOneStage = 0;
  for (size_t v = 0; v < grip.PositionCount(); v++)
    if (grip.IsVertexVisible(v))
      visibleAfterOneStage++;
  TEST_ASSERT_TRUE(visibleAfterOneStage > grip.Dim() + 1);

  // Drive all the way to layer 0; NextStage() must be a no-op once there.
  while (grip.CurrentLayer() > 0)
    grip.NextStage();
  TEST_ASSERT_EQUAL_UINT64(0, grip.CurrentLayer());
  grip.NextStage(); // no-op
  TEST_ASSERT_EQUAL_UINT64(0, grip.CurrentLayer());

  // At layer 0 every subgraph vertex is visible.
  size_t visibleAtFinest = 0;
  for (size_t v = 0; v < grip.PositionCount(); v++)
    if (grip.IsVertexVisible(v))
      visibleAtFinest++;
  TEST_ASSERT_EQUAL_UINT64(kSmallMeshW * kSmallMeshH, visibleAtFinest);
}

// ============================================================================
// REFINE ROUND
// ============================================================================

static void test_refineRound_producesFiniteStatsAndAdvancesRound(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);
  grip.Begin();

  TEST_ASSERT_EQUAL_UINT64(0, grip.CurrentRound());
  grip.RefineRound();
  TEST_ASSERT_EQUAL_UINT64(1, grip.CurrentRound());

  GRIP::RoundStats stats = grip.LastRoundStats();
  TEST_ASSERT_TRUE(std::isfinite(stats.maxDisplacement));
  TEST_ASSERT_TRUE(std::isfinite(stats.meanDisplacement));
  TEST_ASSERT_TRUE(std::isfinite(stats.meanForce));
  TEST_ASSERT_TRUE(stats.maxDisplacement >= 0.0);
}

// ============================================================================
// ACTIONS
// ============================================================================

static void test_actions_registeredAndDriveRefinement(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);

  TEST_ASSERT_NOT_NULL(grip.FindAction("grip.refineRound"));
  TEST_ASSERT_NOT_NULL(grip.FindAction("grip.nextStage"));

  // Before Begin(), both actions are safe no-ops (layerCount_ == 0 guard).
  TEST_ASSERT_TRUE(grip.InvokeAction("grip.refineRound"));
  TEST_ASSERT_EQUAL_UINT64(0, grip.CurrentRound());

  grip.Begin();
  TEST_ASSERT_TRUE(grip.InvokeAction("grip.refineRound"));
  TEST_ASSERT_EQUAL_UINT64(1, grip.CurrentRound());

  size_t layerBefore = grip.CurrentLayer();
  TEST_ASSERT_TRUE(grip.InvokeAction("grip.nextStage"));
  TEST_ASSERT_EQUAL_UINT64(layerBefore - 1, grip.CurrentLayer());
}

// ============================================================================
// K POLICY
// ============================================================================

static void test_configureK_clampsToCapacity(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.knnCapacity = 64;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);

  TEST_ASSERT_EQUAL_UINT64(64, grip.KnnCapacity());

  grip.ConfigureK(100000, 100000, GRIP::KPolicy::Constant);
  TEST_ASSERT_TRUE(grip.PlacementKMax() <= grip.KnnCapacity());
  TEST_ASSERT_TRUE(grip.RefinementKMax() <= grip.KnnCapacity());
  TEST_ASSERT_EQUAL_UINT64(64, grip.PlacementKMax());
  TEST_ASSERT_EQUAL_UINT64(64, grip.RefinementKMax());

  // 0 keeps current values.
  size_t placement = grip.PlacementKMax();
  grip.ConfigureK(0, 0, GRIP::KPolicy::Budget);
  TEST_ASSERT_EQUAL_UINT64(placement, grip.PlacementKMax());
  TEST_ASSERT_TRUE(GRIP::KPolicy::Budget == grip.Policy());
}

static void test_kPolicy_variantsAllProduceFinitePositions(void) {
  const GRIP::KPolicy policies[] = {
      GRIP::KPolicy::Constant, GRIP::KPolicy::LayerDecay,
      GRIP::KPolicy::LayerGrow, GRIP::KPolicy::PlacementDecay,
      GRIP::KPolicy::Budget,
  };

  for (GRIP::KPolicy policy : policies) {
    Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
    GRIP::Config cfg;
    cfg.statsEnabled = false;
    GRIP grip = MakeMeshGRIP(g, 2, cfg);
    grip.ConfigureK(32, 32, policy);

    grip.Embed();

    for (size_t v = 0; v < grip.PositionCount(); v++) {
      const double *p = grip.GetVPosition(v);
      TEST_ASSERT_TRUE(std::isfinite(p[0]));
      TEST_ASSERT_TRUE(std::isfinite(p[1]));
    }
  }
}

// ============================================================================
// STATS ENABLED / DISABLED
// ============================================================================

static void test_stats_disabledRegistersNoSeries(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);

  TEST_ASSERT_EQUAL_UINT64(0, grip.StatSeriesCount());
  grip.Embed();
  TEST_ASSERT_EQUAL_UINT64(0, grip.StatSeriesCount());
}

static void test_stats_enabledRecordsPerRoundSeries(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = true;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);

  TEST_ASSERT_EQUAL_UINT64(4, grip.StatSeriesCount());
  TEST_ASSERT_NOT_NULL(grip.FindStatSeries("grip.heat"));
  TEST_ASSERT_NOT_NULL(grip.FindStatSeries("grip.meanDisp"));
  TEST_ASSERT_NOT_NULL(grip.FindStatSeries("grip.maxDisp"));
  TEST_ASSERT_NOT_NULL(grip.FindStatSeries("grip.meanForce"));

  grip.Begin();
  grip.RefineRound();
  TEST_ASSERT_EQUAL_UINT64(1, grip.FindStatSeries("grip.heat")->samples.size());
}

// ============================================================================
// END-TO-END EMBED
// ============================================================================

static void RunEndToEndSpreadCheck(size_t dim) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, dim, cfg);

  grip.Embed();
  TEST_ASSERT_EQUAL_UINT64(0, grip.CurrentLayer());

  std::vector<double> lo(dim, 1e300), hi(dim, -1e300);
  for (size_t v = 0; v < grip.PositionCount(); v++) {
    const double *p = grip.GetVPosition(v);
    for (size_t d = 0; d < dim; d++) {
      TEST_ASSERT_TRUE(std::isfinite(p[d]));
      lo[d] = std::min(lo[d], p[d]);
      hi[d] = std::max(hi[d], p[d]);
    }
  }
  // The mesh must actually be spread out, not collapsed to a point.
  for (size_t d = 0; d < dim; d++)
    TEST_ASSERT_TRUE(hi[d] - lo[d] > 1.0);

  GRIP::RoundStats stats = grip.LastRoundStats();
  TEST_ASSERT_TRUE(std::isfinite(stats.maxDisplacement));
  TEST_ASSERT_TRUE(std::isfinite(stats.meanDisplacement));
}

static void test_embed_endToEnd_2D_producesSpreadFinitePositions(void) {
  RunEndToEndSpreadCheck(2);
}

static void test_embed_endToEnd_3D_producesSpreadFinitePositions(void) {
  RunEndToEndSpreadCheck(3);
}

static void test_embed_endToEnd_4D_producesSpreadFinitePositions(void) {
  RunEndToEndSpreadCheck(4);
}

// Neighboring mesh vertices should land nearer each other than far-apart
// mesh vertices do on average (sanity that the layout reflects topology).
static void test_embed_endToEnd_neighborsCloserThanFarPairs(void) {
  Graph g = BuildRectMesh(kSmallMeshH, kSmallMeshW);
  GRIP::Config cfg;
  cfg.statsEnabled = false;
  GRIP grip = MakeMeshGRIP(g, 2, cfg);

  grip.Embed();

  double meanEdgeLen = 0.0;
  size_t edgeSamples = 0;
  for (size_t i = 0; i < kSmallMeshH; i++) {
    for (size_t j = 0; j + 1 < kSmallMeshW; j++) {
      const double *a = grip.GetVPosition(i * kSmallMeshW + j);
      const double *b = grip.GetVPosition(i * kSmallMeshW + j + 1);
      meanEdgeLen += std::hypot(a[0] - b[0], a[1] - b[1]);
      edgeSamples++;
    }
  }
  meanEdgeLen /= static_cast<double>(edgeSamples);

  const double *c0 = grip.GetVPosition(0);
  const double *c1 = grip.GetVPosition(kSmallMeshH * kSmallMeshW - 1);
  double cornerDist = std::hypot(c0[0] - c1[0], c0[1] - c1[1]);

  TEST_ASSERT_TRUE(cornerDist > 3.0 * meanEdgeLen);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_init_rejectsBadDimension);
  RUN_TEST(test_init_rejectsBadDimensionTooHigh);
  RUN_TEST(test_init_rejectsTooFewVertices);
  RUN_TEST(test_init_exactlyDimPlusOneVerticesSucceeds);

  RUN_TEST(test_begin_placesCoarsestSimplexWithFinitePositions);

  RUN_TEST(test_nextStage_progressesTowardFinerLayers);

  RUN_TEST(test_refineRound_producesFiniteStatsAndAdvancesRound);

  RUN_TEST(test_actions_registeredAndDriveRefinement);

  RUN_TEST(test_configureK_clampsToCapacity);
  RUN_TEST(test_kPolicy_variantsAllProduceFinitePositions);

  RUN_TEST(test_stats_disabledRegistersNoSeries);
  RUN_TEST(test_stats_enabledRecordsPerRoundSeries);

  RUN_TEST(test_embed_endToEnd_2D_producesSpreadFinitePositions);
  RUN_TEST(test_embed_endToEnd_3D_producesSpreadFinitePositions);
  RUN_TEST(test_embed_endToEnd_4D_producesSpreadFinitePositions);
  RUN_TEST(test_embed_endToEnd_neighborsCloserThanFarPairs);

  return UNITY_END();
}
