#include "unity/unity.h"
#include "ds/gvizGraph.h"
#include "utils/graphLoader.h"
#include "utils/graphs.h"

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

void test_loadFromEdgesFile_sample(void) {
  gvizGraph g;
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);

  TEST_ASSERT_EQUAL_INT(
      0, gvizGraphLoadFromEdgesFile(GVIZ_SAMPLE_EDGES_PATH, &opts, &g));
  TEST_ASSERT_EQUAL_UINT64(4, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(4, gvizGraphEdgeCount(&g));

  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 2, 3));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 0, 3));

  gvizGraphRelease(&g);
}

void test_loadFromEdgesFile_missingFile(void) {
  gvizGraph g;
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);

  TEST_ASSERT_EQUAL_INT(
      -1, gvizGraphLoadFromEdgesFile(GVIZ_MISSING_EDGES_PATH, &opts, &g));
}

void test_loadFromEdgesFile_malformedLine(void) {
  gvizGraph g;
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);

  TEST_ASSERT_EQUAL_INT(
      -1, gvizGraphLoadFromEdgesFile(GVIZ_MALFORMED_EDGES_PATH, &opts, &g));
}

void test_loadFromEdgesFile_directed(void) {
  gvizGraph g;
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);
  opts.directed = 1;

  TEST_ASSERT_EQUAL_INT(
      0, gvizGraphLoadFromEdgesFile(GVIZ_DIRECTED_EDGES_PATH, &opts, &g));
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphSize(&g));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphIsDirected(&g));

  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 1, 0));

  gvizGraphRelease(&g);
}

void test_loadFromEdgesFile_idsWithGaps(void) {
  gvizGraph g;
  gvizEdgesFileOptions opts;
  gvizEdgesFileOptionsInit(&opts);

  TEST_ASSERT_EQUAL_INT(
      0, gvizGraphLoadFromEdgesFile(GVIZ_GAPPED_EDGES_PATH, &opts, &g));
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphSize(&g));

  gvizGraphRelease(&g);
}

void test_loadFromGexfFile_tiny(void) {
  gvizGraph g;

  TEST_ASSERT_EQUAL_INT(0, gvizGraphLoadFromGexfFile(GVIZ_TINY_GEXF_PATH, &g));
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(2, gvizGraphEdgeCount(&g));

  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 1, 0));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 1, 2));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 2, 1));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 0, 2));

  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"A\"\n}",
                           (const char *)gvizGraphGetVertexData(&g, 0));
  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"B\"\n}",
                           (const char *)gvizGraphGetVertexData(&g, 1));
  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"C\"\n}",
                           (const char *)gvizGraphGetVertexData(&g, 2));

  gvizGraphFreeVertexDataStrings(&g);
  gvizGraphRelease(&g);
}

void test_loadFromGexfFile_attributes(void) {
  gvizGraph g;

  TEST_ASSERT_EQUAL_INT(0, gvizGraphLoadFromGexfFile(GVIZ_ATTRS_GEXF_PATH, &g));
  TEST_ASSERT_EQUAL_UINT64(2, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(1, gvizGraphEdgeCount(&g));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));

  TEST_ASSERT_EQUAL_STRING(
      "{\n"
      "  \"label\": \"Alpha\",\n"
      "  \"city\": \"Gotham\",\n"
      "  \"population\": 42,\n"
      "  \"rating\": 3.5,\n"
      "  \"active\": true\n"
      "}",
      (const char *)gvizGraphGetVertexData(&g, 0));
  TEST_ASSERT_EQUAL_STRING("{\n  \"label\": \"Beta\"\n}",
                           (const char *)gvizGraphGetVertexData(&g, 1));

  gvizGraphFreeVertexDataStrings(&g);
  gvizGraphRelease(&g);
}

void test_loadFromGexfFile_legacyAttvalueId(void) {
  gvizGraph g;

  TEST_ASSERT_EQUAL_INT(
      0, gvizGraphLoadFromGexfFile(GVIZ_ATTRS_LEGACY_GEXF_PATH, &g));
  TEST_ASSERT_EQUAL_UINT64(2, gvizGraphSize(&g));

  TEST_ASSERT_EQUAL_STRING("{\n"
                           "  \"label\": \"Alpha\",\n"
                           "  \"0\": \"Alpha\",\n"
                           "  \"1\": 1.5\n"
                           "}",
                           (const char *)gvizGraphGetVertexData(&g, 0));

  gvizGraphFreeVertexDataStrings(&g);
  gvizGraphRelease(&g);
}

void test_loadFromGexfFile_zeroNodes(void) {
  gvizGraph g;
  TEST_ASSERT_EQUAL_INT(
      -1, gvizGraphLoadFromGexfFile(GVIZ_EMPTY_NODES_GEXF_PATH, &g));
}

void test_loadFromGexfFile_missingFile(void) {
  gvizGraph g;
  TEST_ASSERT_EQUAL_INT(-1,
                        gvizGraphLoadFromGexfFile(GVIZ_MISSING_GEXF_PATH, &g));
}

void test_loadFromObjFile_quad(void) {
  gvizGraph g;

  TEST_ASSERT_EQUAL_INT(0, gvizGraphLoadFromObjFile(GVIZ_TINY_OBJ_PATH, &g));
  TEST_ASSERT_EQUAL_UINT64(4, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(4, gvizGraphEdgeCount(&g));

  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 1, 2));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 2, 3));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 3, 0));
  TEST_ASSERT_EQUAL_INT(0, gvizGraphEdgeExists(&g, 0, 2));

  gvizGraphRelease(&g);
}

void test_loadFromObjFile_vtnStyleTokens(void) {
  gvizGraph g;

  TEST_ASSERT_EQUAL_INT(0,
                        gvizGraphLoadFromObjFile(GVIZ_TINY_VTN_OBJ_PATH, &g));
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphSize(&g));

  gvizGraphBuildLayout(&g);
  TEST_ASSERT_EQUAL_UINT64(3, gvizGraphEdgeCount(&g));

  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 0, 1));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 1, 2));
  TEST_ASSERT_EQUAL_INT(1, gvizGraphEdgeExists(&g, 2, 0));

  gvizGraphRelease(&g);
}

void test_loadFromObjFile_outOfRangeIndex(void) {
  gvizGraph g;
  TEST_ASSERT_EQUAL_INT(-1, gvizGraphLoadFromObjFile(GVIZ_BAD_OBJ_PATH, &g));
}

void test_loadFromObjFile_missingFile(void) {
  gvizGraph g;
  TEST_ASSERT_EQUAL_INT(-1,
                        gvizGraphLoadFromObjFile(GVIZ_MISSING_OBJ_PATH, &g));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_loadFromEdgesFile_sample);
  RUN_TEST(test_loadFromEdgesFile_missingFile);
  RUN_TEST(test_loadFromEdgesFile_malformedLine);
  RUN_TEST(test_loadFromEdgesFile_directed);
  RUN_TEST(test_loadFromEdgesFile_idsWithGaps);
  RUN_TEST(test_loadFromGexfFile_tiny);
  RUN_TEST(test_loadFromGexfFile_attributes);
  RUN_TEST(test_loadFromGexfFile_legacyAttvalueId);
  RUN_TEST(test_loadFromGexfFile_zeroNodes);
  RUN_TEST(test_loadFromGexfFile_missingFile);
  RUN_TEST(test_loadFromObjFile_quad);
  RUN_TEST(test_loadFromObjFile_vtnStyleTokens);
  RUN_TEST(test_loadFromObjFile_outOfRangeIndex);
  RUN_TEST(test_loadFromObjFile_missingFile);
  return UNITY_END();
}
