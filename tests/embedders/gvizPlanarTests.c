#include "ds/gvizArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizPlanarEmbedder.h"
#include "embedders/gvizSchnyderWood.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"
#include "utils/serializers.h"
#include <stdio.h>
#include <stdlib.h>

void setUp(void) {}
void tearDown() {}

static gvizSubgraph makeFullSubgraph(gvizGraph *g) {
  gvizGraphBuildLayout(g);
  return gvizSubgraphCreateFull(g);
}

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

  gvizPlanarEmbedderState state;
  int res = gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g));

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

  gvizPlanarEmbedderRelease(&state);
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

  gvizPlanarEmbedderState state;
  int res = gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g));

  TEST_ASSERT_EQUAL(-2, res);

  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

// Non-planar init must hand back a Kuratowski subdivision witness when the
// caller asks for one via gvizSubgraphApplyPlanarRotation.
void test_nonPlanar_kuratowskiWitness() {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 5; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  // K5
  for (size_t u = 0; u < 5; u++)
    for (size_t v = u + 1; v < 5; v++)
      gvizGraphAddEdge(&g, u, v);

  gvizGraphBuildLayout(&g);
  gvizSubgraph sg = gvizSubgraphCreateFull(&g);

  gvizGraph *kuratowski = NULL;
  int res = gvizSubgraphApplyPlanarRotation(&sg, &kuratowski);
  TEST_ASSERT_EQUAL(-2, res);
  TEST_ASSERT_NOT_NULL(kuratowski);

  // The witness must be a subgraph of K5 with some edges (a K5 subdivision).
  TEST_ASSERT_EQUAL_UINT64(5, gvizGraphSize(kuratowski));
  gvizGraphBuildLayout(kuratowski);
  TEST_ASSERT_TRUE(gvizGraphEdgeCount(kuratowski) > 0);

  gvizGraphRelease(kuratowski);
  GVIZ_DEALLOC(kuratowski);
  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_largestFaceBoundary_square() {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  // 4-cycle: both faces are the square itself.
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 0);

  gvizGraphBuildLayout(&g);
  gvizSubgraph sg = gvizSubgraphCreateFull(&g);
  TEST_ASSERT_EQUAL(0, gvizSubgraphApplyPlanarRotation(&sg, NULL));

  size_t boundary[8];
  size_t count = 0;
  TEST_ASSERT_EQUAL(
      0, gvizPlanarLargestFaceBoundary(&g, &sg, boundary, 8, &count));
  TEST_ASSERT_EQUAL_UINT64(4, count);
  // All four vertices appear exactly once.
  int seen[4] = {0, 0, 0, 0};
  for (size_t i = 0; i < count; i++) {
    TEST_ASSERT_TRUE(boundary[i] < 4);
    seen[boundary[i]]++;
  }
  for (int i = 0; i < 4; i++)
    TEST_ASSERT_EQUAL_INT(1, seen[i]);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_halfEdge_twin() {
  gvizPlanarHalfEdge e = {3, 7};
  gvizPlanarHalfEdge t = gvizPlanarHalfEdgeTwin(e);
  TEST_ASSERT_EQUAL_UINT64(7, t.u);
  TEST_ASSERT_EQUAL_UINT64(3, t.v);
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

  gvizPlanarEmbedderState state;
  int res = gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g));

  TEST_ASSERT_EQUAL(0, res);

  gvizArray *neighbors = gvizGraphGetVertexNeighbors(&g, 0);

  gvizFaceIteratorContext faces;
  gvizFaceIteratorInit(&state.embedding.subgraph, &faces);

  gvizPlanarEmbedderFaces(&state.embedding.subgraph, &faces);

  gvizPlanarEmbedderTriangulate(&state.embedding.subgraph, &faces);

  for (size_t i = 0; i < faces.faces.count; i++) {
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&faces.faces, i);
    TEST_ASSERT_EQUAL(3, face->count);
  }

  TEST_ASSERT_EQUAL(2, g.vertices.count - faces.dCount / 2 + faces.faces.count);

  for (size_t i = 0; i < 6; i++)
    gvizArrayPrint(gvizGraphGetVertexNeighbors(&g, i), stdout,
                   gvizSerializeUINT64, 8);

  gvizFaceIteratorRelease(&faces);
  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

