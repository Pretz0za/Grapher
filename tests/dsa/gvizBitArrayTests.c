#include "dsa/gvizBitArray.h"
#include "unity/unity.h"
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

// ============================================================================
// BASIC SET & TEST
// ============================================================================

void test_setBit_single(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  TEST_ASSERT_TRUE(gvizTestBit(arr, 0));
}

void test_setBit_multiple_same_unit(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  gvizSetBit(arr, 1);
  gvizSetBit(arr, 3);

  TEST_ASSERT_TRUE(gvizTestBit(arr, 0));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 1));
  TEST_ASSERT_FALSE(gvizTestBit(arr, 2));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 3));
  TEST_ASSERT_FALSE(gvizTestBit(arr, 4));
}

void test_setBit_crosses_units(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(20)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 7);
  gvizSetBit(arr, 8);
  gvizSetBit(arr, 15);
  gvizSetBit(arr, 16);

  TEST_ASSERT_TRUE(gvizTestBit(arr, 7));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 8));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 15));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 16));
}

void test_testBit_unset(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  TEST_ASSERT_FALSE(gvizTestBit(arr, 0));
  TEST_ASSERT_FALSE(gvizTestBit(arr, 5));
  TEST_ASSERT_FALSE(gvizTestBit(arr, 7));
}

void test_testBit_returns_nonzero(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 3);
  int result = gvizTestBit(arr, 3);
  TEST_ASSERT_NOT_EQUAL(0, result);
}

// ============================================================================
// CLEAR BIT
// ============================================================================

void test_clearBit_single(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 2);
  TEST_ASSERT_TRUE(gvizTestBit(arr, 2));

  gvizClearBit(arr, 2);
  TEST_ASSERT_FALSE(gvizTestBit(arr, 2));
}

void test_clearBit_preserves_others(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 1);
  gvizSetBit(arr, 2);
  gvizSetBit(arr, 3);

  gvizClearBit(arr, 2);

  TEST_ASSERT_TRUE(gvizTestBit(arr, 1));
  TEST_ASSERT_FALSE(gvizTestBit(arr, 2));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 3));
}

void test_clearBit_unset_bit(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  gvizClearBit(arr, 4);
  TEST_ASSERT_FALSE(gvizTestBit(arr, 4));
}

void test_clearBit_crosses_units(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(20)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 7);
  gvizSetBit(arr, 8);
  gvizSetBit(arr, 9);

  gvizClearBit(arr, 8);

  TEST_ASSERT_TRUE(gvizTestBit(arr, 7));
  TEST_ASSERT_FALSE(gvizTestBit(arr, 8));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 9));
}

// ============================================================================
// SET & CLEAR PATTERNS
// ============================================================================

void test_all_bits_in_unit(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_UNIT)];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < GVIZ_BITS_PER_UNIT; i++) {
    gvizSetBit(arr, i);
  }

  for (int i = 0; i < GVIZ_BITS_PER_UNIT; i++) {
    TEST_ASSERT_TRUE(gvizTestBit(arr, i));
  }
}

void test_alternating_bits(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(16)];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < 16; i += 2) {
    gvizSetBit(arr, i);
  }

  for (int i = 0; i < 16; i++) {
    if (i % 2 == 0) {
      TEST_ASSERT_TRUE(gvizTestBit(arr, i));
    } else {
      TEST_ASSERT_FALSE(gvizTestBit(arr, i));
    }
  }
}

void test_set_then_clear_all(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(16)];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < 16; i++) {
    gvizSetBit(arr, i);
  }

  for (int i = 0; i < 16; i++) {
    gvizClearBit(arr, i);
  }

  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_FALSE(gvizTestBit(arr, i));
  }
}

void test_toggle_pattern(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < 8; i++) {
    gvizSetBit(arr, i);
  }

  for (int i = 0; i < 8; i += 2) {
    gvizClearBit(arr, i);
  }

  for (int i = 0; i < 8; i++) {
    if (i % 2 == 1) {
      TEST_ASSERT_TRUE(gvizTestBit(arr, i));
    } else {
      TEST_ASSERT_FALSE(gvizTestBit(arr, i));
    }
  }
}

