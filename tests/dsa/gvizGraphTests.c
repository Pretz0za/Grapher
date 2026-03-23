#include "unity/unity.h"
#include "unity/unity_internals.h"
#undef GVIZ_ARRAY_ALLOC
#undef GVIZ_ARRAY_REALLOC
#undef GVIZ_ARRAY_DEALLOC
#undef GVIZ_GRAPH_ALLOC
#undef GVIZ_GRAPH_REALLOC
#undef GVIZ_GRAPH_DEALLOC
#include "dsa/gvizGraph.h"
#include <stdlib.h>
#include <string.h>

/**
 * Test fixture setup and teardown functions for graph tests.
 */

void setUp(void) {
  // Called before each test
}

void tearDown(void) {
  // Called after each test
}

// ============================================================================
// VERTEX INITIALIZATION TESTS
// ============================================================================

void test_vertexInit_WithValidData(void) {
  gvizVertex v;
  int data = 42;

  int result = gvizVertexInit(&v, &data);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_PTR(&data, v.data);
  TEST_ASSERT_NOT_NULL(v.neighbors.arr);
  TEST_ASSERT_EQUAL_UINT64(0, v.neighbors.count);

  gvizVertexRelease(&v);
}

void test_vertexInit_WithNullPointer(void) {
  int result = gvizVertexInit(NULL, NULL);

  TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_vertexInit_WithNullData(void) {
  gvizVertex v;
  int result = gvizVertexInit(&v, NULL);

  // Initializing with NULL data should succeed, as the vertex can hold NULL
  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_NULL(v.data);

  gvizVertexRelease(&v);
}

void test_vertexInitAtCapacity_WithSpecificCapacity(void) {
  gvizVertex v;
  int data = 100;
  size_t initialCapacity = 50;

  int result = gvizVertexInitAtCapacity(&v, &data, initialCapacity);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_PTR(&data, v.data);
  TEST_ASSERT_EQUAL_UINT64(initialCapacity, v.neighbors.capacity);

  gvizVertexRelease(&v);
}

// ============================================================================
// VERTEX COPY AND CLONE TESTS
// ============================================================================

void test_vertexCopy_Basic(void) {
  gvizVertex src, dest;
  int data = 42;

  gvizVertexInit(&src, &data);
  gvizVertexInit(&dest, NULL);

  // Add some neighbors to source
  int neighbor1 = 0, neighbor2 = 1;
  gvizArrayPush(&src.neighbors, &neighbor1);
  gvizArrayPush(&src.neighbors, &neighbor2);

  int result = gvizVertexCopy(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(src.neighbors.count, dest.neighbors.count);

  gvizVertexRelease(&src);
  gvizVertexRelease(&dest);
}

void test_vertexClone_CreatesIndependentCopy(void) {
  gvizVertex src, dest;
  int data = 42;

  gvizVertexInit(&src, &data);
  int neighbor = 5;
  gvizArrayPush(&src.neighbors, &neighbor);

  int result = gvizVertexClone(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(1, dest.neighbors.count);

  // Verify neighbor is copied
  int *destNeighbor = (int *)gvizArrayAtIndex(&dest.neighbors, 0);
  TEST_ASSERT_EQUAL_INT(5, *destNeighbor);

  gvizVertexRelease(&src);
  gvizVertexRelease(&dest);
}

// ============================================================================
// GRAPH INITIALIZATION TESTS
// ============================================================================

void test_graphInit_DirectedGraph(void) {
  gvizGraph g;
  int result = gvizGraphInit(&g, 1); // 1 = directed

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(1, g.directed);
  TEST_ASSERT_NOT_NULL(g.vertices.arr);
  TEST_ASSERT_EQUAL_UINT64(0, g.vertices.count);

  gvizGraphRelease(&g);
}

void test_graphInit_UndirectedGraph(void) {
  gvizGraph g;
  int result = gvizGraphInit(&g, 0); // 0 = undirected

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(0, g.directed);

  gvizGraphRelease(&g);
}

void test_graphInitAtCapacity_WithSpecificCapacity(void) {
  gvizGraph g;
  size_t initialCapacity = 100;
  int result = gvizGraphInitAtCapacity(&g, 1, initialCapacity);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(initialCapacity, g.vertices.capacity);

  gvizGraphRelease(&g);
}

void test_graphInit_WithNullPointer(void) {
  int result = gvizGraphInit(NULL, 1);

  TEST_ASSERT_EQUAL_INT(-1, result);
}

// ============================================================================
// ADD VERTEX TESTS
// ============================================================================

void test_graphAddVertex_SingleVertex(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data = 42;
  int result = gvizGraphAddVertex(&g, &data, NULL, NULL);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(1, g.vertices.count);

  gvizGraphRelease(&g);
}

void test_graphAddVertex_MultipleVertices(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  for (int i = 0; i < 5; i++) {
    int data = i * 10;
    int result = gvizGraphAddVertex(&g, &data, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  TEST_ASSERT_EQUAL_UINT64(5, g.vertices.count);

  gvizGraphRelease(&g);
}

void test_graphAddVertex_RetrieveVertexData(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data1 = 100;
  int data2 = 200;

  gvizGraphAddVertex(&g, &data1, NULL, NULL);
  gvizGraphAddVertex(&g, &data2, NULL, NULL);

  int *retrieved1 = (int *)gvizGraphGetVertexData(&g, 0);
  int *retrieved2 = (int *)gvizGraphGetVertexData(&g, 1);

  TEST_ASSERT_EQUAL_INT(100, *retrieved1);
  TEST_ASSERT_EQUAL_INT(200, *retrieved2);

  gvizGraphRelease(&g);
}

void test_graphAddVertex_OutOfBounds(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data = 42;
  gvizGraphAddVertex(&g, &data, NULL, NULL);

  // Accessing vertex at index 1 should return NULL
  void *result = gvizGraphGetVertexData(&g, 1);
  TEST_ASSERT_NULL(result);

  gvizGraphRelease(&g);
}

// ============================================================================
// ADD EDGE TESTS
// ============================================================================

void test_graphAddEdge_SimpleEdge(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1); // directed

  int data1 = 10, data2 = 20;
  gvizGraphAddVertex(&g, &data1, NULL, NULL);
  gvizGraphAddVertex(&g, &data2, NULL, NULL);

  // Add edge from vertex 0 to vertex 1
  int result = gvizGraphAddEdge(&g, 0, 1);

  TEST_ASSERT_EQUAL_INT(0, result);

  // Check that the edge exists
  int edgeExists = gvizGraphEdgeExists(&g, 0, 1);
  TEST_ASSERT_EQUAL_INT(1, edgeExists);

  gvizGraphRelease(&g);
}

void test_graphAddEdge_UndirectedGraphBiDirectional(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0); // undirected

  int data1 = 10, data2 = 20;
  gvizGraphAddVertex(&g, &data1, NULL, NULL);
  gvizGraphAddVertex(&g, &data2, NULL, NULL);

  // Add edge between vertex 0 and vertex 1
  int result = gvizGraphAddEdge(&g, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, result);

  // Both directions should exist in undirected graph
  int forward = gvizGraphEdgeExists(&g, 0, 1);
  int backward = gvizGraphEdgeExists(&g, 1, 0);

  TEST_ASSERT_EQUAL_INT(1, forward);
  TEST_ASSERT_EQUAL_INT(1, backward);

  gvizGraphRelease(&g);
}

void test_graphAddEdge_InvalidIndices(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data = 10;
  gvizGraphAddVertex(&g, &data, NULL, NULL);

  // Try to add edge with out-of-bounds indices
  int result = gvizGraphAddEdge(&g, 0, 5);

  TEST_ASSERT_EQUAL_INT(-1, result);

  gvizGraphRelease(&g);
}

void test_graphAddEdge_SelfLoop(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data = 10;
  gvizGraphAddVertex(&g, &data, NULL, NULL);

  // Add edge from vertex 0 to itself
  int result = gvizGraphAddEdge(&g, 0, 0);
  TEST_ASSERT_EQUAL_INT(0, result);

  int edgeExists = gvizGraphEdgeExists(&g, 0, 0);
  TEST_ASSERT_EQUAL_INT(1, edgeExists);

  gvizGraphRelease(&g);
}

void test_graphAddEdge_MultipleEdges(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  // Create a small graph with 4 vertices
  for (int i = 0; i < 4; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  // Add multiple edges
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 1, 3);
  gvizGraphAddEdge(&g, 2, 3);

  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 2));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 1, 3));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 2, 3));

  // Non-existent edges should return 0
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 1, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 3, 0));

  gvizGraphRelease(&g);
}

