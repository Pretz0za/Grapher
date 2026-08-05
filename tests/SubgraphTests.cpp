// Port of tests/ds/gvizSubgraphTests.c against gviz::Subgraph (and the
// Graph-layout machinery it depends on).
//
// Scenarios intentionally dropped:
//   - Raw vertexOffsets/edgeCount-field assertions
//     (test_graphBuildLayout_prefix_sums,
//     test_graphBuildLayout_rebuild_after_edge_add,
//     test_graphEdgeCount_undirected_adj_entries): Graph::Layout is now a
//     private, friend-only implementation detail (see the "Subgraph needs
//     privileged access" design note in Graph.hpp) -- the old tests peeked
//     at internal prefix-sum values that have no public accessor anymore.
//     The *behavior* those values existed to support (EdgeCount() being
//     correct before/after a rebuild) is covered by
//     test_graph_hasLayout_falseInitially/test_graph_buildLayout_*/
//     test_graph_ensureLayout_* in GraphTests.cpp, plus
//     test_graph_edgeCount_rebuildsAfterRemove below.
//   - test_graphRelease_frees_layout: no Release() -- RAII/the destructor
//     handles it, nothing to assert.
//   - test_graphClear_frees_layout, test_graphEdgeCount_requires_layout:
//     already covered in GraphTests.cpp
//     (test_graph_clear_dropsLayout/test_graph_hasLayout_falseInitially).
//   - test_edgeSubset_shares_graph_layout,
//     test_edgeSubset_two_instances_share_layout,
//     test_edgeSubset_independent_of_subgraph: these tested the standalone
//     gvizEdgeSubset type (a raw bitset + layout pointer) directly,
//     including its exact bit-position formula. Per the port plan,
//     Subgraph is one concrete class with an *internal* mode discriminant
//     -- there is no public EdgeSubset/VertexSubset type anymore, so there
//     is nothing to construct standalone or assert a bit position against.
//   - test_vertexSubset_show_hide: same reasoning for the standalone
//     VertexSubset type; the show/hide behavior itself is covered by
//     test_subgraph_view_hide_vertex_and_edge below (through Subgraph's
//     own ShowVertex/HideVertex).
//
// New coverage added beyond the old suite: CreateEmpty/CreateFull throwing
// NoLayoutError when the parent has no layout is new behavior (the old C
// gvizSubgraphCreateEmpty/CreateFull instead returned a subgraph with
// g == NULL, which doesn't exist as a representable state anymore), and
// the copy constructor's independence had no C analogue (gvizSubgraph was
// copied by raw struct assignment, which old code never actually did).

#include "Subgraph.hpp"

#include "Error.hpp"
#include "unity/unity.h"

#include <vector>

void setUp(void) {}
void tearDown(void) {}

static void add_triangle(gviz::Graph &g) {
  for (int i = 0; i < 3; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(0, 2, 1.0);
}

static void add_path4(gviz::Graph &g) {
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
}

static void build_square(gviz::Graph &g) {
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 0, 1.0);
}

static size_t collect_vertices(const gviz::Subgraph &sg, size_t *out, size_t max) {
  size_t n = 0;
  for (size_t u : sg) {
    if (n >= max)
      break;
    out[n++] = u;
  }
  return n;
}

static size_t collect_neighbors(const gviz::Subgraph &sg, size_t u, size_t *out,
                                 size_t max) {
  size_t n = 0;
  for (size_t v : sg.Neighbors(u)) {
    if (n >= max)
      break;
    out[n++] = v;
  }
  return n;
}

// ============================================================================
// SHARED LAYOUT MACHINERY
// ============================================================================

static void test_graph_edgeCount_rebuildsAfterRemove(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(3, g.EdgeCount());

  g.RemoveEdge(0, 2);
  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(2, g.EdgeCount());
}

// ============================================================================
// EDGE SHOW/HIDE (behavioral equivalent of the old standalone EdgeSubset)
// ============================================================================

static void test_edgeSubset_show_hide_edge(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  sg.ShowVertex(0);
  sg.ShowVertex(1);
  sg.ShowEdge(1, 0);
  TEST_ASSERT_TRUE(sg.HasEdge(1, 0));

  sg.HideEdge(1, 0);
  TEST_ASSERT_FALSE(sg.HasEdge(1, 0));
}

