#include "dsa/graph.h"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

// ============================================================================
// VERTEX TESTS
// ============================================================================

void test_vertexInit_valid(void) {
  Vertex v;
  int data = 42;

  TEST_ASSERT_EQUAL_INT(0, vertexInit(&v, &data));
  TEST_ASSERT_EQUAL_PTR(&data, v.data);
  TEST_ASSERT_NOT_NULL(v.neighbors.arr);
  TEST_ASSERT_EQUAL_INT(0, v.neighbors.count);

  vertexRelease(&v);
}

void test_vertexInit_null_vertex(void) {
  int data = 42;
  TEST_ASSERT_EQUAL_INT(-1, vertexInit(NULL, &data));
}

void test_vertexInit_null_data_allowed(void) {
  Vertex v;
  TEST_ASSERT_EQUAL_INT(0, vertexInit(&v, NULL));
  TEST_ASSERT_NULL(v.data);
  vertexRelease(&v);
}

void test_vertexInitAtCapacity(void) {
  Vertex v;
  int data = 10;

  TEST_ASSERT_EQUAL_INT(0, vertexInitAtCapacity(&v, &data, 16));
  TEST_ASSERT_EQUAL_PTR(&data, v.data);
  TEST_ASSERT_GREATER_OR_EQUAL(v.neighbors.capacity, 16);

  vertexRelease(&v);
}

void test_vertexCopy_basic(void) {
  Vertex src, dest;
  int data1 = 100, data2 = 200;

  vertexInit(&src, &data1);
  vertexInit(&dest, &data2);

  src.neighbors.arr[src.neighbors.count++] = 5;
  src.neighbors.arr[src.neighbors.count++] = 10;

  TEST_ASSERT_EQUAL_INT(0, vertexCopy(&dest, &src));
  TEST_ASSERT_EQUAL_PTR(&data2, dest.data);
  TEST_ASSERT_EQUAL_INT(2, dest.neighbors.count);
  TEST_ASSERT_EQUAL_INT(5, dest.neighbors.arr[0]);
  TEST_ASSERT_EQUAL_INT(10, dest.neighbors.arr[1]);
  TEST_ASSERT_NOT_EQUAL_UINT64(src.neighbors.arr, dest.neighbors.arr);

  vertexRelease(&src);
  vertexRelease(&dest);
}

void test_vertexCopy_null_pointers(void) {
  Vertex v;
  vertexInit(&v, NULL);

  TEST_ASSERT_EQUAL_INT(-1, vertexCopy(NULL, &v));
  TEST_ASSERT_EQUAL_INT(-1, vertexCopy(&v, NULL));

  vertexRelease(&v);
}

void test_vertexClone_basic(void) {
  Vertex src, dest;
  int data = 99;

  vertexInit(&src, &data);
  src.neighbors.arr[src.neighbors.count++] = 1;
  src.neighbors.arr[src.neighbors.count++] = 2;
  src.neighbors.arr[src.neighbors.count++] = 3;

  TEST_ASSERT_EQUAL_INT(0, vertexClone(&dest, &src));
  TEST_ASSERT_EQUAL_INT(3, dest.neighbors.count);
  TEST_ASSERT_EQUAL_INT(1, dest.neighbors.arr[0]);
  TEST_ASSERT_EQUAL_INT(2, dest.neighbors.arr[1]);
  TEST_ASSERT_EQUAL_INT(3, dest.neighbors.arr[2]);

  vertexRelease(&src);
  vertexRelease(&dest);
}

// ============================================================================
// GRAPH INITIALIZATION & BASIC OPERATIONS
// ============================================================================

void test_graphInit_directed(void) {
  Graph g;

  TEST_ASSERT_EQUAL_INT(0, graphInit(&g, 1));
  TEST_ASSERT_EQUAL_INT(1, g.directed);
  TEST_ASSERT_EQUAL_INT(0, g.count);
  TEST_ASSERT_NOT_NULL(g.vertices);
  TEST_ASSERT_GREATER_THAN(g.size, 0);

  graphRelease(&g);
}

void test_graphInit_undirected(void) {
  Graph g;

  TEST_ASSERT_EQUAL_INT(0, graphInit(&g, 0));
  TEST_ASSERT_EQUAL_INT(0, g.directed);

  graphRelease(&g);
}

void test_graphInitAtCapacity(void) {
  Graph g;

  TEST_ASSERT_EQUAL_INT(0, graphInitAtCapacity(&g, 1, 32));
  TEST_ASSERT_GREATER_OR_EQUAL(g.size, 32);

  graphRelease(&g);
}