// ============================================================================
// BOUNDARY CONDITIONS
// ============================================================================

void test_single_bit_array(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(1)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  TEST_ASSERT_TRUE(gvizTestBit(arr, 0));

  gvizClearBit(arr, 0);
  TEST_ASSERT_FALSE(gvizTestBit(arr, 0));
}

void test_boundary_between_units(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_UNIT + 1)];
  memset(arr, 0, sizeof(arr));

  int last_bit_unit_0 = GVIZ_BITS_PER_UNIT - 1;
  int first_bit_unit_1 = GVIZ_BITS_PER_UNIT;

  gvizSetBit(arr, last_bit_unit_0);
  gvizSetBit(arr, first_bit_unit_1);

  TEST_ASSERT_TRUE(gvizTestBit(arr, last_bit_unit_0));
  TEST_ASSERT_TRUE(gvizTestBit(arr, first_bit_unit_1));
}

void test_large_bit_index(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(256)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 255);
  TEST_ASSERT_TRUE(gvizTestBit(arr, 255));

  gvizClearBit(arr, 255);
  TEST_ASSERT_FALSE(gvizTestBit(arr, 255));
}

void test_multiple_units_boundary(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(3 * GVIZ_BITS_PER_UNIT)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, GVIZ_BITS_PER_UNIT - 1);
  gvizSetBit(arr, GVIZ_BITS_PER_UNIT);
  gvizSetBit(arr, 2 * GVIZ_BITS_PER_UNIT - 1);
  gvizSetBit(arr, 2 * GVIZ_BITS_PER_UNIT);

  TEST_ASSERT_TRUE(gvizTestBit(arr, GVIZ_BITS_PER_UNIT - 1));
  TEST_ASSERT_TRUE(gvizTestBit(arr, GVIZ_BITS_PER_UNIT));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 2 * GVIZ_BITS_PER_UNIT - 1));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 2 * GVIZ_BITS_PER_UNIT));
}

// ============================================================================
// ARRAY UNITS CALCULATION
// ============================================================================

void test_array_units_exact_fit(void) {
  size_t units = GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_UNIT);
  TEST_ASSERT_EQUAL(1, units);

  units = GVIZ_ARRAY_UNITS(2 * GVIZ_BITS_PER_UNIT);
  TEST_ASSERT_EQUAL(2, units);
}

void test_array_units_partial(void) {
  size_t units = GVIZ_ARRAY_UNITS(1);
  TEST_ASSERT_EQUAL(1, units);

  units = GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_UNIT - 1);
  TEST_ASSERT_EQUAL(1, units);

  units = GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_UNIT + 1);
  TEST_ASSERT_EQUAL(2, units);
}

void test_array_units_zero(void) {
  size_t units = GVIZ_ARRAY_UNITS(0);
  TEST_ASSERT_EQUAL(0, units);
}

// ============================================================================
// MEMORY ISOLATION
// ============================================================================

void test_no_overflow_single_unit(void) {
  GVIZ_BIT_UNIT arr[1];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < GVIZ_BITS_PER_UNIT; i++) {
    gvizSetBit(arr, i);
  }

  TEST_ASSERT_EQUAL(0xFF, arr[0]);
}

void test_isolation_between_units(void) {
  GVIZ_BIT_UNIT arr[2];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, GVIZ_BITS_PER_UNIT - 1);
  gvizSetBit(arr, GVIZ_BITS_PER_UNIT);

  TEST_ASSERT_EQUAL(0x80, arr[0]);
  TEST_ASSERT_EQUAL(0x01, arr[1]);
}

void test_clear_isolation(void) {
  GVIZ_BIT_UNIT arr[2];
  memset(arr, 0xFF, sizeof(arr));

  gvizClearBit(arr, 4);

  TEST_ASSERT_EQUAL(0xEF, arr[0]);
  TEST_ASSERT_EQUAL(0xFF, arr[1]);
}

