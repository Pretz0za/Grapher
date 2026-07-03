#include "dsa/gvizGraph.h"
#include "dsa/gvizSubgraph.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"

#define MESH_W 45
#define MESH_H 85
#define MESH_D ((MESH_W - 1) + (MESH_H - 1))

#define SMALL_MESH_W 10
#define SMALL_MESH_H 10

void setUp(void) {}
void tearDown(void) {}

gvizGraph build_rect_mesh(size_t L, size_t W) {
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
  gvizGRIPState state;
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, MESH_D, 2);
  size_t layers = createMISFiltration(&state);

  for (size_t i = 1; i < layers; i++) {
    TEST_ASSERT(state.misBorder[i] <= state.misBorder[i - 1]);
  }

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_eachVertexAppearsOnce(void) {
  gvizGRIPState state;
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
  gvizGRIPState state;
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, 18, 2);
  size_t layers = createMISFiltration(&state);

  TEST_ASSERT_GREATER_THAN(4, layers);
  TEST_ASSERT_GREATER_THAN(0, state.misBorder[1]);
  TEST_ASSERT_GREATER_THAN(0, state.misBorder[2]);
  TEST_ASSERT_TRUE(state.misBorder[2] < state.misBorder[1]);

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_final_layer_has_simplex_count(void) {
  gvizGRIPState state;
  gvizGraph graph = build_rect_mesh(SMALL_MESH_H, SMALL_MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, 18, 2);
  size_t layers = createMISFiltration(&state);
  size_t final = layers - 1;

  TEST_ASSERT_EQUAL_UINT64(3, state.misBorder[final]);

  gvizGRIPEmbedderRelease(&state);
  gvizGraphRelease(&graph);
}

void test_filtration_perLayerVertexSpacingAndMaximality(void) {
  gvizGRIPState state;
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGraphBuildLayout(&graph);
  gvizSubgraph sg = gvizSubgraphCreateFull(&graph);
  gvizGRIPEmbedderInit(&state, sg, MESH_D, 2);
  size_t layers = createMISFiltration(&state);

  TEST_ASSERT_GREATER_THAN(4, layers);

  gvizSubgraph fullSg = gvizSubgraphCreateFull(&graph);
  size_t distances[graph.vertices.count];

  for (size_t i = 2; i < layers - 1; i++) {
    size_t end = state.misBorder[i];
    size_t minDist = (1ULL << (i - 1)) + 1;

    for (size_t a = 0; a < end; a++) {
      gvizSubgraph bfs = gvizSubgraphCreateEmpty(&graph);
      gvizSubgraphBFSTree(&fullSg, &bfs, state.misFiltration[a], 0, distances);
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

    for (size_t a = end; a < state.misBorder[i - 1]; a++) {
      gvizSubgraph bfs = gvizSubgraphCreateEmpty(&graph);
      gvizSubgraphBFSTree(&fullSg, &bfs, state.misFiltration[a], 0, distances);
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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_filtration_nestedLayers);
  RUN_TEST(test_filtration_eachVertexAppearsOnce);
  RUN_TEST(test_filtration_layer2_is_nonempty_and_builds_depth);
  RUN_TEST(test_filtration_final_layer_has_simplex_count);
  RUN_TEST(test_filtration_perLayerVertexSpacingAndMaximality);
  return UNITY_END();
}