// ============================================================================
// CREATE
// ============================================================================

static void test_subgraphCreateEmpty(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  TEST_ASSERT_TRUE(sg.IsFull());
  TEST_ASSERT_EQUAL_UINT64(0, sg.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(0, sg.EdgeCount());
}

static void test_subgraphCreateEmpty_throwsWithoutLayout(void) {
  gviz::Graph g(false);
  add_triangle(g);
  // Deliberately never call BuildLayout.
  bool threw = false;
  try {
    gviz::Subgraph::CreateEmpty(g);
  } catch (const gviz::NoLayoutError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_subgraphCreateFull_throwsWithoutLayout(void) {
  gviz::Graph g(false);
  add_triangle(g);
  bool threw = false;
  try {
    gviz::Subgraph::CreateFull(g);
  } catch (const gviz::NoLayoutError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_subgraphCreateVertexInduced(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  sg.ShowVertex(2);

  TEST_ASSERT_FALSE(sg.IsFull());
  TEST_ASSERT_TRUE(sg.HasVertex(0));
  TEST_ASSERT_FALSE(sg.HasVertex(1));
  TEST_ASSERT_TRUE(sg.HasVertex(2));
  TEST_ASSERT_TRUE(sg.HasEdge(0, 2));
  TEST_ASSERT_FALSE(sg.HasEdge(0, 1));
}

static void test_subgraphCreateVertexInduced_neverNeedsLayout(void) {
  gviz::Graph g(false);
  add_triangle(g);
  // No BuildLayout() call anywhere -- must not throw.
  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  TEST_ASSERT_TRUE(sg.HasVertex(0));
  TEST_ASSERT_FALSE(g.HasLayout());
}

// ============================================================================
// MAKE EDGE SUBSET / MAKE FULL
// ============================================================================

static void test_makeEdgeSubset_vertex_induced(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  sg.ShowVertex(1);
  sg.ShowVertex(2);
  sg.MakeEdgeSubset();

  TEST_ASSERT_TRUE(sg.IsFull());
  TEST_ASSERT_EQUAL_UINT64(6, sg.EdgeCount()); // 3 undirected edges, both directions
}

static void test_makeEdgeSubset_partial_vertices(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  sg.ShowVertex(2);
  sg.MakeEdgeSubset();

  TEST_ASSERT_TRUE(sg.HasEdge(0, 2));
  TEST_ASSERT_FALSE(sg.HasEdge(0, 1));
  TEST_ASSERT_EQUAL_UINT64(2, sg.EdgeCount());
}

static void test_vertex_induced_query_without_edge_subset(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  sg.ShowVertex(1);
  sg.ShowVertex(2);

  TEST_ASSERT_EQUAL_UINT64(3, sg.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(6, sg.EdgeCount());
  TEST_ASSERT_EQUAL_UINT64(2, sg.Degree(0));

  size_t nbrs[4];
  TEST_ASSERT_EQUAL_UINT64(2, collect_neighbors(sg, 0, nbrs, 4));
}

// ============================================================================
// VIEW / HIDE
// ============================================================================

static void test_subgraph_view_hide_vertex_and_edge(void) {
  gviz::Graph g(true);
  add_path4(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  sg.ShowVertex(1);
  sg.ShowVertex(2);
  sg.ShowEdge(1, 2);

  TEST_ASSERT_TRUE(sg.HasVertex(1));
  TEST_ASSERT_TRUE(sg.HasVertex(2));
  TEST_ASSERT_TRUE(sg.HasEdge(1, 2));
  TEST_ASSERT_FALSE(sg.HasEdge(0, 1));

  sg.HideEdge(1, 2);
  TEST_ASSERT_FALSE(sg.HasEdge(1, 2));

  sg.ShowEdge(1, 2);
  sg.HideVertex(2);
  TEST_ASSERT_FALSE(sg.HasVertex(2));
  TEST_ASSERT_FALSE(sg.HasEdge(1, 2));
}

// ============================================================================
// QUERY / ITERATION
// ============================================================================

static void test_subgraph_query_counts(void) {
  gviz::Graph g(false);
  build_square(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  for (size_t i = 0; i < 4; i++)
    sg.ShowVertex(i);
  sg.ShowEdge(0, 1);
  sg.ShowEdge(1, 2);
  sg.ShowEdge(2, 3);

  TEST_ASSERT_EQUAL_UINT64(4, sg.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(3, sg.EdgeCount());
  TEST_ASSERT_EQUAL_UINT64(1, sg.Degree(0));
  TEST_ASSERT_EQUAL_UINT64(1, sg.Degree(1));
  TEST_ASSERT_FALSE(sg.HasEdge(0, 2));
}

static void test_subgraph_vertex_iteration(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  sg.ShowVertex(0);
  sg.ShowVertex(2);

  size_t verts[4];
  size_t n = collect_vertices(sg, verts, 4);
  TEST_ASSERT_EQUAL_UINT64(2, n);
  TEST_ASSERT_EQUAL_UINT64(0, verts[0]);
  TEST_ASSERT_EQUAL_UINT64(2, verts[1]);
}

static void test_subgraph_neighbor_iteration(void) {
  gviz::Graph g(false);
  build_square(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  for (size_t i = 0; i < 4; i++)
    sg.ShowVertex(i);
  sg.ShowEdge(0, 1);
  sg.ShowEdge(0, 3);

  size_t nbrs[4];
  size_t n = collect_neighbors(sg, 0, nbrs, 4);
  TEST_ASSERT_EQUAL_UINT64(2, n);
  TEST_ASSERT_EQUAL_UINT64(1, nbrs[0]);
  TEST_ASSERT_EQUAL_UINT64(3, nbrs[1]);

  n = collect_neighbors(sg, 2, nbrs, 4);
  TEST_ASSERT_EQUAL_UINT64(0, n);
}

static void test_subgraph_query_vertex_induced_partial(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);

  TEST_ASSERT_TRUE(sg.HasVertex(0));
  TEST_ASSERT_EQUAL_UINT64(1, sg.VertexCount());
  TEST_ASSERT_EQUAL_UINT64(0, sg.Degree(0));
}

// ============================================================================
// REBUILD
// ============================================================================

static void test_subgraph_rebuild_full_preserves_bits(void) {
  gviz::Graph g(true);
  add_path4(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  sg.ShowVertex(1);
  sg.ShowVertex(2);
  sg.ShowEdge(1, 2);

  g.AddVertex();
  g.AddEdge(3, 4, 1.0);
  sg.Rebuild();

  TEST_ASSERT_TRUE(sg.HasVertex(1));
  TEST_ASSERT_TRUE(sg.HasVertex(2));
  TEST_ASSERT_FALSE(sg.HasVertex(4));
  TEST_ASSERT_TRUE(sg.HasEdge(1, 2));
}

static void test_subgraph_rebuild_vertex_induced_no_edge_subset(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);

  g.AddVertex();
  sg.Rebuild();

  TEST_ASSERT_FALSE(sg.IsFull());
  TEST_ASSERT_TRUE(sg.HasVertex(0));
  TEST_ASSERT_FALSE(sg.HasVertex(2));
}

static void test_subgraph_rebuild_drops_removed_edge(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  sg.ShowVertex(0);
  sg.ShowVertex(1);
  sg.ShowEdge(0, 1);

  g.RemoveEdge(0, 1);
  sg.Rebuild();

  TEST_ASSERT_FALSE(sg.HasEdge(0, 1));
  TEST_ASSERT_TRUE(sg.HasVertex(0));
}

// ============================================================================
// VERTEX-INDUCED INDEPENDENCE FROM GRAPH LAYOUT
// ============================================================================

static void test_vertex_induced_works_without_layout(void) {
  gviz::Graph g(false);
  add_triangle(g);
  TEST_ASSERT_FALSE(g.HasLayout());

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  sg.ShowVertex(2);

  TEST_ASSERT_FALSE(sg.IsFull());
  TEST_ASSERT_TRUE(sg.HasVertex(0));
  TEST_ASSERT_EQUAL_UINT64(2, sg.VertexCount());
  TEST_ASSERT_TRUE(sg.HasEdge(0, 2));
  TEST_ASSERT_FALSE(g.HasLayout());
}

static void test_vertex_induced_rebuild_grows_without_layout(void) {
  gviz::Graph g(false);
  g.AddVertex();
  g.AddVertex();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);
  TEST_ASSERT_EQUAL_UINT64(2, sg.VertexCapacity());

  g.AddVertex();
  sg.Rebuild();

  TEST_ASSERT_FALSE(g.HasLayout());
  // Only vertex 0 was ever shown; growth alone doesn't add vertices.
  TEST_ASSERT_EQUAL_UINT64(1, sg.VertexCount());
  TEST_ASSERT_TRUE(sg.HasVertex(0));
  TEST_ASSERT_FALSE(sg.HasVertex(2));
  TEST_ASSERT_EQUAL_UINT64(4, sg.VertexCapacity());
}

static void test_vertex_induced_rebuild_capacity_doubles(void) {
  gviz::Graph g(false);
  g.AddVertex();

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  TEST_ASSERT_EQUAL_UINT64(1, sg.VertexCapacity());

  for (int i = 0; i < 3; i++) {
    g.AddVertex();
    sg.Rebuild();
  }
  // Graph now has 4 vertices; capacity should have doubled (1 -> 2 -> 4)
  // rather than exact-fitting on every single addition.
  TEST_ASSERT_EQUAL_UINT64(4, sg.VertexCapacity());

  g.AddVertex();
  sg.Rebuild();
  TEST_ASSERT_EQUAL_UINT64(8, sg.VertexCapacity());
}

static void test_subgraph_rebuild_full_updates_vertex_capacity(void) {
  gviz::Graph g(true);
  add_path4(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateFull(g);
  TEST_ASSERT_EQUAL_UINT64(4, sg.VertexCapacity());

  g.AddVertex();
  sg.Rebuild();
  TEST_ASSERT_EQUAL_UINT64(5, sg.VertexCapacity());
}

static void test_subgraphIsFull_predicate(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph full = gviz::Subgraph::CreateEmpty(g);
  TEST_ASSERT_TRUE(full.IsFull());

  gviz::Subgraph induced = gviz::Subgraph::CreateVertexInduced(g);
  TEST_ASSERT_FALSE(induced.IsFull());
}

// ============================================================================
// COPY CONSTRUCTION
// ============================================================================

static void test_subgraph_copyCtor_isIndependentDeepCopy(void) {
  gviz::Graph g(false);
  add_triangle(g);
  g.BuildLayout();

  gviz::Subgraph sg = gviz::Subgraph::CreateEmpty(g);
  sg.ShowVertex(0);
  sg.ShowVertex(1);
  sg.ShowEdge(0, 1);

  gviz::Subgraph copy(sg);
  sg.ShowVertex(2);
  sg.ShowEdge(1, 2);

  TEST_ASSERT_TRUE(copy.HasVertex(0));
  TEST_ASSERT_TRUE(copy.HasVertex(1));
  TEST_ASSERT_FALSE(copy.HasVertex(2)); // unaffected by the later mutation on sg
  TEST_ASSERT_TRUE(copy.HasEdge(0, 1));
  TEST_ASSERT_FALSE(copy.HasEdge(1, 2));
}

static void test_subgraph_copyCtor_preservesVertexInducedMode(void) {
  gviz::Graph g(false);
  add_triangle(g);

  gviz::Subgraph sg = gviz::Subgraph::CreateVertexInduced(g);
  sg.ShowVertex(0);

  gviz::Subgraph copy(sg);
  TEST_ASSERT_FALSE(copy.IsFull());
  TEST_ASSERT_TRUE(copy.HasVertex(0));
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_graph_edgeCount_rebuildsAfterRemove);

  RUN_TEST(test_edgeSubset_show_hide_edge);

  RUN_TEST(test_subgraphCreateEmpty);
  RUN_TEST(test_subgraphCreateEmpty_throwsWithoutLayout);
  RUN_TEST(test_subgraphCreateFull_throwsWithoutLayout);
  RUN_TEST(test_subgraphCreateVertexInduced);
  RUN_TEST(test_subgraphCreateVertexInduced_neverNeedsLayout);

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

  RUN_TEST(test_vertex_induced_works_without_layout);
  RUN_TEST(test_vertex_induced_rebuild_grows_without_layout);
  RUN_TEST(test_vertex_induced_rebuild_capacity_doubles);
  RUN_TEST(test_subgraph_rebuild_full_updates_vertex_capacity);
  RUN_TEST(test_subgraphIsFull_predicate);

  RUN_TEST(test_subgraph_copyCtor_isIndependentDeepCopy);
  RUN_TEST(test_subgraph_copyCtor_preservesVertexInducedMode);

  return UNITY_END();
}
