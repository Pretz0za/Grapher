#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizGraphView.h"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

// Build a small undirected graph: 0-1, 1-2, 2-3, 3-0, 1-3 (5 edges, 10 slots)
static void buildSmallGraph(gvizGraph *g) {
  gvizGraphInit(g, 0);
  for (int i = 0; i < 4; i++) {
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  }
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 3);
  gvizGraphAddEdge(g, 3, 0);
  gvizGraphAddEdge(g, 1, 3);
}

// ============================================================================
// INIT / RELEASE
// ============================================================================

void test_view_init_full(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  TEST_ASSERT_EQUAL(0, gvizGraphViewInitFull(&v, &g));
  TEST_ASSERT_EQUAL(4, gvizGraphViewVertexCount(&v));
  TEST_ASSERT_NULL(v.vertexMask);
  TEST_ASSERT_NULL(v.edgeMask);
  TEST_ASSERT_EQUAL(1, gvizGraphViewVertexInView(&v, 0));
  TEST_ASSERT_EQUAL(1, gvizGraphViewVertexInView(&v, 3));
  TEST_ASSERT_EQUAL(0, gvizGraphViewVertexInView(&v, 99));
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_init_from_vertices(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  GVIZ_BIT_ARRAY mask = gvizBitArrayAlloc(4);
  gvizSetBit(mask, 0);
  gvizSetBit(mask, 1);
  gvizSetBit(mask, 2);

  gvizGraphView v;
  TEST_ASSERT_EQUAL(0, gvizGraphViewInitFromVertices(&v, &g, mask));
  TEST_ASSERT_EQUAL(3, gvizGraphViewVertexCount(&v));
  TEST_ASSERT_EQUAL(1, gvizGraphViewVertexInView(&v, 1));
  TEST_ASSERT_EQUAL(0, gvizGraphViewVertexInView(&v, 3));

  gvizBitArrayFree(mask);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_total_edge_slots(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  // undirected: each edge contributes 2 slots; 5 edges * 2 = 10
  TEST_ASSERT_EQUAL(10, gvizGraphViewTotalEdgeSlots(&v));
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_init_from_edges(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView tmp;
  gvizGraphViewInitFull(&tmp, &g);
  size_t total = gvizGraphViewTotalEdgeSlots(&tmp);
  gvizGraphViewRelease(&tmp);

  GVIZ_BIT_ARRAY emask = gvizBitArrayAlloc(total);
  for (size_t i = 0; i < total; i++) {
    gvizSetBit(emask, i);
  }

  gvizGraphView v;
  TEST_ASSERT_EQUAL(0, gvizGraphViewInitFromEdges(&v, &g, emask));
  TEST_ASSERT_NOT_NULL(v.edgeStart);
  TEST_ASSERT_EQUAL(0, v.edgeStart[0]);
  TEST_ASSERT_EQUAL(total, v.edgeStart[4]);
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 0, 1));

  gvizBitArrayFree(emask);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

// ============================================================================
// READ INTERFACE
// ============================================================================

void test_view_edge_in_view_full(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 0, 1));
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 1, 0));
  TEST_ASSERT_EQUAL(0, gvizGraphViewEdgeInView(&v, 0, 2));
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_edge_in_view_vertex_filtered(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  // exclude vertex 3
  GVIZ_BIT_ARRAY mask = gvizBitArrayAlloc(4);
  gvizSetBit(mask, 0);
  gvizSetBit(mask, 1);
  gvizSetBit(mask, 2);
  gvizGraphView v;
  gvizGraphViewInitFromVertices(&v, &g, mask);
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 0, 1));
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 1, 2));
  TEST_ASSERT_EQUAL(0, gvizGraphViewEdgeInView(&v, 1, 3));
  TEST_ASSERT_EQUAL(0, gvizGraphViewEdgeInView(&v, 0, 3));
  gvizBitArrayFree(mask);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_neighbors_iter_full(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  // vertex 1 has neighbors 0, 2, 3
  gvizGraphViewNeighborsIter it;
  gvizGraphViewNeighborsIterInit(&it, &v, 1);
  size_t out;
  int seen[4] = {0};
  int n = 0;
  while (gvizGraphViewNeighborsIterNext(&it, &out)) {
    TEST_ASSERT_TRUE(out < 4);
    seen[out] = 1;
    n++;
  }
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_TRUE(seen[0]);
  TEST_ASSERT_TRUE(seen[2]);
  TEST_ASSERT_TRUE(seen[3]);
  gvizGraphViewNeighborsIterRelease(&it);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_neighbors_iter_vertex_filtered(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  GVIZ_BIT_ARRAY mask = gvizBitArrayAlloc(4);
  gvizSetBit(mask, 0);
  gvizSetBit(mask, 1);
  gvizSetBit(mask, 2);
  gvizGraphView v;
  gvizGraphViewInitFromVertices(&v, &g, mask);
  gvizGraphViewNeighborsIter it;
  gvizGraphViewNeighborsIterInit(&it, &v, 1);
  size_t out;
  int n = 0;
  int seen[4] = {0};
  while (gvizGraphViewNeighborsIterNext(&it, &out)) {
    seen[out] = 1;
    n++;
  }
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_TRUE(seen[0]);
  TEST_ASSERT_TRUE(seen[2]);
  TEST_ASSERT_FALSE(seen[3]);
  gvizGraphViewNeighborsIterRelease(&it);
  gvizBitArrayFree(mask);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_vertex_iter_full(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  gvizGraphViewVertexIter it;
  gvizGraphViewVertexIterInit(&it, &v);
  size_t out, expected = 0;
  while (gvizGraphViewVertexIterNext(&it, &out)) {
    TEST_ASSERT_EQUAL(expected, out);
    expected++;
  }
  TEST_ASSERT_EQUAL(4, expected);
  gvizGraphViewVertexIterRelease(&it);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_vertex_iter_masked(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  GVIZ_BIT_ARRAY mask = gvizBitArrayAlloc(4);
  gvizSetBit(mask, 1);
  gvizSetBit(mask, 3);
  gvizGraphView v;
  gvizGraphViewInitFromVertices(&v, &g, mask);
  gvizGraphViewVertexIter it;
  gvizGraphViewVertexIterInit(&it, &v);
  size_t out;
  TEST_ASSERT_EQUAL(1, gvizGraphViewVertexIterNext(&it, &out));
  TEST_ASSERT_EQUAL(1, out);
  TEST_ASSERT_EQUAL(1, gvizGraphViewVertexIterNext(&it, &out));
  TEST_ASSERT_EQUAL(3, out);
  TEST_ASSERT_EQUAL(0, gvizGraphViewVertexIterNext(&it, &out));
  gvizGraphViewVertexIterRelease(&it);
  gvizBitArrayFree(mask);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_degree(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  TEST_ASSERT_EQUAL(2, gvizGraphViewDegree(&v, 0));
  TEST_ASSERT_EQUAL(3, gvizGraphViewDegree(&v, 1));
  TEST_ASSERT_EQUAL(2, gvizGraphViewDegree(&v, 2));
  TEST_ASSERT_EQUAL(3, gvizGraphViewDegree(&v, 3));
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

// ============================================================================
// MUTATION
// ============================================================================

void test_view_remove_vertex_updates_count(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  TEST_ASSERT_EQUAL(4, v.count);
  TEST_ASSERT_EQUAL(0, gvizGraphViewRemoveVertex(&v, 2));
  TEST_ASSERT_EQUAL(3, v.count);
  TEST_ASSERT_EQUAL(0, gvizGraphViewVertexInView(&v, 2));
  TEST_ASSERT_EQUAL(1, gvizGraphViewVertexInView(&v, 0));
  // remove again -> idempotent
  TEST_ASSERT_EQUAL(0, gvizGraphViewRemoveVertex(&v, 2));
  TEST_ASSERT_EQUAL(3, v.count);
  // re-add
  TEST_ASSERT_EQUAL(0, gvizGraphViewAddVertex(&v, 2));
  TEST_ASSERT_EQUAL(4, v.count);
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_remove_edge_flips_bit(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 0, 1));
  TEST_ASSERT_EQUAL(0, gvizGraphViewRemoveEdge(&v, 0, 1));
  TEST_ASSERT_EQUAL(0, gvizGraphViewEdgeInView(&v, 0, 1));
  // reverse direction unaffected (mutation is directed)
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 1, 0));
  TEST_ASSERT_EQUAL(0, gvizGraphViewAddEdge(&v, 0, 1));
  TEST_ASSERT_EQUAL(1, gvizGraphViewEdgeInView(&v, 0, 1));
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

void test_view_edgestart_consistency(void) {
  gvizGraph g;
  buildSmallGraph(&g);
  gvizGraphView v;
  gvizGraphViewInitFull(&v, &g);
  gvizGraphViewRebuildEdgeStart(&v);
  TEST_ASSERT_EQUAL(0, v.edgeStart[0]);
  TEST_ASSERT_EQUAL(2, v.edgeStart[1]); // deg(0) = 2
  TEST_ASSERT_EQUAL(5, v.edgeStart[2]); // deg(1) = 3
  TEST_ASSERT_EQUAL(7, v.edgeStart[3]); // deg(2) = 2
  TEST_ASSERT_EQUAL(10, v.edgeStart[4]); // deg(3) = 3
  gvizGraphViewRelease(&v);
  gvizGraphRelease(&g);
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_view_init_full);
  RUN_TEST(test_view_init_from_vertices);
  RUN_TEST(test_view_total_edge_slots);
  RUN_TEST(test_view_init_from_edges);
  RUN_TEST(test_view_edge_in_view_full);
  RUN_TEST(test_view_edge_in_view_vertex_filtered);
  RUN_TEST(test_view_neighbors_iter_full);
  RUN_TEST(test_view_neighbors_iter_vertex_filtered);
  RUN_TEST(test_view_vertex_iter_full);
  RUN_TEST(test_view_vertex_iter_masked);
  RUN_TEST(test_view_degree);
  RUN_TEST(test_view_remove_vertex_updates_count);
  RUN_TEST(test_view_remove_edge_flips_bit);
  RUN_TEST(test_view_edgestart_consistency);
  return UNITY_END();
}
