#include "unity/unity.h"
#include "algorithms/search/gvizBreadthFirst.h"
#include "algorithms/search/gvizConnectedComponents.h"
#include "algorithms/search/gvizDepthFirst.h"
#include "algorithms/search/gvizKNearest.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void add_path4(gvizGraph *g) {
  gvizGraphInit(g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 3);
}

static void add_triangle(gvizGraph *g) {
  gvizGraphInit(g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 0, 2);
}

void test_searchBreadthFirst_path(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  size_t distances[4];
  TEST_ASSERT_EQUAL_INT(0, gvizSearchBreadthFirst(&sg, &tree, 0, 0, distances));
  TEST_ASSERT_EQUAL_UINT64(4, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(3, gvizSubgraphEdgeCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(0, distances[0]);
  TEST_ASSERT_EQUAL_UINT64(1, distances[1]);
  TEST_ASSERT_EQUAL_UINT64(2, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(3, distances[3]);

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchBreadthFirst_maxDepth(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  size_t distances[4];
  memset(distances, 0xFF, sizeof(distances));
  TEST_ASSERT_EQUAL_INT(0, gvizSearchBreadthFirst(&sg, &tree, 0, 1, distances));
  TEST_ASSERT_EQUAL_UINT64(2, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(1, gvizSubgraphEdgeCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(0, distances[0]);
  TEST_ASSERT_EQUAL_UINT64(1, distances[1]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[3]);

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchDepthFirst_path(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  TEST_ASSERT_EQUAL_INT(0, gvizSearchDepthFirst(&sg, &tree, 0));
  TEST_ASSERT_EQUAL_UINT64(4, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(3, gvizSubgraphEdgeCount(&tree));

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_triangle(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizFoundVertex found[2];

  int count = gvizSearchKNearest(&sg, found, 2, 0, NULL);
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].dist);
  TEST_ASSERT_EQUAL_UINT64(2, found[1].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[1].dist);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_withFilter(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizVertexSubset filter = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(filter, 2);

  gvizFoundVertex found[1];
  int count = gvizSearchKNearest(&sg, found, 1, 0, filter);
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_EQUAL_UINT64(2, found[0].v);
  TEST_ASSERT_EQUAL_UINT64(1, found[0].dist);

  gvizVertexSubsetRelease(filter);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_connectedComponents_singleComponent(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  size_t labels[3];
  size_t count = 0;

  TEST_ASSERT_EQUAL_INT(0, gvizConnectedComponents(&sg, labels, &count));
  TEST_ASSERT_EQUAL_UINT64(1, count);
  TEST_ASSERT_EQUAL_UINT64(0, labels[0]);
  TEST_ASSERT_EQUAL_UINT64(0, labels[1]);
  TEST_ASSERT_EQUAL_UINT64(0, labels[2]);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_connectedComponents_disconnected(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 5; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  size_t labels[5];
  size_t sizes[3];
  size_t count = 0;

  TEST_ASSERT_EQUAL_INT(0, gvizConnectedComponents(&sg, labels, &count));
  TEST_ASSERT_EQUAL_UINT64(3, count);
  TEST_ASSERT_EQUAL_UINT64(0, labels[0]);
  TEST_ASSERT_EQUAL_UINT64(0, labels[1]);
  TEST_ASSERT_EQUAL_UINT64(1, labels[2]);
  TEST_ASSERT_EQUAL_UINT64(1, labels[3]);
  TEST_ASSERT_EQUAL_UINT64(2, labels[4]);

  memset(sizes, 0, sizeof(sizes));
  gvizConnectedComponentSizes(labels, 5, count, sizes);
  TEST_ASSERT_EQUAL_UINT64(2, sizes[0]);
  TEST_ASSERT_EQUAL_UINT64(2, sizes[1]);
  TEST_ASSERT_EQUAL_UINT64(1, sizes[2]);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_scratch_matches_stateless(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizKNearestScratch scratch;
  TEST_ASSERT_EQUAL_INT(0, gvizKNearestScratchInit(&scratch, 4));

  gvizFoundVertex a[3];
  gvizFoundVertex b[3];
  int countA = gvizSearchKNearest(&sg, a, 3, 0, NULL);
  int countB =
      gvizSearchKNearestScratch(&sg, b, 3, 0, NULL, &scratch);
  TEST_ASSERT_EQUAL_INT(countA, countB);
  for (int i = 0; i < countA; i++) {
    TEST_ASSERT_EQUAL_UINT64(a[i].v, b[i].v);
    TEST_ASSERT_EQUAL_UINT64(a[i].dist, b[i].dist);
  }

  gvizKNearestScratchRelease(&scratch);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchBreadthFirst_invalidInputs(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);

  // source outside the subgraph
  gvizSubgraphHideVertex(&sg, 2);
  TEST_ASSERT_EQUAL_INT(-1, gvizSearchBreadthFirst(&sg, &tree, 2, 0, NULL));
  // NULL subgraph / output
  TEST_ASSERT_EQUAL_INT(-1, gvizSearchBreadthFirst(NULL, &tree, 0, 0, NULL));
  TEST_ASSERT_EQUAL_INT(-1, gvizSearchBreadthFirst(&sg, NULL, 0, 0, NULL));

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

// BFS in a subgraph must not walk through hidden vertices.
void test_searchBreadthFirst_respectsHiddenVertices(void) {
  gvizGraph g;
  add_path4(&g); // 0-1-2-3
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraphHideVertex(&sg, 1); // cuts the path

  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);
  size_t distances[4];
  TEST_ASSERT_EQUAL_INT(0, gvizSearchBreadthFirst(&sg, &tree, 0, 0, distances));
  TEST_ASSERT_EQUAL_UINT64(1, gvizSubgraphVertexCount(&tree));
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[2]);
  TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, distances[3]);

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchDepthFirst_invalidSource(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizSubgraph tree = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphHideVertex(&sg, 3);

  TEST_ASSERT_EQUAL_INT(-1, gvizSearchDepthFirst(&sg, &tree, 3));
  TEST_ASSERT_EQUAL_INT(-1, gvizSearchDepthFirst(NULL, &tree, 0));

  gvizSubgraphRelease(&tree);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_connectedComponents_emptySubgraph(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  // vertex-induced subgraph with no vertices shown
  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);

  size_t labels[3];
  size_t count = 42;
  TEST_ASSERT_EQUAL_INT(0, gvizConnectedComponents(&sg, labels, &count));
  TEST_ASSERT_EQUAL_UINT64(0, count);
  for (int i = 0; i < 3; i++)
    TEST_ASSERT_EQUAL_UINT64(SIZE_MAX, labels[i]);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_kLargerThanGraph(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  gvizFoundVertex found[10];
  int count = gvizSearchKNearest(&sg, found, 10, 0, NULL);
  TEST_ASSERT_EQUAL_INT(2, count); // only 2 other vertices exist

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_searchKNearest_zeroK(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  TEST_ASSERT_EQUAL_INT(0, gvizSearchKNearest(&sg, NULL, 0, 0, NULL));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

// The batched multi-source path must agree with per-vertex BFS results.
void test_searchKNearest_batchMatchesPerVertex(void) {
  enum { N = 30 };
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, N);
  for (int i = 0; i < N; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int i = 0; i + 1 < N; i++)
    gvizGraphAddEdge(&g, i, i + 1); // path graph
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateFull(&g);

  // 2 visible seeds, everyone else is a target.
  gvizVertexSubset visible = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(visible, 0);
  gvizVertexSubsetShowVertex(visible, N - 1);

  enum { K = 2 };
  gvizKNNBatchTarget targets[N - 2];
  gvizFoundVertex batchOut[(N - 2) * K];
  size_t t = 0;
  for (size_t v = 1; v + 1 < N; v++, t++) {
    targets[t].vertex = v;
    targets[t].out = batchOut + t * K;
    targets[t].count = 0;
  }

  gvizKNearestScratch scratch;
  TEST_ASSERT_EQUAL_INT(0, gvizKNearestScratchInit(&scratch, N));
  TEST_ASSERT_EQUAL_INT(0,
                        gvizSearchKNearestFromVisibleBatch(
                            &sg, visible, K, targets, N - 2, &scratch));

  for (t = 0; t < N - 2; t++) {
    gvizFoundVertex direct[K];
    int directCount = gvizSearchKNearestScratch(&sg, direct, K,
                                                targets[t].vertex, visible,
                                                &scratch);
    TEST_ASSERT_EQUAL_INT(directCount, (int)targets[t].count);
    // Compare distances (vertex order may differ on ties).
    for (int i = 0; i < directCount; i++)
      TEST_ASSERT_EQUAL_UINT64(direct[i].dist, targets[t].out[i].dist);
  }

  gvizKNearestScratchRelease(&scratch);
  gvizVertexSubsetRelease(visible);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_searchBreadthFirst_path);
  RUN_TEST(test_searchBreadthFirst_maxDepth);
  RUN_TEST(test_searchDepthFirst_path);
  RUN_TEST(test_connectedComponents_singleComponent);
  RUN_TEST(test_connectedComponents_disconnected);
  RUN_TEST(test_searchKNearest_triangle);
  RUN_TEST(test_searchKNearest_withFilter);
  RUN_TEST(test_searchKNearest_scratch_matches_stateless);
  return UNITY_END();
}