void test_graphAddVertex_single(void) {
  Graph g;
  int data = 42;

  graphInit(&g, 1);
  TEST_ASSERT_EQUAL_INT(0, graphAddVertex(&g, &data, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(1, g.count);
  TEST_ASSERT_EQUAL_PTR(&data, g.vertices[0].data);

  graphRelease(&g);
}

void test_graphAddVertex_multiple(void) {
  Graph g;
  int d1 = 1, d2 = 2, d3 = 3;

  graphInit(&g, 0);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);
  graphAddVertex(&g, &d3, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(3, g.count);

  graphRelease(&g);
}

void test_graphAddVertex_capacity_growth(void) {
  Graph g;
  int data = 7;

  graphInitAtCapacity(&g, 1, 2);
  size_t initial_size = g.size;

  graphAddVertex(&g, &data, NULL, NULL);
  graphAddVertex(&g, &data, NULL, NULL);
  graphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(3, g.count);
  TEST_ASSERT_GREATER_THAN(g.size, initial_size);

  graphRelease(&g);
}

// ============================================================================
// EDGES
// ============================================================================

void test_graphAddEdge_directed(void) {
  Graph g;
  int d1 = 1, d2 = 2;

  graphInit(&g, 1);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(0, graphAddEdge(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, g.vertices[0].neighbors.count);
  TEST_ASSERT_EQUAL_INT(1, g.vertices[0].neighbors.arr[0]);
  TEST_ASSERT_EQUAL_INT(0, g.vertices[1].neighbors.count);

  graphRelease(&g);
}

void test_graphAddEdge_undirected(void) {
  Graph g;
  int d1 = 1, d2 = 2;

  graphInit(&g, 0);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(0, graphAddEdge(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, g.vertices[0].neighbors.count);
  TEST_ASSERT_EQUAL_INT(1, g.vertices[1].neighbors.count);

  graphRelease(&g);
}

void test_graphAddEdge_out_of_bounds(void) {
  Graph g;
  int data = 1;

  graphInit(&g, 1);
  graphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(-1, graphAddEdge(&g, 0, 5));
  TEST_ASSERT_EQUAL_INT(-1, graphAddEdge(&g, 10, 0));

  graphRelease(&g);
}

void test_graphAddEdge_self_loop(void) {
  Graph g;
  int data = 1;

  graphInit(&g, 1);
  graphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(0, graphAddEdge(&g, 0, 0));
  TEST_ASSERT_EQUAL_INT(1, g.vertices[0].neighbors.count);

  graphRelease(&g);
}

void test_graphEdgeExists_true(void) {
  Graph g;
  int d1 = 1, d2 = 2;

  graphInit(&g, 1);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);
  graphAddEdge(&g, 0, 1);

  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, graphEdgeExists(&g, 1, 0));

  graphRelease(&g);
}

void test_graphEdgeExists_out_of_bounds(void) {
  Graph g;
  int data = 1;

  graphInit(&g, 1);
  graphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(-1, graphEdgeExists(&g, 0, 5));
  TEST_ASSERT_EQUAL_INT(-1, graphEdgeExists(&g, 5, 0));

  graphRelease(&g);
}

void test_graphRemoveEdge_directed(void) {
  Graph g;
  int d1 = 1, d2 = 2;

  graphInit(&g, 1);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);
  graphAddEdge(&g, 0, 1);

  TEST_ASSERT_EQUAL_INT(1, g.vertices[0].neighbors.count);
  TEST_ASSERT_EQUAL_INT(0, graphRemoveEdge(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, g.vertices[0].neighbors.count);

  graphRelease(&g);
}

void test_graphRemoveEdge_undirected(void) {
  Graph g;
  int d1 = 1, d2 = 2;

  graphInit(&g, 0);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);
  graphAddEdge(&g, 0, 1);

  TEST_ASSERT_EQUAL_INT(0, graphRemoveEdge(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, g.vertices[0].neighbors.count);
  TEST_ASSERT_EQUAL_INT(0, g.vertices[1].neighbors.count);

  graphRelease(&g);
}

void test_graphRemoveEdge_nonexistent(void) {
  Graph g;
  int d1 = 1, d2 = 2;

  graphInit(&g, 1);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(-1, graphRemoveEdge(&g, 0, 1));

  graphRelease(&g);
}

// ============================================================================
// GRAPH QUERIES
// ============================================================================

void test_graphGetVertexData(void) {
  Graph g;
  int d1 = 11, d2 = 22;

  graphInit(&g, 1);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);

  TEST_ASSERT_EQUAL_PTR(&d1, graphGetVertexData(&g, 0));
  TEST_ASSERT_EQUAL_PTR(&d2, graphGetVertexData(&g, 1));
  TEST_ASSERT_NULL(graphGetVertexData(&g, 5));

  graphRelease(&g);
}

void test_graphGetVertexNeighbors(void) {
  Graph g;
  int d1 = 1, d2 = 2, d3 = 3;

  graphInit(&g, 1);
  graphAddVertex(&g, &d1, NULL, NULL);
  graphAddVertex(&g, &d2, NULL, NULL);
  graphAddVertex(&g, &d3, NULL, NULL);
  graphAddEdge(&g, 0, 1);
  graphAddEdge(&g, 0, 2);

  ULongArray *neighbors = graphGetVertexNeighbors(&g, 0);
  TEST_ASSERT_NOT_NULL(neighbors);
  TEST_ASSERT_EQUAL_INT(2, neighbors->count);

  TEST_ASSERT_NULL(graphGetVertexNeighbors(&g, 10));

  graphRelease(&g);
}

// ============================================================================
// COPY & CLONE
// ============================================================================

void test_graphCopy_simple(void) {
  Graph src, dest;
  int d1 = 1, d2 = 2;

  graphInit(&src, 1);
  graphAddVertex(&src, &d1, NULL, NULL);
  graphAddVertex(&src, &d2, NULL, NULL);
  graphAddEdge(&src, 0, 1);

  graphInit(&dest, 0);
  TEST_ASSERT_EQUAL_INT(0, graphCopy(&dest, &src));

  TEST_ASSERT_EQUAL_INT(1, dest.directed);
  TEST_ASSERT_EQUAL_INT(2, dest.count);
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 0, 1));
  TEST_ASSERT_NOT_EQUAL_UINT64(src.vertices, dest.vertices);

  graphRelease(&src);
  graphRelease(&dest);
}

