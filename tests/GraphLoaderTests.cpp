// Port of tests/utils/gvizGraphLoaderTests.c against gviz::io::GraphLoader.
//
// Scenarios reshaped from the old C suite:
//   - Every "-1 on failure" assertion becomes a try/catch around the
//     throwing call (matching the try/catch-and-flag pattern already used
//     by GRIPTests.cpp/PlanarTests.cpp for this port), since these loaders
//     throw std::runtime_error instead of returning a status code -- see
//     GraphLoader.hpp's doc comments on why std::runtime_error (a plain
//     I/O/parse failure) rather than gviz::LayoutError (reserved for
//     graph-structural/dimensional layout failures) was chosen.
//   - gvizGraphGetVertexData's raw `const char *` comparisons become
//     dereferences of the `std::string *` LoadFromGexfFile stores (see
//     FreeVertexDataStrings's doc comment on why it's `std::string*`, not
//     `char*`, for this port).
//   - No manual gvizGraph out-param + gvizGraphRelease: Graph is returned by
//     value and cleans itself up via RAII: no successful-load test below
//     needs an explicit "release" step.

#include "GraphLoader.hpp"

#include "Graph.hpp"
#include "unity/unity.h"

#include <stdexcept>
#include <string>

using gviz::Graph;

namespace io = gviz::io;

#define GVIZ_SAMPLE_EDGES_PATH GVIZ_TEST_DATA_DIR "/sample.edges"
#define GVIZ_MALFORMED_EDGES_PATH GVIZ_TEST_DATA_DIR "/malformed.edges"
#define GVIZ_DIRECTED_EDGES_PATH GVIZ_TEST_DATA_DIR "/directed.edges"
#define GVIZ_GAPPED_EDGES_PATH GVIZ_TEST_DATA_DIR "/gapped.edges"
#define GVIZ_MISSING_EDGES_PATH GVIZ_TEST_DATA_DIR "/missing.edges"
#define GVIZ_TINY_GEXF_PATH GVIZ_TEST_DATA_DIR "/tiny.gexf"
#define GVIZ_EMPTY_NODES_GEXF_PATH GVIZ_TEST_DATA_DIR "/empty_nodes.gexf"
#define GVIZ_MISSING_GEXF_PATH GVIZ_TEST_DATA_DIR "/missing.gexf"
#define GVIZ_ATTRS_GEXF_PATH GVIZ_TEST_DATA_DIR "/attrs.gexf"
#define GVIZ_ATTRS_LEGACY_GEXF_PATH GVIZ_TEST_DATA_DIR "/attrs_legacy.gexf"
#define GVIZ_TINY_OBJ_PATH GVIZ_TEST_DATA_DIR "/tiny.obj"
#define GVIZ_TINY_VTN_OBJ_PATH GVIZ_TEST_DATA_DIR "/tiny_vtn.obj"
#define GVIZ_BAD_OBJ_PATH GVIZ_TEST_DATA_DIR "/bad.obj"
#define GVIZ_MISSING_OBJ_PATH GVIZ_TEST_DATA_DIR "/missing.obj"

void setUp(void) {}
void tearDown(void) {}

// ============================================================================
// .edges
// ============================================================================

static void test_loadFromEdgesFile_sample(void) {
  Graph g = io::LoadFromEdgesFile(GVIZ_SAMPLE_EDGES_PATH);
  TEST_ASSERT_EQUAL_UINT64(4, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(4, g.EdgeCount());

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(2, 3));
  TEST_ASSERT_FALSE(g.EdgeExists(0, 3));
}