void test_independent_units(void) {
  GVIZ_BIT_UNIT arr[3];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  gvizSetBit(arr, GVIZ_BITS_PER_UNIT + 3);
  gvizSetBit(arr, 2 * GVIZ_BITS_PER_UNIT + 7);

  TEST_ASSERT_EQUAL(0x01, arr[0]);
  TEST_ASSERT_EQUAL(0x08, arr[1]);
  TEST_ASSERT_EQUAL(0x80, arr[2]);
}

// ============================================================================
// LSB ORDERING VERIFICATION
// ============================================================================

void test_lsb_is_zero(void) {
  GVIZ_BIT_UNIT arr[1];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  TEST_ASSERT_EQUAL(0x01, arr[0]);
}

void test_msb_is_last(void) {
  GVIZ_BIT_UNIT arr[1];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, GVIZ_BITS_PER_UNIT - 1);
  TEST_ASSERT_EQUAL(0x80, arr[0]);
}

void test_bit_order(void) {
  GVIZ_BIT_UNIT arr[1];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < GVIZ_BITS_PER_UNIT; i++) {
    gvizSetBit(arr, i);
    TEST_ASSERT_EQUAL(1 << i, arr[0]);
    gvizClearBit(arr, i);
  }
}

// ============================================================================
// STRESS TESTS
// ============================================================================

void test_sparse_large_array(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(1000)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  gvizSetBit(arr, 500);
  gvizSetBit(arr, 999);

  TEST_ASSERT_TRUE(gvizTestBit(arr, 0));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 500));
  TEST_ASSERT_TRUE(gvizTestBit(arr, 999));

  for (int i = 1; i < 500; i++) {
    if (i != 500) {
      TEST_ASSERT_FALSE(gvizTestBit(arr, i));
    }
  }
}

void test_dense_large_array(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(512)];
  memset(arr, 0, sizeof(arr));

  for (int i = 0; i < 512; i += 3) {
    gvizSetBit(arr, i);
  }

  for (int i = 0; i < 512; i++) {
    if (i % 3 == 0) {
      TEST_ASSERT_TRUE(gvizTestBit(arr, i));
    } else {
      TEST_ASSERT_FALSE(gvizTestBit(arr, i));
    }
  }
}

void test_random_access_pattern(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(256)];
  memset(arr, 0, sizeof(arr));

  int indices[] = {7, 42, 15, 123, 200, 99, 1, 255, 64, 128};
  int count = sizeof(indices) / sizeof(indices[0]);

  for (int i = 0; i < count; i++) {
    gvizSetBit(arr, indices[i]);
  }

  for (int i = 0; i < count; i++) {
    TEST_ASSERT_TRUE(gvizTestBit(arr, indices[i]));
  }

  gvizClearBit(arr, indices[0]);
  TEST_ASSERT_FALSE(gvizTestBit(arr, indices[0]));

  for (int i = 1; i < count; i++) {
    TEST_ASSERT_TRUE(gvizTestBit(arr, indices[i]));
  }
}

// ============================================================================
// ALLOC / FREE / CLONE / COUNT / CLEAR_ALL
// ============================================================================

void test_alloc_zero_returns_null(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(0);
  TEST_ASSERT_NULL(arr);
}

void test_alloc_zeroed(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(64);
  TEST_ASSERT_NOT_NULL(arr);
  for (size_t i = 0; i < 64; i++) {
    TEST_ASSERT_FALSE(gvizTestBit(arr, i));
  }
  gvizBitArrayFree(arr);
}

void test_free_null_safe(void) {
  gvizBitArrayFree(NULL);
}