void test_graphCopy_preserves_dest_vertex_data(void) {
  Graph src, dest;
  int s1 = 10, s2 = 20, d1 = 99, d2 = 88;

  graphInit(&src, 1);
  graphAddVertex(&src, &s1, NULL, NULL);
  graphAddVertex(&src, &s2, NULL, NULL);

  graphInit(&dest, 0);
  graphAddVertex(&dest, &d1, NULL, NULL);
  graphAddVertex(&dest, &d2, NULL, NULL);

  graphCopy(&dest, &src);

  TEST_ASSERT_EQUAL_PTR(&d1, dest.vertices[0].data);
  TEST_ASSERT_EQUAL_PTR(&d2, dest.vertices[1].data);

  graphRelease(&src);
  graphRelease(&dest);
}

void test_graphClone_basic(void) {
  Graph src, dest;
  int d1 = 5, d2 = 10, d3 = 15;

  graphInit(&src, 0);
  graphAddVertex(&src, &d1, NULL, NULL);
  graphAddVertex(&src, &d2, NULL, NULL);
  graphAddVertex(&src, &d3, NULL, NULL);
  graphAddEdge(&src, 0, 1);
  graphAddEdge(&src, 1, 2);

  memset(&dest, 0, sizeof(Graph));
  TEST_ASSERT_EQUAL_INT(0, graphClone(&dest, &src));

  TEST_ASSERT_EQUAL_INT(0, dest.directed);
  TEST_ASSERT_EQUAL_INT(3, dest.count);
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 1, 2));

  graphRelease(&src);
  graphRelease(&dest);
}

void test_graphCopyReversed_directed(void) {
  Graph src, dest;
  int d1 = 1, d2 = 2, d3 = 3;

  graphInit(&src, 1);
  graphAddVertex(&src, &d1, NULL, NULL);
  graphAddVertex(&src, &d2, NULL, NULL);
  graphAddVertex(&src, &d3, NULL, NULL);
  graphAddEdge(&src, 0, 1);
  graphAddEdge(&src, 1, 2);
  graphAddEdge(&src, 2, 0);

  graphInit(&dest, 1);
  TEST_ASSERT_EQUAL_INT(0, graphCopyReversed(&dest, &src));

  TEST_ASSERT_EQUAL_INT(0, graphEdgeExists(&dest, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 1, 0));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 2, 1));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 0, 2));

  graphRelease(&src);
  graphRelease(&dest);
}

void test_graphCopyReversed_undirected_same(void) {
  Graph src, dest;
  int d1 = 1, d2 = 2;

  graphInit(&src, 0);
  graphAddVertex(&src, &d1, NULL, NULL);
  graphAddVertex(&src, &d2, NULL, NULL);
  graphAddEdge(&src, 0, 1);

  graphInit(&dest, 0);
  graphCopyReversed(&dest, &src);

  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&dest, 1, 0));

  graphRelease(&src);
  graphRelease(&dest);
}

// ============================================================================
// COMPLEX GRAPH STRUCTURES
// ============================================================================

void test_linear_chain_directed(void) {
  Graph g;
  int data = 0;

  graphInit(&g, 1);
  for (int i = 0; i < 5; i++) {
    graphAddVertex(&g, &data, NULL, NULL);
  }

  for (int i = 0; i < 4; i++) {
    graphAddEdge(&g, i, i + 1);
  }

  TEST_ASSERT_EQUAL_INT(5, g.count);
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&g, 3, 4));
  TEST_ASSERT_EQUAL_INT(0, graphEdgeExists(&g, 4, 0));

  graphRelease(&g);
}