static void test_loadFromEdgesFile_missingFile(void) {
  bool threw = false;
  try {
    Graph g = io::LoadFromEdgesFile(GVIZ_MISSING_EDGES_PATH);
    (void)g;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_loadFromEdgesFile_malformedLine(void) {
  bool threw = false;
  try {
    Graph g = io::LoadFromEdgesFile(GVIZ_MALFORMED_EDGES_PATH);
    (void)g;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_loadFromEdgesFile_directed(void) {
  io::EdgesFileOptions opts;
  opts.directed = true;

  Graph g = io::LoadFromEdgesFile(GVIZ_DIRECTED_EDGES_PATH, opts);
  TEST_ASSERT_EQUAL_UINT64(3, g.Size());
  TEST_ASSERT_TRUE(g.IsDirected());

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(1, 0));
}

static void test_loadFromEdgesFile_idsWithGaps(void) {
  Graph g = io::LoadFromEdgesFile(GVIZ_GAPPED_EDGES_PATH);
  TEST_ASSERT_EQUAL_UINT64(3, g.Size());
}

static void test_loadFromEdgesFile_skipHeader(void) {
  // sample.edges has no numeric header line, but skipHeader must still
  // consume exactly the first data line -- verify against directed.edges
  // (2 data lines "1 2" / "2 3") that skipping drops only the first edge.
  io::EdgesFileOptions opts;
  opts.directed = true;
  opts.skipHeader = true;

  Graph g = io::LoadFromEdgesFile(GVIZ_DIRECTED_EDGES_PATH, opts);
  TEST_ASSERT_EQUAL_UINT64(2, g.Size());
  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
}

// ============================================================================
// .gexf
// ============================================================================

static void test_loadFromGexfFile_tiny(void) {
  Graph g = io::LoadFromGexfFile(GVIZ_TINY_GEXF_PATH, false);
  TEST_ASSERT_EQUAL_UINT64(3, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(2, g.EdgeCount());

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 0));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 2));
  TEST_ASSERT_TRUE(g.EdgeExists(2, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(0, 2));

  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"A\"\n}",
                            static_cast<std::string *>(g.GetVertexData(0))->c_str());
  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"B\"\n}",
                            static_cast<std::string *>(g.GetVertexData(1))->c_str());
  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"C\"\n}",
                            static_cast<std::string *>(g.GetVertexData(2))->c_str());

  io::FreeVertexDataStrings(g);
  TEST_ASSERT_NULL(g.GetVertexData(0));
}

static void test_loadFromGexfFile_directed(void) {
  Graph g = io::LoadFromGexfFile(GVIZ_TINY_GEXF_PATH, true);
  TEST_ASSERT_EQUAL_UINT64(3, g.Size());
  TEST_ASSERT_TRUE(g.IsDirected());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(2, g.EdgeCount());

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_FALSE(g.EdgeExists(1, 0));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 2));
  TEST_ASSERT_FALSE(g.EdgeExists(2, 1));

  io::FreeVertexDataStrings(g);
}

static void test_loadFromGexfFile_attributes(void) {
  Graph g = io::LoadFromGexfFile(GVIZ_ATTRS_GEXF_PATH, false);
  TEST_ASSERT_EQUAL_UINT64(2, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(1, g.EdgeCount());
  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));

  TEST_ASSERT_EQUAL_STRING("{\n"
                            "  \"label\": \"Alpha\",\n"
                            "  \"city\": \"Gotham\",\n"
                            "  \"population\": 42,\n"
                            "  \"rating\": 3.5,\n"
                            "  \"active\": true\n"
                            "}",
                            static_cast<std::string *>(g.GetVertexData(0))->c_str());
  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"Beta\"\n}",
                            static_cast<std::string *>(g.GetVertexData(1))->c_str());

  io::FreeVertexDataStrings(g);
}

static void test_loadFromGexfFile_legacyAttvalueId(void) {
  Graph g = io::LoadFromGexfFile(GVIZ_ATTRS_LEGACY_GEXF_PATH, false);
  TEST_ASSERT_EQUAL_UINT64(2, g.Size());

  TEST_ASSERT_EQUAL_STRING("{\n"
                            "  \"label\": \"Alpha\",\n"
                            "  \"0\": \"Alpha\",\n"
                            "  \"1\": 1.5\n"
                            "}",
                            static_cast<std::string *>(g.GetVertexData(0))->c_str());

  io::FreeVertexDataStrings(g);
}

