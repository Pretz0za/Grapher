#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizPlanarEmbedding.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"
#include "utils/serializers.h"
#include <stdio.h>

void setUp(void) {}
void tearDown() {}

int indexOf(size_t *arr, size_t target, size_t count) {
  for (size_t i = 0; i < count; i++) {
    if (arr[i] == target)
      return i;
  }
  return -1;
}

int getNextVertex(size_t *order, size_t currVertex, size_t count) {
  int res = indexOf(order, currVertex, count);
  if (res < 0)
    return -1;
  size_t idx = ((size_t)res + 1) % count;
  return order[idx];
}

void test_planar() {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  size_t correctOrder[3] = {3, 2, 1};

  // add edges in the wrong roational order
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 3);

  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);

  gvizPlanarEmbeddingState state;
  int res = gvizPlanarEmbeddingInit(&state, &g);

  TEST_ASSERT_EQUAL(0, res);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, 0);

  size_t prev = *(size_t *)gvizArrayAtIndex(neighbors, 0);
  for (size_t i = 1; i < 3; i++) {

    size_t curr = *(size_t *)gvizArrayAtIndex(neighbors, i);

    int res = getNextVertex(correctOrder, prev, 3);
    TEST_ASSERT_NOT_EQUAL(-1, res);
    size_t expected = (size_t)res;

    TEST_ASSERT_EQUAL_UINT64(expected, curr);
    prev = curr;
  }

  gvizPlanarEmbeddingRelease(&state);
  gvizGraphRelease(&g);
}

// nvm fix graph
void test_nonPlanar() {

  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  size_t correctOrder[3] = {3, 2, 1};

  // construct K3,3

  gvizGraphAddEdge(&g, 0, 3);
  gvizGraphAddEdge(&g, 0, 4);
  gvizGraphAddEdge(&g, 0, 5);

  gvizGraphAddEdge(&g, 1, 3);
  gvizGraphAddEdge(&g, 1, 4);
  gvizGraphAddEdge(&g, 1, 5);

  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 2, 4);
  gvizGraphAddEdge(&g, 2, 5);

  gvizPlanarEmbeddingState state;
  int res = gvizPlanarEmbeddingInit(&state, &g);

  TEST_ASSERT_EQUAL(-2, res);

  gvizGraphRelease(&g);
}

void test_triangulation() {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  // basic hexagon
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 4);
  gvizGraphAddEdge(&g, 4, 5);
  gvizGraphAddEdge(&g, 5, 0);

  gvizGraphAddEdge(&g, 5, 3);

  gvizPlanarEmbeddingState state;
  int res = gvizPlanarEmbeddingInit(&state, &g);

  TEST_ASSERT_EQUAL(0, res);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, 0);

  gvizFaceIteratorContext faces;
  gvizFaceIteratorInit(&state, &faces);

  gvizPlanarEmbeddingFaces(&state, &faces);

  gvizPlanarEmbeddingTriangulate(&state, &faces);

  for (size_t i = 0; i < faces.faces.count; i++) {
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&faces.faces, i);
    TEST_ASSERT_EQUAL(3, face->count);
  }

  TEST_ASSERT_EQUAL(2, g.vertices.count - faces.dCount / 2 + faces.faces.count);

  for (size_t i = 0; i < 6; i++)
    gvizArrayPrint(gvizGraphGetVertexNeighbors(&g, i), stdout,
                   gvizSerializeUINT64, 8);

  gvizFaceIteratorRelease(&faces);
  gvizPlanarEmbeddingRelease(&state);
  gvizGraphRelease(&g);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_planar);
  RUN_TEST(test_nonPlanar);
  RUN_TEST(test_triangulation);

  UNITY_END();
  return 0;
}
