#include "algorithms/search/gvizBreadthFirst.h"
#include "ds/gvizArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "embedders/gvizGRIPInternal.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"
#include <math.h>
#include <string.h>

#define MESH_W 45
#define MESH_H 25
#define MESH_D ((MESH_W - 1) + (MESH_H - 1))

#define SMALL_MESH_W 10
#define SMALL_MESH_H 10

void setUp(void) {}
void tearDown(void) {}

static size_t gripMisBorderAt(const gvizGRIPState *state, size_t i) {
  return *(const size_t *)gvizArrayAtIndex(&state->misBorder, i);
}

static gvizGraph build_rect_mesh(size_t L, size_t W) {
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, L * W);

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++)
      gvizGraphAddVertex(&g, NULL, NULL, NULL);

  size_t idx, idx_right, idx_down;

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++) {
      idx = i * W + j;

      if (j + 1 < W) {
        idx_right = i * W + (j + 1);
        gvizGraphAddEdge(&g, idx, idx_right);
      }

      if (i + 1 < L) {
        idx_down = (i + 1) * W + j;
        gvizGraphAddEdge(&g, idx, idx_down);
      }
    }

  return g;
}

void test_filtration_nestedLayers(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, MESH_D, 2);
  size_t layers = createMISFiltration(&state);

  for (size_t i = 1; i < layers; i++) {
    TEST_ASSERT(gripMisBorderAt(&state, i) <= gripMisBorderAt(&state, i - 1));
  }

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_eachVertexAppearsOnce(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, MESH_D, 2);
  size_t layers = createMISFiltration(&state);
  size_t sum = 0;

  for (size_t i = 0; i < graph.vertices.count; i++) {
    sum += state.misFiltration[i];
  }

  TEST_ASSERT_EQUAL((graph.vertices.count - 1) * graph.vertices.count, 2 * sum);
  (void)layers;

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_layer2_is_nonempty_and_builds_depth(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, 18, 2);
  size_t layers = createMISFiltration(&state);

  TEST_ASSERT_GREATER_THAN(4, layers);
  TEST_ASSERT_GREATER_THAN(0, gripMisBorderAt(&state, 1));
  TEST_ASSERT_GREATER_THAN(0, gripMisBorderAt(&state, 2));
  TEST_ASSERT_TRUE(gripMisBorderAt(&state, 2) < gripMisBorderAt(&state, 1));

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_final_layer_has_simplex_count(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, 18, 2);
  size_t layers = createMISFiltration(&state);
  size_t final = layers - 1;

  TEST_ASSERT_EQUAL_UINT64(3, gripMisBorderAt(&state, final));

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_finest_border_matches_subgraph(void) {
  gvizGRIPState state = {0};
  gvizGraph graph;
  gvizGraphInitAtCapacity(&graph, 0, 8);

  for (size_t i = 0; i < 4; i++)
    gvizGraphAddVertex(&graph, NULL, NULL, NULL);
  for (size_t i = 0; i < 4; i++)
    gvizGraphAddVertex(&graph, NULL, NULL, NULL);

  gvizGraphAddEdge(&graph, 0, 1);
  gvizGraphAddEdge(&graph, 1, 2);
  gvizGraphAddEdge(&graph, 2, 3);
  gvizGraphAddEdge(&graph, 4, 5);
  gvizGraphAddEdge(&graph, 5, 6);

  gvizGraphBuildLayout(&graph);

  gvizVertexSubset component = gvizVertexSubsetCreateEmpty(&graph);
  for (size_t v = 0; v < 4; v++)
    gvizVertexSubsetShowVertex(component, v);
  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&graph, component);

  TEST_ASSERT_EQUAL_UINT64(4, gvizSubgraphVertexCount(&sg));
  TEST_ASSERT_EQUAL_UINT64(8, gvizGraphSize(&graph));

  gvizGRIPEmbedderInit(&state, sg, 4, 2);
  size_t layers = createMISFiltration(&state);

  TEST_ASSERT_GREATER_THAN(1, layers);
  TEST_ASSERT_EQUAL_UINT64(4, gripMisBorderAt(&state, 0));

  for (size_t i = 0; i < gripMisBorderAt(&state, 0); i++) {
    TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, state.misFiltration[i]));
  }

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_perLayerVertexSpacingAndMaximality(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, MESH_D, 2);
  size_t layers = createMISFiltration(&state);

  TEST_ASSERT_GREATER_THAN(4, layers);

  gvizSubgraph fullSg = gvizSubgraphCreateFull(&graph);
  size_t distances[graph.vertices.count];

  for (size_t i = 2; i < layers - 1; i++) {
    size_t end = gripMisBorderAt(&state, i);
    size_t minDist = (1ULL << (i - 1)) + 1;

    for (size_t a = 0; a < end; a++) {
      gvizSubgraph bfs = gvizSubgraphCreateEmpty(&graph);
      gvizSearchBreadthFirst(&fullSg, &bfs, state.misFiltration[a], 0, distances);
      char errStr[4096];

      for (size_t b = a + 1; b < end; b++) {
        size_t dist = distances[state.misFiltration[b]];
        if (dist < minDist) {
          snprintf(errStr, sizeof(errStr),
                   "Layer: %zu, a: %zu, b: %zu, misFiltration[a]: %zu, "
                   "misFiltration[b]: %zu, minDist: %zu, dist: %zu",
                   i, a, b, state.misFiltration[a], state.misFiltration[b],
                   minDist, dist);
        }
        TEST_ASSERT_GREATER_OR_EQUAL_UINT64_MESSAGE(minDist, dist, errStr);
      }

      gvizSubgraphRelease(&bfs);
    }

    for (size_t a = end; a < gripMisBorderAt(&state, i - 1); a++) {
      gvizSubgraph bfs = gvizSubgraphCreateEmpty(&graph);
      gvizSearchBreadthFirst(&fullSg, &bfs, state.misFiltration[a], 0, distances);
      int found = 0;

      for (size_t b = 0; b < end; b++) {
        size_t dist = distances[state.misFiltration[b]];
        if (dist < minDist) {
          found = 1;
          break;
        }
      }

      gvizSubgraphRelease(&bfs);

      TEST_ASSERT_TRUE(found);
    }
  }

  gvizSubgraphRelease(&fullSg);

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