static void test_loadFromGexfFile_zeroNodes(void) {
  bool threw = false;
  try {
    Graph g = io::LoadFromGexfFile(GVIZ_EMPTY_NODES_GEXF_PATH, false);
    (void)g;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_loadFromGexfFile_missingFile(void) {
  bool threw = false;
  try {
    Graph g = io::LoadFromGexfFile(GVIZ_MISSING_GEXF_PATH, false);
    (void)g;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_loadFromGexfFile_freeVertexDataStrings_leavesNonGexfDataAlone(void) {
  // FreeVertexDataStrings only knows how to delete a std::string*; a vertex
  // with a nullptr data pointer must be left untouched (no double-free, no
  // crash) -- this is the "vertices with a nullptr data pointer are left
  // untouched" contract from GraphLoader.hpp's doc comment.
  Graph g(false, 1);
  g.AddVertex(nullptr);
  io::FreeVertexDataStrings(g);
  TEST_ASSERT_NULL(g.GetVertexData(0));
}

// ============================================================================
// .obj
// ============================================================================

static void test_loadFromObjFile_quad(void) {
  Graph g = io::LoadFromObjFile(GVIZ_TINY_OBJ_PATH);
  TEST_ASSERT_EQUAL_UINT64(4, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(4, g.EdgeCount());

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 2));
  TEST_ASSERT_TRUE(g.EdgeExists(2, 3));
  TEST_ASSERT_TRUE(g.EdgeExists(3, 0));
  TEST_ASSERT_FALSE(g.EdgeExists(0, 2));
}

static void test_loadFromObjFile_vtnStyleTokens(void) {
  Graph g = io::LoadFromObjFile(GVIZ_TINY_VTN_OBJ_PATH);
  TEST_ASSERT_EQUAL_UINT64(3, g.Size());

  g.BuildLayout();
  TEST_ASSERT_EQUAL_UINT64(3, g.EdgeCount());

  TEST_ASSERT_TRUE(g.EdgeExists(0, 1));
  TEST_ASSERT_TRUE(g.EdgeExists(1, 2));
  TEST_ASSERT_TRUE(g.EdgeExists(2, 0));
}

static void test_loadFromObjFile_outOfRangeIndex(void) {
  bool threw = false;
  try {
    Graph g = io::LoadFromObjFile(GVIZ_BAD_OBJ_PATH);
    (void)g;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

static void test_loadFromObjFile_missingFile(void) {
  bool threw = false;
  try {
    Graph g = io::LoadFromObjFile(GVIZ_MISSING_OBJ_PATH);
    (void)g;
  } catch (const std::runtime_error &) {
    threw = true;
  }
  TEST_ASSERT_TRUE(threw);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_loadFromEdgesFile_sample);
  RUN_TEST(test_loadFromEdgesFile_missingFile);
  RUN_TEST(test_loadFromEdgesFile_malformedLine);
  RUN_TEST(test_loadFromEdgesFile_directed);
  RUN_TEST(test_loadFromEdgesFile_idsWithGaps);
  RUN_TEST(test_loadFromEdgesFile_skipHeader);
  RUN_TEST(test_loadFromGexfFile_tiny);
  RUN_TEST(test_loadFromGexfFile_directed);
  RUN_TEST(test_loadFromGexfFile_attributes);
  RUN_TEST(test_loadFromGexfFile_legacyAttvalueId);
  RUN_TEST(test_loadFromGexfFile_zeroNodes);
  RUN_TEST(test_loadFromGexfFile_missingFile);
  RUN_TEST(test_loadFromGexfFile_freeVertexDataStrings_leavesNonGexfDataAlone);
  RUN_TEST(test_loadFromObjFile_quad);
  RUN_TEST(test_loadFromObjFile_vtnStyleTokens);
  RUN_TEST(test_loadFromObjFile_outOfRangeIndex);
  RUN_TEST(test_loadFromObjFile_missingFile);
  return UNITY_END();
}
