#include "unity/unity.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizSubgraph.h"
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static void add_triangle(gvizGraph *g) {
  gvizGraphInit(g, 0);
  for (int i = 0; i < 3; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 0, 2);
}

static void add_path4(gvizGraph *g) {
  gvizGraphInit(g, 1);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 3);
}

// ============================================================================
// GRAPH LAYOUT
// ============================================================================

void test_graphLayout_null_on_init(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  TEST_ASSERT_NULL(g.layout);
  gvizGraphRelease(&g);
}

void test_graphBuildLayout_prefix_sums(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  TEST_ASSERT_NOT_NULL(g.layout);
  TEST_ASSERT_EQUAL_UINT64(4, g.layout->nvertices);
  TEST_ASSERT_EQUAL_UINT64(0, g.layout->vertexOffsets[0]);
  TEST_ASSERT_EQUAL_UINT64(1, g.layout->vertexOffsets[1]);
  TEST_ASSERT_EQUAL_UINT64(2, g.layout->vertexOffsets[2]);
  TEST_ASSERT_EQUAL_UINT64(3, g.layout->vertexOffsets[3]);
  TEST_ASSERT_EQUAL_UINT64(3, g.layout->vertexOffsets[4]);
  TEST_ASSERT_EQUAL_UINT64(3, g.layout->edgeCount);
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_graphBuildLayout_rebuild_after_edge_add(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphBuildLayout(&g);

  TEST_ASSERT_EQUAL_UINT64(0, g.layout->vertexOffsets[2]);

  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphBuildLayout(&g);

  TEST_ASSERT_EQUAL_UINT64(0, g.layout->vertexOffsets[0]);
  TEST_ASSERT_EQUAL_UINT64(1, g.layout->vertexOffsets[1]);
  TEST_ASSERT_EQUAL_UINT64(2, g.layout->vertexOffsets[2]);

  gvizGraphRelease(&g);
}

void test_graphRelease_frees_layout(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);
  gvizGraphLayout *layout = g.layout;
  TEST_ASSERT_NOT_NULL(layout);

  gvizGraphRelease(&g);
  (void)layout;
}

void test_graphClear_frees_layout(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);
  TEST_ASSERT_NOT_NULL(g.layout);

  gvizGraphClear(&g);
  TEST_ASSERT_NULL(g.layout);
  gvizGraphRelease(&g);
}