// End-to-end: the full pipeline must produce finite, non-collapsed 2D
// positions for every vertex of a small mesh.
void test_embed_endToEnd_producesSpreadFinitePositions(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  TEST_ASSERT_EQUAL_INT(0, gvizGRIPEmbedderInit(&state, sg, 18, 2));
  gvizGRIPEmbedderConfigureStats(&state, false);

  TEST_ASSERT_EQUAL_INT(0, gvizGRIPEmbedderEmbed(&state));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&state;
  size_t n = SMALL_MESH_H * SMALL_MESH_W;
  double minX = 1e300, maxX = -1e300, minY = 1e300, maxY = -1e300;
  for (size_t v = 0; v < n; v++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, v);
    TEST_ASSERT_TRUE(isfinite(p[0]));
    TEST_ASSERT_TRUE(isfinite(p[1]));
    if (p[0] < minX)
      minX = p[0];
    if (p[0] > maxX)
      maxX = p[0];
    if (p[1] < minY)
      minY = p[1];
    if (p[1] > maxY)
      maxY = p[1];
  }
  // The mesh must actually be spread out, not collapsed to a point.
  TEST_ASSERT_TRUE(maxX - minX > 1.0);
  TEST_ASSERT_TRUE(maxY - minY > 1.0);

  gvizGRIPRoundStats stats = gvizGRIPEmbedderLastRoundStats(&state);
  TEST_ASSERT_TRUE(isfinite(stats.maxDisplacement));
  TEST_ASSERT_TRUE(isfinite(stats.meanDisplacement));

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