/* Helper: follow parent[tree] pointers from v, return root reached.
 * Returns GVIZ_SW_NONE if a cycle is detected or too many hops. */
static size_t swFollowToRoot(const gvizSchnyderWood *sw, int tree, size_t v) {
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
static void verifySchnyderWood(const gvizSchnyderWood *sw, gvizGraph *g) {
  for (int c = 0; c < 3; c++)
    TEST_ASSERT_EQUAL_UINT64(GVIZ_SW_NONE, sw->parent[c][sw->root[c]]);

  for (size_t v = 0; v < sw->n; v++) {
    int is_outer = (v == sw->root[0] || v == sw->root[1] || v == sw->root[2]);
    for (int c = 0; c < 3; c++) {
      size_t p = sw->parent[c][v];
      if (p == GVIZ_SW_NONE)
        continue;

      /* Must be an actual edge in the graph. */
      TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(g, v, p));

      /* Chain must terminate at the correct root. */
      size_t reached = swFollowToRoot(sw, c, v);
      TEST_ASSERT_EQUAL_UINT64(sw->root[c], reached);
    }
    /* Every interior vertex must have exactly one parent per tree. */
    if (!is_outer) {
      for (int c = 0; c < 3; c++)
        TEST_ASSERT_NOT_EQUAL(GVIZ_SW_NONE, sw->parent[c][v]);
    }
  }
}

/* --- Schnyder wood on K4 (already a maximal planar graph) --- */
void test_schnyderWood_K4(void) {
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

  gvizPlanarEmbedderState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g)));

  gvizSchnyderWood sw;
  TEST_ASSERT_EQUAL(0, gvizSchnyderWoodInit(&sw, &g));
  TEST_ASSERT_EQUAL_UINT64(4, sw.n);

  verifySchnyderWood(&sw, &g);

  gvizSchnyderWoodRelease(&sw);
  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

/* --- Schnyder wood on a hexagon after triangulation --- */
void test_schnyderWood_hexagon(void) {
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

  gvizPlanarEmbedderState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g)));

  gvizFaceIteratorContext faces;
  gvizFaceIteratorInit(&state.embedding.subgraph, &faces);
  gvizPlanarEmbedderFaces(&state.embedding.subgraph, &faces);
  gvizPlanarEmbedderTriangulate(&state.embedding.subgraph, &faces);
  gvizFaceIteratorRelease(&faces);

  gvizSchnyderWood sw;
  TEST_ASSERT_EQUAL(0, gvizSchnyderWoodInit(&sw, &g));
  TEST_ASSERT_EQUAL_UINT64(6, sw.n);

  verifySchnyderWood(&sw, &g);

  printf("Schnyder Wood Created: \n");
  printf("\t root[0]: %zu\n", sw.root[0]);
  printf("\t root[1]: %zu\n", sw.root[1]);
  printf("\t root[2]: %zu\n", sw.root[2]);

  printf("Parents: \n");
  for (size_t r = 0; r < 3; r++) {
    printf("Tree %zu parents arr:\n", r);
    for (size_t i = 0; i < sw.n; i++) {
      if (i == sw.root[0] || i == sw.root[1] || i == sw.root[2])
        continue;

      printf("\t%zu's parent is %zu. %zu <--- %zu\n", i, sw.parent[r][i], i,
             sw.parent[r][i]);
    }
  }

  gvizSchnyderWoodEmbed(&sw, (gvizEmbeddedGraph *)(&state));

  printf("Schnyder Wood Drawn: \n");
  for (size_t i = 0; i < sw.n; i++) {
    double *pos =
        gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)(&state), i);
    printf("\t vertex %zu: (%f, %f)\n", i, pos[0], pos[1]);
  }

  gvizSchnyderWoodRelease(&sw);
  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