void test_graphEdgeCount_requires_layout(void) {
  gvizGraph g;
  add_triangle(&g);
  TEST_ASSERT_EQUAL_UINT64(0, gvizGraphEdgeCount(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(3, g.layout->edgeCount);
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

void test_graphEdgeCount_undirected_adj_entries(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  TEST_ASSERT_EQUAL_UINT64(6, g.layout->vertexOffsets[3]);
  TEST_ASSERT_EQUAL_UINT64(3, g.layout->edgeCount);

  gvizGraphRelease(&g);
}

void test_graphEdgeCount_rebuild_after_remove(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphEdgeCount(&g));

  gvizGraphRemoveEdge(&g, 0, 2);
  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(2, gvizGraphEdgeCount(&g));

  gvizGraphRelease(&g);
}

// ============================================================================
// EDGE SUBSET
// ============================================================================

void test_edgeSubset_shares_graph_layout(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizEdgeSubset es = gvizEdgeSubsetCreateEmpty(&g);
  TEST_ASSERT_NOT_NULL(es.bitset);
  TEST_ASSERT_EQUAL_PTR(g.layout, es.layout);

  gvizEdgeSubsetRelease(es);
  gvizGraphRelease(&g);
}

void test_edgeSubset_two_instances_share_layout(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizEdgeSubset a = gvizEdgeSubsetCreateEmpty(&g);
  gvizEdgeSubset b = gvizEdgeSubsetCreateEmpty(&g);

  TEST_ASSERT_EQUAL_PTR(g.layout, a.layout);
  TEST_ASSERT_EQUAL_PTR(g.layout, b.layout);
  TEST_ASSERT_EQUAL_PTR(a.layout, b.layout);

  gvizEdgeSubsetRelease(a);
  gvizEdgeSubsetRelease(b);
  gvizGraphRelease(&g);
}

void test_edgeSubset_show_hide_edge(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizEdgeSubset es = gvizEdgeSubsetCreateEmpty(&g);

  gvizEdgeSubsetShowEdge(es, 1, 0);
  TEST_ASSERT_TRUE(gvizTestBit(es.bitset, g.layout->vertexOffsets[1]));
  TEST_ASSERT_FALSE(gvizTestBit(es.bitset, g.layout->vertexOffsets[1] + 1));

  gvizEdgeSubsetHideEdge(es, 1, 0);
  TEST_ASSERT_FALSE(gvizTestBit(es.bitset, g.layout->vertexOffsets[1]));

  gvizEdgeSubsetRelease(es);
  gvizGraphRelease(&g);
}

void test_edgeSubset_independent_of_subgraph(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizEdgeSubset es = gvizEdgeSubsetCreateEmpty(&g);
  gvizEdgeSubsetShowEdge(es, 2, 0);

  size_t bit = g.layout->vertexOffsets[2];
  TEST_ASSERT_TRUE(gvizTestBit(es.bitset, bit));

  gvizEdgeSubsetRelease(es);
  gvizGraphRelease(&g);
}

// ============================================================================
// SUBGRAPH
// ============================================================================

void test_subgraphCreateEmpty(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  TEST_ASSERT_EQUAL_PTR(&g, sg.g);
  TEST_ASSERT_NOT_NULL(sg.vs);
  TEST_ASSERT_NOT_NULL(sg.es.bitset);
  TEST_ASSERT_EQUAL_PTR(g.layout, sg.es.layout);

  gvizSubgraphRelease(&sg);
  TEST_ASSERT_NULL(sg.vs);
  TEST_ASSERT_NULL(sg.es.bitset);
  TEST_ASSERT_NOT_NULL(g.layout);

  gvizGraphRelease(&g);
}

void test_subgraphCreateVertexInduced(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizVertexSubsetShowVertex(vs, 2);

  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);
  TEST_ASSERT_EQUAL_PTR(&g, sg.g);
  TEST_ASSERT_TRUE(gvizTestBit(sg.vs, 0));
  TEST_ASSERT_FALSE(gvizTestBit(sg.vs, 1));
  TEST_ASSERT_TRUE(gvizTestBit(sg.vs, 2));
  TEST_ASSERT_NULL(sg.es.bitset);
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 0));
  TEST_ASSERT_TRUE(gvizSubgraphHasEdge(&sg, 0, 2));
  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 0, 1));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_vertexSubset_show_hide(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 1);
  TEST_ASSERT_FALSE(gvizTestBit(vs, 0));
  TEST_ASSERT_TRUE(gvizTestBit(vs, 1));

  gvizVertexSubsetHideVertex(vs, 1);
  TEST_ASSERT_FALSE(gvizTestBit(vs, 1));

  gvizVertexSubsetRelease(vs);
  gvizGraphRelease(&g);
}

static void build_square(gvizGraph *g) {
  gvizGraphInit(g, 0);
  for (int i = 0; i < 4; i++)
    gvizGraphAddVertex(g, NULL, NULL, NULL);
  gvizGraphAddEdge(g, 0, 1);
  gvizGraphAddEdge(g, 1, 2);
  gvizGraphAddEdge(g, 2, 3);
  gvizGraphAddEdge(g, 3, 0);
}

static size_t collect_vertices(const gvizSubgraph *sg, size_t *out, size_t max) {
  gvizSubgraphVertexIterator it = gvizSubgraphVertexIteratorCreate(sg);
  size_t n = 0;
  size_t u;
  while (gvizSubgraphVertexIterate(&it, &u) && n < max)
    out[n++] = u;
  return n;
}

static size_t collect_neighbors(const gvizSubgraph *sg, size_t u, size_t *out,
                                size_t max) {
  gvizSubgraphNeighborIterator it = gvizSubgraphNeighborIteratorCreate(sg, u);
  size_t n = 0;
  size_t v;
  while (gvizSubgraphNeighborIterate(&it, &v) && n < max)
    out[n++] = v;
  return n;
}

// ============================================================================
// MAKE SUBSETS
// ============================================================================

