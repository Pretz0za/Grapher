#include "dsa/gvizBitArray.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"

void setUp() {}

void tearDown() {}

void test_gvizBitArray_Basic(void) {
  GVIZ_BIT_UNIT bit_array[GVIZ_ARRAY_UNITS(100)] = {};
  TEST_ASSERT_EQUAL_UINT64(104 / 8,
                           sizeof(bit_array)); // Closest multiple of 8 to 100.
  //
  printf("%d\n", (int)bit_array[0]);
  gvizSetBit(bit_array, 0);
  printf("%d\n", (int)bit_array[0]);
  TEST_ASSERT_EQUAL_UINT8(0x01, bit_array[0]);
  gvizSetBit(bit_array, 7);
  printf("%d\n", (int)bit_array[0]);
  TEST_ASSERT_EQUAL_UINT8(129, bit_array[0]);
}

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_gvizBitArray_Basic);

  UNITY_END();
}