void test_cycle_directed(void) {
  Graph g;
  int data = 0;

  graphInit(&g, 1);
  for (int i = 0; i < 4; i++) {
    graphAddVertex(&g, &data, NULL, NULL);
  }

  graphAddEdge(&g, 0, 1);
  graphAddEdge(&g, 1, 2);
  graphAddEdge(&g, 2, 3);
  graphAddEdge(&g, 3, 0);

  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, graphEdgeExists(&g, 3, 0));

  graphRelease(&g);
}

void test_complete_graph_undirected(void) {
  Graph g;
  int data = 0;

  graphInit(&g, 0);
  for (int i = 0; i < 4; i++) {
    graphAddVertex(&g, &data, NULL, NULL);
  }

  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 4; j++) {
      graphAddEdge(&g, i, j);
    }
  }

  for (int i = 0; i < 4; i++) {
    TEST_ASSERT_EQUAL_INT(3, g.vertices[i].neighbors.count);
  }

  graphRelease(&g);
}

void test_star_graph_undirected(void) {
  Graph g;
  int data = 0;

  graphInit(&g, 0);
  for (int i = 0; i < 5; i++) {
    graphAddVertex(&g, &data, NULL, NULL);
  }

  for (int i = 1; i < 5; i++) {
    graphAddEdge(&g, 0, i);
  }

  TEST_ASSERT_EQUAL_INT(4, g.vertices[0].neighbors.count);
  for (int i = 1; i < 5; i++) {
    TEST_ASSERT_EQUAL_INT(1, g.vertices[i].neighbors.count);
  }

  graphRelease(&g);
}

// ============================================================================
// MEMORY & CLEAR
// ============================================================================

void test_graphClear(void) {
  Graph g;
  int data = 5;

  graphInit(&g, 1);
  graphAddVertex(&g, &data, NULL, NULL);
  graphAddVertex(&g, &data, NULL, NULL);
  graphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(3, g.count);
  graphClear(&g);
  TEST_ASSERT_EQUAL_INT(0, g.count);

  graphRelease(&g);
}

void test_clear_then_reuse(void) {
  Graph g;
  int data = 7;

  graphInit(&g, 1);
  graphAddVertex(&g, &data, NULL, NULL);
  graphAddVertex(&g, &data, NULL, NULL);
  graphClear(&g);

  graphAddVertex(&g, &data, NULL, NULL);
  TEST_ASSERT_EQUAL_INT(1, g.count);

  graphRelease(&g);
}

// ============================================================================
// ERROR CONDITIONS
// ============================================================================

void test_graph_null_operations(void) {
  Graph g;
  int data = 1;

  graphInit(&g, 1);
  graphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_NULL(graphGetVertexData(&g, 100));
  TEST_ASSERT_NULL(graphGetVertexNeighbors(&g, 100));
  TEST_ASSERT_EQUAL_INT(-1, graphRemoveEdge(&g, 100, 0));

  graphRelease(&g);
}

void test_graph_empty_operations(void) {
  Graph g;

  graphInit(&g, 1);
  TEST_ASSERT_EQUAL_INT(0, g.count);
  TEST_ASSERT_NULL(graphGetVertexData(&g, 0));

  graphRelease(&g);
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_vertexInit_valid);
  RUN_TEST(test_vertexInit_null_vertex);
  RUN_TEST(test_vertexInit_null_data_allowed);
  RUN_TEST(test_vertexInitAtCapacity);
  RUN_TEST(test_vertexCopy_basic);
  RUN_TEST(test_vertexCopy_null_pointers);
  RUN_TEST(test_vertexClone_basic);

  RUN_TEST(test_graphInit_directed);
  RUN_TEST(test_graphInit_undirected);
  RUN_TEST(test_graphInitAtCapacity);
  RUN_TEST(test_graphAddVertex_single);
  RUN_TEST(test_graphAddVertex_multiple);
  RUN_TEST(test_graphAddVertex_capacity_growth);

  RUN_TEST(test_graphAddEdge_directed);
  RUN_TEST(test_graphAddEdge_undirected);
  RUN_TEST(test_graphAddEdge_out_of_bounds);
  RUN_TEST(test_graphAddEdge_self_loop);
  RUN_TEST(test_graphEdgeExists_true);
  RUN_TEST(test_graphEdgeExists_out_of_bounds);
  RUN_TEST(test_graphRemoveEdge_directed);
  RUN_TEST(test_graphRemoveEdge_undirected);
  RUN_TEST(test_graphRemoveEdge_nonexistent);

  RUN_TEST(test_graphGetVertexData);
}