void test_makeEdgeSubset_vertex_induced(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizVertexSubsetShowVertex(vs, 1);
  gvizVertexSubsetShowVertex(vs, 2);

  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);
  gvizSubgraphMakeEdgeSubset(&sg);

  TEST_ASSERT_NOT_NULL(sg.es.bitset);
  TEST_ASSERT_EQUAL_UINT64(6, gvizSubgraphEdgeCount(&sg));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_makeEdgeSubset_partial_vertices(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizVertexSubsetShowVertex(vs, 2);

  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);
  gvizSubgraphMakeEdgeSubset(&sg);

  TEST_ASSERT_TRUE(gvizSubgraphHasEdge(&sg, 0, 2));
  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 0, 1));
  TEST_ASSERT_EQUAL_UINT64(2, gvizSubgraphEdgeCount(&sg));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_vertex_induced_query_without_edge_subset(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizVertexSubsetShowVertex(vs, 1);
  gvizVertexSubsetShowVertex(vs, 2);
  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);

  TEST_ASSERT_EQUAL_UINT64(3, gvizSubgraphVertexCount(&sg));
  TEST_ASSERT_EQUAL_UINT64(6, gvizSubgraphEdgeCount(&sg));
  TEST_ASSERT_EQUAL_UINT64(2, gvizSubgraphDegree(&sg, 0));

  size_t nbrs[4];
  TEST_ASSERT_EQUAL_UINT64(2, collect_neighbors(&sg, 0, nbrs, 4));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

// ============================================================================
// VIEW / HIDE
// ============================================================================

void test_subgraph_view_hide_vertex_and_edge(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphShowVertex(&sg, 1);
  gvizSubgraphShowVertex(&sg, 2);
  gvizSubgraphShowEdge(&sg, 1, 2);

  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 1));
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 2));
  TEST_ASSERT_TRUE(gvizSubgraphHasEdge(&sg, 1, 2));
  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 0, 1));

  gvizSubgraphHideEdge(&sg, 1, 2);
  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 1, 2));

  gvizSubgraphShowEdge(&sg, 1, 2);
  gvizSubgraphHideVertex(&sg, 2);
  TEST_ASSERT_FALSE(gvizSubgraphHasVertex(&sg, 2));
  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 1, 2));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

// ============================================================================
// QUERY / ITERATION
// ============================================================================