// ============================================================================
// REMOVE EDGE TESTS
// ============================================================================

void test_graphRemoveEdge_Basic(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data1 = 10, data2 = 20;
  gvizGraphAddVertex(&g, &data1, NULL, NULL);
  gvizGraphAddVertex(&g, &data2, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));

  // Remove the edge
  int result = gvizGraphRemoveEdge(&g, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, result);

  // Edge should no longer exist
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 0, 1));

  gvizGraphRelease(&g);
}

void test_graphRemoveEdge_NonExistentEdge(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  // Try to remove non-existent edge
  int result = gvizGraphRemoveEdge(&g, 0, 1);

  TEST_ASSERT_EQUAL_INT(-1, result);

  gvizGraphRelease(&g);
}

void test_graphRemoveEdge_InvalidIndices(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int data = 10;
  gvizGraphAddVertex(&g, &data, NULL, NULL);

  // Try to remove edge with out-of-bounds indices
  int result = gvizGraphRemoveEdge(&g, 0, 5);

  TEST_ASSERT_EQUAL_INT(-1, result);

  gvizGraphRelease(&g);
}

void test_graphRemoveEdge_UndirectedRemovesBoth(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0); // undirected

  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);

  // Remove edge should remove both directions
  int result = gvizGraphRemoveEdge(&g, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, result);

  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 1, 0));

  gvizGraphRelease(&g);
}

