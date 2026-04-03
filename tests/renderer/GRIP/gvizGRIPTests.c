#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"

#define MESH_W 45
#define MESH_H 85
#define MESH_D ((MESH_W - 1) + (MESH_H - 1))

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

      // right neighbor
      if (j + 1 < W) {
        idx_right = i * W + (j + 1);
        gvizGraphAddEdge(&g, idx, idx_right);
      }

      // down neighbor
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

  gvizGRIPEmbeddingInit(&state, &graph, MESH_D, 2);
  size_t layers = createMISFiltration(&state);

  for (size_t i = 1; i < layers; i++) {
    TEST_ASSERT(state.misBorder[i] <= state.misBorder[i - 1]);
  }

  gvizGRIPEmbeddingRelease(&state);
}

void test_filtration_eachVertexAppearsOnce(void) {
  gvizGRIPState state;
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGRIPEmbeddingInit(&state, &graph, MESH_D, 2);
  size_t layers = createMISFiltration(&state);
  size_t sum = 0;

  for (size_t i = 0; i < graph.vertices.count; i++) {
    sum += state.misFiltration[i];
  }

  // printf("filtration array: \n");
  // for (size_t i = 0; i < graph.vertices.count; i++) {
  //   printf("%zu, ", state.misFiltration[i]);
  // }
  // printf("\n");

  TEST_ASSERT_EQUAL((graph.vertices.count - 1) * graph.vertices.count, 2 * sum);
}

// From GRIP paper : "...each Vi is a maximal subset of Vi−1 so that the graph
// distance between any pair of its elements is at least 2i−1 + 1."
// we test maximality and spacing with this:
void test_filtration_perLayerVertexSpacingAndMaximality(void) {
  gvizGRIPState state;
  gvizGraph graph = build_rect_mesh(MESH_H, MESH_W);

  gvizGRIPEmbeddingInit(&state, &graph, MESH_D, 2);
  size_t layers = createMISFiltration(&state);

  // printf("filtration array: \n");
  // for (size_t i = 0; i < graph.vertices.count; i++) {
  //   printf("%zu, ", state.misFiltration[i]);
  // }
  // printf("\n");

  for (size_t i = 2; i < layers - 1; i++) {
    size_t end = state.misBorder[i];
    size_t minDist = (1ULL << (i - 1)) + 1;

    for (size_t a = 0; a < end; a++) {

      gvizGraph bfs;
      gvizGraphBFSTree(&graph, &bfs, state.misFiltration[a], 0, 1);
      char errStr[4096];

      // inside the current layer, means the distance is ?= to minDist
      for (size_t b = a + 1; b < end; b++) {
        size_t dist = (size_t)gvizGraphGetVertexData(
            &bfs, bfs.map[state.misFiltration[b]]);
        if (dist < minDist) {
          snprintf(errStr, sizeof(errStr),
                   "Layer: %zu, a: %zu, b: %zu, misFiltration[a]: %zu, "
                   "misFiltration[b]: %zu, minDist: %zu, dist: %zu",
                   i, a, b, state.misFiltration[a], state.misFiltration[b],
                   minDist, dist);
        }
        TEST_ASSERT_GREATER_OR_EQUAL_UINT64_MESSAGE(minDist, dist, errStr);
      }

      gvizGraphRelease(&bfs);
    }

    for (size_t a = end; a < state.misBorder[i - 1]; a++) {
      gvizGraph bfs;
      gvizGraphBFSTree(&graph, &bfs, state.misFiltration[a], 0, 1);
      int found = 0;

      for (size_t b = 0; b < end; b++) {
        size_t dist = (size_t)gvizGraphGetVertexData(
            &bfs, bfs.map[state.misFiltration[b]]);
        if (dist < minDist) {
          found = 1;
          break;
        }
      }

      gvizGraphRelease(&bfs);

      TEST_ASSERT_TRUE(found);
    }
  }

  gvizGRIPEmbeddingRelease(&state);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_filtration_nestedLayers);
  RUN_TEST(test_filtration_eachVertexAppearsOnce);
  RUN_TEST(test_filtration_perLayerVertexSpacingAndMaximality);
  return UNITY_END();
}