// Neighboring mesh vertices should land nearer each other than far-apart
// mesh vertices do on average (sanity that the layout reflects topology).
void test_embed_endToEnd_neighborsCloserThanFarPairs(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  TEST_ASSERT_EQUAL_INT(0, gvizGRIPEmbedderInit(&state, sg, 18, 2));
  gvizGRIPEmbedderConfigureStats(&state, false);
  TEST_ASSERT_EQUAL_INT(0, gvizGRIPEmbedderEmbed(&state));

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&state;

  double meanEdgeLen = 0.0;
  size_t edgeSamples = 0;
  for (size_t i = 0; i < SMALL_MESH_H; i++) {
    for (size_t j = 0; j + 1 < SMALL_MESH_W; j++) {
      double *a = gvizEmbeddedGraphGetVPosition(eg, i * SMALL_MESH_W + j);
      double *b = gvizEmbeddedGraphGetVPosition(eg, i * SMALL_MESH_W + j + 1);
      meanEdgeLen += hypot(a[0] - b[0], a[1] - b[1]);
      edgeSamples++;
    }
  }
  meanEdgeLen /= (double)edgeSamples;

  // Opposite mesh corners.
  double *c0 = gvizEmbeddedGraphGetVPosition(eg, 0);
  double *c1 = gvizEmbeddedGraphGetVPosition(
      eg, SMALL_MESH_H * SMALL_MESH_W - 1);
  double cornerDist = hypot(c0[0] - c1[0], c0[1] - c1[1]);

  TEST_ASSERT_TRUE(cornerDist > 3.0 * meanEdgeLen);

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

// Init must reject subgraphs too small to place the coarsest simplex.
void test_init_rejectsTooFewVertices(void) {
  gvizGRIPState state = {0};
  gvizGraph graph;
  gvizGraphInitAtCapacity(&graph, 0, 2);
  gvizGraphAddVertex(&graph, NULL, NULL, NULL);
  gvizGraphAddVertex(&graph, NULL, NULL, NULL);
  gvizGraphAddEdge(&graph, 0, 1);
  gvizGraphBuildLayout(&graph);

  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  TEST_ASSERT_EQUAL_INT(-1, gvizGRIPEmbedderInit(&state, sg, 1, 2));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&graph);
}

void test_configureK_clampsToCapacity(void) {
  gvizGRIPState state = {0};
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);
  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  TEST_ASSERT_EQUAL_INT(0, gvizGRIPEmbedderInit(&state, sg, 18, 2));

  gvizGRIPEmbedderConfigureK(&state, 100000, 100000, GVIZ_GRIP_K_CONSTANT);
  TEST_ASSERT_TRUE(state.placementKMax <= state.knnCapacity);
  TEST_ASSERT_TRUE(state.refinementKMax <= state.knnCapacity);

  // 0 keeps current values
  size_t placement = state.placementKMax;
  gvizGRIPEmbedderConfigureK(&state, 0, 0, GVIZ_GRIP_K_BUDGET);
  TEST_ASSERT_EQUAL_size_t(placement, state.placementKMax);

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_filtration_nestedLayers);
  RUN_TEST(test_filtration_eachVertexAppearsOnce);
  RUN_TEST(test_filtration_layer2_is_nonempty_and_builds_depth);
  RUN_TEST(test_filtration_final_layer_has_simplex_count);
  RUN_TEST(test_filtration_finest_border_matches_subgraph);
  RUN_TEST(test_filtration_perLayerVertexSpacingAndMaximality);
  RUN_TEST(test_embed_endToEnd_producesSpreadFinitePositions);
  RUN_TEST(test_embed_endToEnd_neighborsCloserThanFarPairs);
  RUN_TEST(test_init_rejectsTooFewVertices);
  RUN_TEST(test_configureK_clampsToCapacity);
  return UNITY_END();
}