// ============================================================================
// GET NEIGHBORS TESTS
// ============================================================================

void test_graphGetVertexNeighbors_WithEdges(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  for (int i = 0; i < 3; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 2);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, 0);

  TEST_ASSERT_NOT_NULL(neighbors);
  TEST_ASSERT_EQUAL_UINT64(2, neighbors->count);

  gvizGraphRelease(&g);
}

void test_graphGetVertexNeighbors_NoNeighbors(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, 0);

  TEST_ASSERT_NOT_NULL(neighbors);
  TEST_ASSERT_EQUAL_UINT64(0, neighbors->count);

  gvizGraphRelease(&g);
}

void test_graphGetVertexNeighbors_InvalidIndex(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, 5);

  TEST_ASSERT_NULL(neighbors);

  gvizGraphRelease(&g);
}

// ============================================================================
// COPY AND CLONE TESTS
// ============================================================================

void test_graphCopy_Basic(void) {
  gvizGraph src, dest;
  gvizGraphInit(&src, 1);
  gvizGraphInit(&dest, 1);

  // Create a simple graph
  for (int i = 0; i < 3; i++) {
    gvizGraphAddVertex(&src, NULL, NULL, NULL);
  }
  gvizGraphAddEdge(&src, 0, 1);
  gvizGraphAddEdge(&src, 1, 2);

  int result = gvizGraphCopy(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(3, dest.vertices.count);
  TEST_ASSERT_EQUAL_INT(1, dest.directed);

  // Verify edges are copied
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&dest, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&dest, 1, 2));

  gvizGraphRelease(&src);
  gvizGraphRelease(&dest);
}