static void collectSubgraphNeighbors(const gvizSubgraph *sg, size_t u,
                                     size_t *out, size_t *count) {
  *count = 0;
  size_t v;
  gvizSubgraphNeighborIterator nit = gvizSubgraphNeighborIteratorCreate(sg, u);
  while (gvizSubgraphNeighborIterate(&nit, &v))
    out[(*count)++] = v;
}

void test_subgraph_neighbor_ccw_order(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 2);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 0, 3);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);

  gvizPlanarEmbedderState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g)));

  size_t correctOrder[3] = {3, 2, 1};
  size_t nbrs[8];
  size_t ncount = 0;
  collectSubgraphNeighbors(&state.embedding.subgraph, 0, nbrs, &ncount);
  TEST_ASSERT_EQUAL(3, ncount);

  size_t prev = nbrs[0];
  for (size_t i = 1; i < ncount; i++) {
    int next = getNextVertex(correctOrder, prev, 3);
    TEST_ASSERT_NOT_EQUAL(-1, next);
    TEST_ASSERT_EQUAL_UINT64((size_t)next, nbrs[i]);
    prev = nbrs[i];
  }

  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

void test_face_walk_triangle(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 0);

  gvizPlanarEmbedderState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbedderInit(&state, makeFullSubgraph(&g)));

  gvizPlanarFaceWalk walk;
  TEST_ASSERT_EQUAL(
      0, gvizPlanarFaceWalkBegin(&state.embedding.subgraph,
                                 (gvizPlanarHalfEdge){0, 1}, &walk));

  size_t seen[4];
  size_t seenCount = 0;
  int step;
  do {
    step = gvizPlanarFaceWalkStep(&walk, &seen[seenCount]);
    if (step >= 0)
      seenCount++;
  } while (step == 1);

  TEST_ASSERT_EQUAL(0, step);
  TEST_ASSERT_EQUAL(3, seenCount);

  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

void test_vertex_induced_subgraph_planar(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < 5; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphAddEdge(&g, 1, 2);
  gvizGraphAddEdge(&g, 2, 3);
  gvizGraphAddEdge(&g, 3, 0);
  gvizGraphAddEdge(&g, 0, 4);

  gvizGraphBuildLayout(&g);
  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  for (size_t i = 0; i < 4; i++)
    gvizVertexSubsetShowVertex(vs, i);

  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);
  gvizPlanarEmbedderState state;
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbedderInit(&state, sg));

  TEST_ASSERT_EQUAL(4, gvizSubgraphVertexCount(&state.embedding.subgraph));
  TEST_ASSERT_EQUAL(8, gvizSubgraphEdgeCount(&state.embedding.subgraph));

  gvizFaceIteratorContext faces;
  TEST_ASSERT_EQUAL(0, gvizFaceIteratorInit(&state.embedding.subgraph, &faces));
  TEST_ASSERT_EQUAL(0, gvizPlanarEmbedderFaces(&state.embedding.subgraph, &faces));
  TEST_ASSERT_TRUE(faces.faces.count >= 1);

  gvizFaceIteratorRelease(&faces);
  gvizPlanarEmbedderRelease(&state);
  gvizGraphRelease(&g);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_planar);
  RUN_TEST(test_nonPlanar);
  RUN_TEST(test_nonPlanar_kuratowskiWitness);
  RUN_TEST(test_largestFaceBoundary_square);
  RUN_TEST(test_halfEdge_twin);
  RUN_TEST(test_triangulation);
  RUN_TEST(test_subgraph_neighbor_ccw_order);
  RUN_TEST(test_face_walk_triangle);
  RUN_TEST(test_vertex_induced_subgraph_planar);
  RUN_TEST(test_schnyderWood_K4);
  RUN_TEST(test_schnyderWood_hexagon);

  UNITY_END();
  return 0;
}
