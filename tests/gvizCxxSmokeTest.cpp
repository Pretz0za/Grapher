#include "gviz.hpp"

#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_LayoutError_hierarchy_catches_as_base(void) {
  bool caught = false;
  try {
    throw gviz::NotPlanarError();
  } catch (const gviz::LayoutError &e) {
    caught = true;
    TEST_ASSERT_EQUAL_STRING("graph is not planar", e.what());
  }
  TEST_ASSERT_TRUE(caught);
}

static void test_DimensionError_message(void) {
  try {
    throw gviz::DimensionError("dimension must be 2, 3, or 4");
  } catch (const gviz::LayoutError &e) {
    TEST_ASSERT_EQUAL_STRING("dimension must be 2, 3, or 4", e.what());
    return;
  }
  TEST_FAIL_MESSAGE("expected DimensionError to be caught");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_LayoutError_hierarchy_catches_as_base);
  RUN_TEST(test_DimensionError_message);
  return UNITY_END();
}