void test_graphClone_Independent(void) {
  gvizGraph src, dest;
  gvizGraphInit(&src, 1);

  for (int i = 0; i < 2; i++) {
    gvizGraphAddVertex(&src, NULL, NULL, NULL);
  }
  gvizGraphAddEdge(&src, 0, 1);

  int result = gvizGraphClone(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(2, dest.vertices.count);

  gvizGraphRelease(&src);
  gvizGraphRelease(&dest);
}

void test_graphCopyReversed_DirectedGraph(void) {
  gvizGraph src, dest;
  gvizGraphInit(&src, 1); // directed
  gvizGraphInit(&dest, 0);

  for (int i = 0; i < 3; i++) {
    gvizGraphAddVertex(&src, NULL, NULL, NULL);
  }

  // Add edges: 0->1, 1->2
  gvizGraphAddEdge(&src, 0, 1);
  gvizGraphAddEdge(&src, 1, 2);

  int result = gvizGraphCopyReversed(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);

  // In reversed graph: 1->0, 2->1
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&dest, 1, 0));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&dest, 2, 1));

  // Original edges should NOT exist
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&dest, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&dest, 1, 2));

  gvizGraphRelease(&src);
  gvizGraphRelease(&dest);
}

// ============================================================================
// GRAPH CLEAR TESTS
// ============================================================================

void test_graphClear_RemovesAllVertices(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  for (int i = 0; i < 5; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  TEST_ASSERT_EQUAL_UINT64(5, g.vertices.count);

  gvizGraphClear(&g);

  TEST_ASSERT_EQUAL_UINT64(0, g.vertices.count);

  gvizGraphRelease(&g);
}

// ============================================================================
// STRESS TESTS - LARGE GRAPHS
// ============================================================================

void test_graphAddVertex_LargeNumberOfVertices(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int largeSize = 1000;

  for (int i = 0; i < largeSize; i++) {
    int result = gvizGraphAddVertex(&g, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  TEST_ASSERT_EQUAL_UINT64(largeSize, g.vertices.count);

  gvizGraphRelease(&g);
}

void test_graph_CompleteGraph(void) {
  // Create a complete graph where every vertex connects to every other
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int vertexCount = 20;

  for (int i = 0; i < vertexCount; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  // Add edges to create complete graph
  for (int i = 0; i < vertexCount; i++) {
    for (int j = 0; j < vertexCount; j++) {
      if (i != j) {
        int result = gvizGraphAddEdge(&g, i, j);
        TEST_ASSERT_EQUAL_INT(0, result);
      }
    }
  }

  TEST_ASSERT_EQUAL_UINT64(vertexCount, g.vertices.count);

  // Verify a few edges
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 5, 10));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, vertexCount - 1, 0));

  gvizGraphRelease(&g);
}

void test_graph_LinearChain(void) {
  // Create a linear chain: 0->1->2->3->...->99
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int chainLength = 100;

  for (int i = 0; i < chainLength; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  for (int i = 0; i < chainLength - 1; i++) {
    int result = gvizGraphAddEdge(&g, i, i + 1);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  // Verify chain
  for (int i = 0; i < chainLength - 1; i++) {
    TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, i, i + 1));
  }

  gvizGraphRelease(&g);
}

void test_graph_StarTopology(void) {
  // Create a star graph where one central vertex connects to all others
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int vertexCount = 50;
  int centerVertex = 0;

  for (int i = 0; i < vertexCount; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  // Connect center to all others
  for (int i = 1; i < vertexCount; i++) {
    int result = gvizGraphAddEdge(&g, centerVertex, i);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, centerVertex);
  TEST_ASSERT_EQUAL_UINT64(vertexCount - 1, neighbors->count);

  gvizGraphRelease(&g);
}

void test_graph_LargeEdgeSet(void) {
  // Create a graph with many edges
  gvizGraph g;
  gvizGraphInit(&g, 1);

  int vertexCount = 100;

  for (int i = 0; i < vertexCount; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  // Add many edges (e.g., every even vertex to next odd vertex)
  for (int i = 0; i < vertexCount - 1; i += 2) {
    int result = gvizGraphAddEdge(&g, i, i + 1);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  gvizGraphRelease(&g);
}

// ============================================================================
// DIRECTED VS UNDIRECTED TESTS
// ============================================================================

void test_graph_DirectedAsymmetry(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1); // directed

  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);

  // In directed graph, only forward edge exists
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 1, 0));

  gvizGraphRelease(&g);
}

