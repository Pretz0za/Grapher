#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizPlanarEmbedding.h"
#include "renderer/embeddings/gvizSchnyderWood.h"
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

/* Helper: follow parent[tree] pointers from v, return root reached.
 * Returns GVIZ_SW_NONE if a cycle is detected or too many hops. */
static size_t swFollowToRoot(const gvizSchnyderWood *sw, int tree, size_t v)
{
  size_t hops = sw->n + 1;
  while (hops-- && sw->parent[tree][v] != GVIZ_SW_NONE)
    v = sw->parent[tree][v];
  return (hops == (size_t)-1) ? GVIZ_SW_NONE : v;
}

/* Verify that the Schnyder wood sw is structurally valid for graph g:
 *   1. Each tree root[i] has GVIZ_SW_NONE as parent[i][root[i]].
 *   2. Every non-{s0,s1,s2} vertex has a valid (non-NONE) parent in every tree.
 *   3. Every assigned parent is an actual neighbour in g.
 *   4. Following any chain in tree i eventually reaches root[i].
 */
static void verifySchnyderWood(const gvizSchnyderWood *sw, gvizGraph *g)
{
  for (int c = 0; c < 3; c++)
    TEST_ASSERT_EQUAL_UINT64(GVIZ_SW_NONE, sw->parent[c][sw->root[c]]);

  for (size_t v = 0; v < sw->n; v++) {
    int is_outer = (v == sw->root[0] || v == sw->root[1] || v == sw->root[2]);
    for (int c = 0; c < 3; c++) {
      size_t p = sw->parent[c][v];
      if (p == GVIZ_SW_NONE) continue;

      /* Must be an actual edge in the graph. */
      TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(g, v, p));

      /* Chain must terminate at the correct root. */
      size_t reached = swFollowToRoot(sw, c, v);
      TEST_ASSERT_EQUAL_UINT64(sw->root[c], reached);

      if (!is_outer) {
        /* Interior vertices must have a parent in every tree. */
      }
    }
    /* Every interior vertex must have exactly one parent per tree. */
    if (!is_outer) {
      for (int c = 0; c < 3; c++)
        TEST_ASSERT_NOT_EQUAL(GVIZ_SW_NONE, sw->parent[c][v]);
    }
  }
}

/* --- Schnyder wood on K4 (already a maximal planar graph) --- */
void test_schnyderWood_K4(void)
{
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 0, 3);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 1, 3);
  gvizGraphAddEdge(&g, 2, 3);

  gvizPlanarEmbeddingState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbeddingInit(&state, &g));

  gvizSchnyderWood sw;
  TEST_ASSERT_EQUAL(0, gvizSchnyderWoodInit(&sw, &g));
  TEST_ASSERT_EQUAL_UINT64(4, sw.n);

  verifySchnyderWood(&sw, &g);

  gvizSchnyderWoodRelease(&sw);
  gvizPlanarEmbeddingRelease(&state);
  gvizGraphRelease(&g);
}

/* --- Schnyder wood on a hexagon after triangulation --- */
void test_schnyderWood_hexagon(void)
{
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 4);
  gvizGraphAddEdge(&g, 4, 5);
  gvizGraphAddEdge(&g, 5, 0);
  gvizGraphAddEdge(&g, 5, 3);

  gvizPlanarEmbeddingState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbeddingInit(&state, &g));

  gvizFaceIteratorContext faces;
  gvizFaceIteratorInit(&state, &faces);
  gvizPlanarEmbeddingFaces(&state, &faces);
  gvizPlanarEmbeddingTriangulate(&state, &faces);
  gvizFaceIteratorRelease(&faces);

  gvizSchnyderWood sw;
  TEST_ASSERT_EQUAL(0, gvizSchnyderWoodInit(&sw, &g));
  TEST_ASSERT_EQUAL_UINT64(6, sw.n);

  verifySchnyderWood(&sw, &g);

  gvizSchnyderWoodRelease(&sw);
  gvizPlanarEmbeddingRelease(&state);
  gvizGraphRelease(&g);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_planar);
  RUN_TEST(test_nonPlanar);
  RUN_TEST(test_triangulation);
  RUN_TEST(test_schnyderWood_K4);
  RUN_TEST(test_schnyderWood_hexagon);

  UNITY_END();
  return 0;
}