void test_clone_basic(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(20);
  gvizSetBit(arr, 1);
  gvizSetBit(arr, 7);
  gvizSetBit(arr, 15);

  GVIZ_BIT_ARRAY copy = gvizBitArrayClone(arr, 20);
  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_TRUE(gvizTestBit(copy, 1));
  TEST_ASSERT_TRUE(gvizTestBit(copy, 7));
  TEST_ASSERT_TRUE(gvizTestBit(copy, 15));
  TEST_ASSERT_FALSE(gvizTestBit(copy, 0));

  gvizSetBit(copy, 0);
  TEST_ASSERT_FALSE(gvizTestBit(arr, 0));

  gvizBitArrayFree(arr);
  gvizBitArrayFree(copy);
}

void test_clone_null_returns_null(void) {
  TEST_ASSERT_NULL(gvizBitArrayClone(NULL, 16));
}

void test_count_empty(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(64);
  TEST_ASSERT_EQUAL(0, gvizBitArrayCount(arr, 64));
  gvizBitArrayFree(arr);
}

void test_count_basic(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(64);
  gvizSetBit(arr, 0);
  gvizSetBit(arr, 17);
  gvizSetBit(arr, 31);
  gvizSetBit(arr, 63);
  TEST_ASSERT_EQUAL(4, gvizBitArrayCount(arr, 64));
  gvizBitArrayFree(arr);
}

void test_count_respects_nbits(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(16);
  for (size_t i = 0; i < 16; i++) {
    gvizSetBit(arr, i);
  }
  TEST_ASSERT_EQUAL(13, gvizBitArrayCount(arr, 13));
  TEST_ASSERT_EQUAL(16, gvizBitArrayCount(arr, 16));
  gvizBitArrayFree(arr);
}

void test_clear_all(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(40);
  for (size_t i = 0; i < 40; i++) {
    gvizSetBit(arr, i);
  }
  gvizBitArrayClearAll(arr, 40);
  TEST_ASSERT_EQUAL(0, gvizBitArrayCount(arr, 40));
  gvizBitArrayFree(arr);
}

// ============================================================================
// ITERATOR
// ============================================================================

void test_iter_empty(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(32);
  gvizBitArrayIter it;
  gvizBitArrayIterInit(&it, arr, 32);
  size_t out = 0;
  TEST_ASSERT_EQUAL(0, gvizBitArrayIterNext(&it, &out));
  gvizBitArrayIterRelease(&it);
  gvizBitArrayFree(arr);
}

void test_iter_sparse(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(128);
  gvizSetBit(arr, 3);
  gvizSetBit(arr, 70);
  gvizSetBit(arr, 127);

  gvizBitArrayIter it;
  gvizBitArrayIterInit(&it, arr, 128);

  size_t out = 0;
  TEST_ASSERT_EQUAL(1, gvizBitArrayIterNext(&it, &out));
  TEST_ASSERT_EQUAL(3, out);
  TEST_ASSERT_EQUAL(1, gvizBitArrayIterNext(&it, &out));
  TEST_ASSERT_EQUAL(70, out);
  TEST_ASSERT_EQUAL(1, gvizBitArrayIterNext(&it, &out));
  TEST_ASSERT_EQUAL(127, out);
  TEST_ASSERT_EQUAL(0, gvizBitArrayIterNext(&it, &out));

  gvizBitArrayIterRelease(&it);
  gvizBitArrayFree(arr);
}

void test_iter_dense_in_order(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(40);
  for (size_t i = 0; i < 40; i++) {
    gvizSetBit(arr, i);
  }
  gvizBitArrayIter it;
  gvizBitArrayIterInit(&it, arr, 40);

  size_t expected = 0;
  size_t out = 0;
  while (gvizBitArrayIterNext(&it, &out)) {
    TEST_ASSERT_EQUAL(expected, out);
    expected++;
  }
  TEST_ASSERT_EQUAL(40, expected);

  gvizBitArrayIterRelease(&it);
  gvizBitArrayFree(arr);
}