void test_graph_UndirectedSymmetry(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0); // undirected

  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);

  // In undirected graph, both directions exist
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 1, 0));

  gvizGraphRelease(&g);
}

// ============================================================================
// MIXED OPERATION TESTS
// ============================================================================

void test_graph_BuildAndModify(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  // Build initial graph
  for (int i = 0; i < 5; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 4);

  // Verify initial state
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 3, 4));

  // Remove middle edge
  gvizGraphRemoveEdge(&g, 2, 3);
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 2, 3));

  // Add new edges
  gvizGraphAddEdge(&g, 0, 4);
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 4));

  gvizGraphRelease(&g);
}

void test_graph_EdgeExistsInvalidIndices(void) {
  gvizGraph g;
  gvizGraphInit(&g, 1);

  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  // Test edge existence with invalid indices
  int result = gvizGraphEdgeExists(&g, 0, 5);
  TEST_ASSERT_EQUAL_INT(-1, result);

  result = gvizGraphEdgeExists(&g, 10, 0);
  TEST_ASSERT_EQUAL_INT(-1, result);

  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_vertexInit_WithValidData);
  RUN_TEST(test_vertexInit_WithNullPointer);
  RUN_TEST(test_vertexInit_WithNullData);
  RUN_TEST(test_vertexInitAtCapacity_WithSpecificCapacity);
  RUN_TEST(test_vertexCopy_Basic);
  RUN_TEST(test_vertexClone_CreatesIndependentCopy);
  RUN_TEST(test_graphInit_DirectedGraph);
  RUN_TEST(test_graphInit_UndirectedGraph);
  RUN_TEST(test_graphInitAtCapacity_WithSpecificCapacity);
  RUN_TEST(test_graphInit_WithNullPointer);
  RUN_TEST(test_graphAddVertex_SingleVertex);
  RUN_TEST(test_graphAddVertex_MultipleVertices);
  RUN_TEST(test_graphAddVertex_RetrieveVertexData);
  RUN_TEST(test_graphAddVertex_OutOfBounds);
  RUN_TEST(test_graphAddEdge_SimpleEdge);
  RUN_TEST(test_graphAddEdge_UndirectedGraphBiDirectional);
  RUN_TEST(test_graphAddEdge_InvalidIndices);
  RUN_TEST(test_graphAddEdge_SelfLoop);
  RUN_TEST(test_graphAddEdge_MultipleEdges);
  RUN_TEST(test_graphRemoveEdge_Basic);
  RUN_TEST(test_graphRemoveEdge_NonExistentEdge);
  RUN_TEST(test_graphRemoveEdge_InvalidIndices);
  RUN_TEST(test_graphRemoveEdge_UndirectedRemovesBoth);
  RUN_TEST(test_graphGetVertexNeighbors_WithEdges);
  RUN_TEST(test_graphGetVertexNeighbors_NoNeighbors);
  RUN_TEST(test_graphGetVertexNeighbors_InvalidIndex);
  RUN_TEST(test_graphCopy_Basic);
  RUN_TEST(test_graphClone_Independent);
  RUN_TEST(test_graphCopyReversed_DirectedGraph);
  RUN_TEST(test_graphClear_RemovesAllVertices);
  RUN_TEST(test_graphAddVertex_LargeNumberOfVertices);
  RUN_TEST(test_graph_CompleteGraph);
  RUN_TEST(test_graph_LinearChain);
  RUN_TEST(test_graph_StarTopology);
  RUN_TEST(test_graph_LargeEdgeSet);
  RUN_TEST(test_graph_DirectedAsymmetry);
  RUN_TEST(test_graph_UndirectedSymmetry);
  RUN_TEST(test_graph_BuildAndModify);
  RUN_TEST(test_graph_EdgeExistsInvalidIndices);

  int result = UNITY_END();

  printf("\n");

  return result;
}