void test_subgraph_query_counts(void) {
  gvizGraph g;
  build_square(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  for (size_t i = 0; i < 4; i++)
    gvizSubgraphShowVertex(&sg, i);
  gvizSubgraphShowEdge(&sg, 0, 1);
  gvizSubgraphShowEdge(&sg, 1, 2);
  gvizSubgraphShowEdge(&sg, 2, 3);

  TEST_ASSERT_EQUAL_UINT64(4, gvizSubgraphVertexCount(&sg));
  TEST_ASSERT_EQUAL_UINT64(3, gvizSubgraphEdgeCount(&sg));
  TEST_ASSERT_EQUAL_UINT64(1, gvizSubgraphDegree(&sg, 0));
  TEST_ASSERT_EQUAL_UINT64(1, gvizSubgraphDegree(&sg, 1));
  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 0, 2));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_subgraph_vertex_iteration(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphShowVertex(&sg, 0);
  gvizSubgraphShowVertex(&sg, 2);

  size_t verts[4];
  size_t n = collect_vertices(&sg, verts, 4);
  TEST_ASSERT_EQUAL_UINT64(2, n);
  TEST_ASSERT_EQUAL_UINT64(0, verts[0]);
  TEST_ASSERT_EQUAL_UINT64(2, verts[1]);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_subgraph_neighbor_iteration(void) {
  gvizGraph g;
  build_square(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  for (size_t i = 0; i < 4; i++)
    gvizSubgraphShowVertex(&sg, i);
  gvizSubgraphShowEdge(&sg, 0, 1);
  gvizSubgraphShowEdge(&sg, 0, 3);

  size_t nbrs[4];
  size_t n = collect_neighbors(&sg, 0, nbrs, 4);
  TEST_ASSERT_EQUAL_UINT64(2, n);
  TEST_ASSERT_EQUAL_UINT64(1, nbrs[0]);
  TEST_ASSERT_EQUAL_UINT64(3, nbrs[1]);

  n = collect_neighbors(&sg, 2, nbrs, 4);
  TEST_ASSERT_EQUAL_UINT64(0, n);

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_subgraph_query_vertex_induced_partial(void) {
  gvizGraph g;
  add_triangle(&g);
  gvizGraphBuildLayout(&g);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);

  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 0));
  TEST_ASSERT_EQUAL_UINT64(1, gvizSubgraphVertexCount(&sg));
  TEST_ASSERT_EQUAL_UINT64(0, gvizSubgraphDegree(&sg, 0));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

// ============================================================================
// REBUILD
// ============================================================================

void test_subgraph_rebuild_full_preserves_bits(void) {
  gvizGraph g;
  add_path4(&g);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphShowVertex(&sg, 1);
  gvizSubgraphShowVertex(&sg, 2);
  gvizSubgraphShowEdge(&sg, 1, 2);

  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 3, 4);
  TEST_ASSERT_EQUAL_INT(0, gvizSubgraphRebuild(&sg));

  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 1));
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 2));
  TEST_ASSERT_FALSE(gvizSubgraphHasVertex(&sg, 4));
  TEST_ASSERT_TRUE(gvizSubgraphHasEdge(&sg, 1, 2));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_subgraph_rebuild_vertex_induced_no_edge_subset(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphBuildLayout(&g);

  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(&g);
  gvizVertexSubsetShowVertex(vs, 0);
  gvizSubgraph sg = gvizSubgraphCreateVertexInduced(&g, vs);

  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  TEST_ASSERT_EQUAL_INT(0, gvizSubgraphRebuild(&sg));

  TEST_ASSERT_NOT_NULL(sg.vs);
  TEST_ASSERT_NULL(sg.es.bitset);
  TEST_ASSERT_TRUE(gvizTestBit(sg.vs, 0));
  TEST_ASSERT_FALSE(gvizTestBit(sg.vs, 2));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

void test_subgraph_rebuild_drops_removed_edge(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddVertex(&g, NULL, NULL, NULL);
  gvizGraphAddEdge(&g, 0, 1);
  gvizGraphBuildLayout(&g);

  gvizSubgraph sg = gvizSubgraphCreateEmpty(&g);
  gvizSubgraphShowVertex(&sg, 0);
  gvizSubgraphShowVertex(&sg, 1);
  gvizSubgraphShowEdge(&sg, 0, 1);

  gvizGraphRemoveEdge(&g, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, gvizSubgraphRebuild(&sg));

  TEST_ASSERT_FALSE(gvizSubgraphHasEdge(&sg, 0, 1));
  TEST_ASSERT_TRUE(gvizSubgraphHasVertex(&sg, 0));

  gvizSubgraphRelease(&sg);
  gvizGraphRelease(&g);
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_graphLayout_null_on_init);
  RUN_TEST(test_graphBuildLayout_prefix_sums);
  RUN_TEST(test_graphBuildLayout_rebuild_after_edge_add);
  RUN_TEST(test_graphRelease_frees_layout);
  RUN_TEST(test_graphClear_frees_layout);
  RUN_TEST(test_graphEdgeCount_requires_layout);
  RUN_TEST(test_graphEdgeCount_undirected_adj_entries);
  RUN_TEST(test_graphEdgeCount_rebuild_after_remove);
  RUN_TEST(test_edgeSubset_shares_graph_layout);
  RUN_TEST(test_edgeSubset_two_instances_share_layout);
  RUN_TEST(test_edgeSubset_show_hide_edge);
  RUN_TEST(test_edgeSubset_independent_of_subgraph);
  RUN_TEST(test_subgraphCreateEmpty);
  RUN_TEST(test_subgraphCreateVertexInduced);
  RUN_TEST(test_vertexSubset_show_hide);
  RUN_TEST(test_makeEdgeSubset_vertex_induced);
  RUN_TEST(test_makeEdgeSubset_partial_vertices);
  RUN_TEST(test_vertex_induced_query_without_edge_subset);
  RUN_TEST(test_subgraph_view_hide_vertex_and_edge);
  RUN_TEST(test_subgraph_query_counts);
  RUN_TEST(test_subgraph_vertex_iteration);
  RUN_TEST(test_subgraph_neighbor_iteration);
  RUN_TEST(test_subgraph_query_vertex_induced_partial);
  RUN_TEST(test_subgraph_rebuild_full_preserves_bits);
  RUN_TEST(test_subgraph_rebuild_vertex_induced_no_edge_subset);
  RUN_TEST(test_subgraph_rebuild_drops_removed_edge);

  int result = UNITY_END();
  printf("\n");
  return result;
}