void test_iter_off_the_end_nbits(void) {
  // nbits not a multiple of 8 — bits past nbits must never appear.
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(16);
  // tamper with the storage past nbits=13 to ensure iter respects nbits
  gvizSetBit(arr, 12);
  gvizSetBit(arr, 13);
  gvizSetBit(arr, 14);
  gvizSetBit(arr, 15);

  gvizBitArrayIter it;
  gvizBitArrayIterInit(&it, arr, 13);
  size_t out = 0;
  TEST_ASSERT_EQUAL(1, gvizBitArrayIterNext(&it, &out));
  TEST_ASSERT_EQUAL(12, out);
  TEST_ASSERT_EQUAL(0, gvizBitArrayIterNext(&it, &out));
  gvizBitArrayIterRelease(&it);
  gvizBitArrayFree(arr);
}

void test_iter_does_not_mutate_source(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(64);
  gvizSetBit(arr, 1);
  gvizSetBit(arr, 33);
  gvizSetBit(arr, 60);

  gvizBitArrayIter it1;
  gvizBitArrayIterInit(&it1, arr, 64);
  size_t out;
  while (gvizBitArrayIterNext(&it1, &out)) {
  }
  gvizBitArrayIterRelease(&it1);

  // source should be unchanged — second iteration sees the same bits
  gvizBitArrayIter it2;
  gvizBitArrayIterInit(&it2, arr, 64);
  size_t expected[] = {1, 33, 60};
  for (int i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL(1, gvizBitArrayIterNext(&it2, &out));
    TEST_ASSERT_EQUAL(expected[i], out);
  }
  TEST_ASSERT_EQUAL(0, gvizBitArrayIterNext(&it2, &out));
  gvizBitArrayIterRelease(&it2);
  gvizBitArrayFree(arr);
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_setBit_single);
  RUN_TEST(test_setBit_multiple_same_unit);
  RUN_TEST(test_setBit_crosses_units);
  RUN_TEST(test_testBit_unset);
  RUN_TEST(test_testBit_returns_nonzero);

  RUN_TEST(test_clearBit_single);
  RUN_TEST(test_clearBit_preserves_others);
  RUN_TEST(test_clearBit_unset_bit);
  RUN_TEST(test_clearBit_crosses_units);

  RUN_TEST(test_all_bits_in_unit);
  RUN_TEST(test_alternating_bits);
  RUN_TEST(test_set_then_clear_all);
  RUN_TEST(test_toggle_pattern);

  RUN_TEST(test_single_bit_array);
  RUN_TEST(test_boundary_between_units);
  RUN_TEST(test_large_bit_index);
  RUN_TEST(test_multiple_units_boundary);

  RUN_TEST(test_array_units_exact_fit);
  RUN_TEST(test_array_units_partial);
  RUN_TEST(test_array_units_zero);

  RUN_TEST(test_no_overflow_single_unit);
  RUN_TEST(test_isolation_between_units);
  RUN_TEST(test_clear_isolation);
  RUN_TEST(test_independent_units);

  RUN_TEST(test_lsb_is_zero);
  RUN_TEST(test_msb_is_last);
  RUN_TEST(test_bit_order);

  RUN_TEST(test_sparse_large_array);
  RUN_TEST(test_dense_large_array);
  RUN_TEST(test_random_access_pattern);

  RUN_TEST(test_alloc_zero_returns_null);
  RUN_TEST(test_alloc_zeroed);
  RUN_TEST(test_free_null_safe);
  RUN_TEST(test_clone_basic);
  RUN_TEST(test_clone_null_returns_null);
  RUN_TEST(test_count_empty);
  RUN_TEST(test_count_basic);
  RUN_TEST(test_count_respects_nbits);
  RUN_TEST(test_clear_all);

  RUN_TEST(test_iter_empty);
  RUN_TEST(test_iter_sparse);
  RUN_TEST(test_iter_dense_in_order);
  RUN_TEST(test_iter_off_the_end_nbits);
  RUN_TEST(test_iter_does_not_mutate_source);

  return UNITY_END();
}
