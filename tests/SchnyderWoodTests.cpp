// Port of the Schnyder-wood portion of tests/embedders/gvizPlanarTests.c
// against gviz::layout::SchnyderWood.
//
// See SchnyderWood.hpp's IMPLEMENTATION STATUS note: the constructor
// (canonical-ordering construction) is believed correct and is checked
// structurally here, same as the old C test did. Embed() ports a
// known-incomplete algorithm (an unresolved TODO in the original C); this
// suite only checks that it runs and produces finite coordinates for every
// non-root vertex, exactly as weak as the old test's coverage (which just
// printed the result), not that the drawing is geometrically correct.

#include "SchnyderWood.hpp"

#include "Error.hpp"
#include "Graph.hpp"
#include "Planar.hpp"
#include "Subgraph.hpp"
#include "unity/unity.h"

#include <cmath>

using gviz::Graph;
using gviz::Subgraph;
using gviz::layout::EmbeddedGraph;
using gviz::layout::FaceEnumerator;
using gviz::layout::Planar;
using gviz::layout::SchnyderWood;
using gviz::layout::Triangulate;

void setUp(void) {}
void tearDown(void) {}

// Follows parent[tree] pointers from v, returning the root reached (or
// kNone if a cycle is detected / too many hops -- defensive, mirrors the
// old C test's swFollowToRoot).
static size_t FollowToRoot(const SchnyderWood &sw, size_t tree, size_t v) {
  size_t hops = sw.Size() + 1;
  while (hops-- && sw.Parent(tree, v) != SchnyderWood::kNone)
    v = sw.Parent(tree, v);
  return (hops == static_cast<size_t>(-1)) ? SchnyderWood::kNone : v;
}

// Verifies the Schnyder wood sw is structurally valid for graph g:
//   1. Each tree's own root has kNone as its own-tree parent.
//   2. Every non-root vertex has a valid (non-kNone) parent in every tree.
//   3. Every assigned parent is an actual neighbor in g.
//   4. Following any chain in tree i eventually reaches root i.
static void VerifySchnyderWood(const SchnyderWood &sw, const Graph &g) {
  for (size_t c = 0; c < 3; c++)
    TEST_ASSERT_EQUAL_UINT64(SchnyderWood::kNone, sw.Parent(c, sw.Root(c)));

  for (size_t v = 0; v < sw.Size(); v++) {
    bool isOuter = (v == sw.Root(0) || v == sw.Root(1) || v == sw.Root(2));
    for (size_t c = 0; c < 3; c++) {
      size_t p = sw.Parent(c, v);
      if (p == SchnyderWood::kNone)
        continue;

      TEST_ASSERT_TRUE(g.EdgeExists(v, p));

      size_t reached = FollowToRoot(sw, c, v);
      TEST_ASSERT_EQUAL_UINT64(sw.Root(c), reached);
    }
    if (!isOuter) {
      for (size_t c = 0; c < 3; c++)
        TEST_ASSERT_NOT_EQUAL(SchnyderWood::kNone, sw.Parent(c, v));
    }
  }
}

static void test_schnyderWood_K4(void) {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();

  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 3, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(2, 3, 1.0);

  g.BuildLayout();
  Planar p(g);

  SchnyderWood sw(g);
  TEST_ASSERT_EQUAL_UINT64(4, sw.Size());

  VerifySchnyderWood(sw, g);
}

static void test_schnyderWood_hexagonAfterTriangulation(void) {
  Graph g(false);
  for (int i = 0; i < 6; i++)
    g.AddVertex();

  g.AddEdge(0, 1, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.AddEdge(3, 4, 1.0);
  g.AddEdge(4, 5, 1.0);
  g.AddEdge(5, 0, 1.0);
  g.AddEdge(5, 3, 1.0);

  g.BuildLayout();
  Planar p(g);

  (void)p;
  Subgraph sg = Subgraph::CreateFull(g);
  FaceEnumerator faces(g, sg);
  Triangulate(g, sg, faces);

  SchnyderWood sw(g);
  TEST_ASSERT_EQUAL_UINT64(6, sw.Size());

  VerifySchnyderWood(sw, g);

  sw.Embed(p);

  for (size_t i = 0; i < sw.Size(); i++) {
    const double *pos = p.GetVPosition(i);
    TEST_ASSERT_TRUE(std::isfinite(pos[0]));
    TEST_ASSERT_TRUE(std::isfinite(pos[1]));
  }
}

static void test_schnyderWood_embedRejectsNon2D(void) {
  Graph g(false);
  for (int i = 0; i < 4; i++)
    g.AddVertex();
  g.AddEdge(0, 1, 1.0);
  g.AddEdge(0, 2, 1.0);
  g.AddEdge(0, 3, 1.0);
  g.AddEdge(1, 2, 1.0);
  g.AddEdge(1, 3, 1.0);
  g.AddEdge(2, 3, 1.0);
  g.BuildLayout();

  // SchnyderWood requires a graph that already carries a valid CCW rotation
  // system (see its class doc) -- installing edges in raw insertion order,
  // as above, does not guarantee one, so a Planar embedder must run first,
  // exactly like every other SchnyderWood test in this file.
  Planar p(g);
  (void)p;

  // A bare (non-planar-embedder) 3D EmbeddedGraph over the same, now
  // rotation-installed graph, to exercise Embed()'s dimension guard -- the
  // old C had no such check and would silently over-read its 3-element
  // stack buffer here instead.
  EmbeddedGraph eg3d(g.Size(), 3);

  SchnyderWood sw(g);

  bool threw = false;
  try {
    sw.Embed(eg3d);
  } catch (const gviz::DimensionError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_schnyderWood_tooFewVerticesThrows(void) {
  Graph g(false);
  g.AddVertex();
  g.AddVertex();

  bool threw = false;
  try {
    SchnyderWood sw(g);
    (void)sw;
  } catch (const gviz::LayoutError &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_schnyderWood_K4);
  RUN_TEST(test_schnyderWood_hexagonAfterTriangulation);
  RUN_TEST(test_schnyderWood_embedRejectsNon2D);
  RUN_TEST(test_schnyderWood_tooFewVerticesThrows);

  return UNITY_END();
}
