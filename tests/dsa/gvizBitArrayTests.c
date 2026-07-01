#include "dsa/gvizBitArray.h"
#include "unity/unity.h"
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

static size_t collect_set_bits(GVIZ_BIT_ARRAY arr, size_t nbits, size_t *out,
                               size_t max_out) {
  gvizBitArrayIterator it = gvizBitArrayIteratorCreate(arr, nbits);
  size_t n = 0;
  size_t idx;

  while (gvizBitArrayIterate(&it, &idx) && n < max_out) {
    out[n++] = idx;
  }
  return n;
}

static void assert_bits_match_iterator(GVIZ_BIT_ARRAY arr, size_t nbits) {
  size_t collected[1024];
  size_t n = collect_set_bits(arr, nbits, collected, 1024);

  size_t expect = 0;
  for (size_t i = 0; i < nbits; i++) {
    if (gvizTestBit(arr, i)) {
      TEST_ASSERT_LESS_THAN(n, expect);
      TEST_ASSERT_EQUAL(i, collected[expect]);
      expect++;
    }
  }
  TEST_ASSERT_EQUAL(expect, n);
}

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
// BIT ARRAY OR / AND
// ============================================================================

void test_or_disjoint_bits(void) {
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(32)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(32)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  gvizSetBit(dest, 1);
  gvizSetBit(dest, 10);
  gvizSetBit(src, 5);
  gvizSetBit(src, 20);

  gvizBitArrayOr(dest, src, 32);

  TEST_ASSERT_TRUE(gvizTestBit(dest, 1));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 5));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 10));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 20));
  TEST_ASSERT_FALSE(gvizTestBit(dest, 0));
}

void test_or_overlapping_bits(void) {
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(16)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(16)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  gvizSetBit(dest, 3);
  gvizSetBit(dest, 7);
  gvizSetBit(src, 3);
  gvizSetBit(src, 9);

  gvizBitArrayOr(dest, src, 16);

  TEST_ASSERT_TRUE(gvizTestBit(dest, 3));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 7));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 9));
  TEST_ASSERT_FALSE(gvizTestBit(dest, 4));
}

void test_or_preserves_dest_only_bits(void) {
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(8)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(8)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  gvizSetBit(dest, 2);
  gvizBitArrayOr(dest, src, 8);

  TEST_ASSERT_TRUE(gvizTestBit(dest, 2));
}

void test_and_disjoint_clears_all(void) {
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(16)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(16)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  gvizSetBit(dest, 1);
  gvizSetBit(dest, 4);
  gvizSetBit(src, 8);

  gvizBitArrayAnd(dest, src, 16);

  TEST_ASSERT_FALSE(gvizTestBit(dest, 1));
  TEST_ASSERT_FALSE(gvizTestBit(dest, 4));
  TEST_ASSERT_FALSE(gvizTestBit(dest, 8));
}

void test_and_keeps_common_bits(void) {
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(32)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(32)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  gvizSetBit(dest, 2);
  gvizSetBit(dest, 5);
  gvizSetBit(dest, 11);
  gvizSetBit(src, 5);
  gvizSetBit(src, 11);
  gvizSetBit(src, 14);

  gvizBitArrayAnd(dest, src, 32);

  TEST_ASSERT_FALSE(gvizTestBit(dest, 2));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 5));
  TEST_ASSERT_TRUE(gvizTestBit(dest, 11));
  TEST_ASSERT_FALSE(gvizTestBit(dest, 14));
}

void test_or_and_cross_word_boundary(void) {
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_WORD + 8)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_WORD + 8)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  size_t last_in_word = GVIZ_BITS_PER_WORD - 1;
  size_t first_next = GVIZ_BITS_PER_WORD;

  gvizSetBit(dest, last_in_word);
  gvizSetBit(src, first_next);

  gvizBitArrayOr(dest, src, GVIZ_BITS_PER_WORD + 8);
  TEST_ASSERT_TRUE(gvizTestBit(dest, last_in_word));
  TEST_ASSERT_TRUE(gvizTestBit(dest, first_next));

  gvizSetBit(dest, first_next);
  gvizSetBit(src, last_in_word);
  gvizClearBit(src, first_next);
  gvizBitArrayAnd(dest, src, GVIZ_BITS_PER_WORD + 8);
  TEST_ASSERT_TRUE(gvizTestBit(dest, last_in_word));
  TEST_ASSERT_FALSE(gvizTestBit(dest, first_next));
}

void test_or_and_partial_last_word(void) {
  size_t nbits = GVIZ_BITS_PER_WORD + 3;
  GVIZ_BIT_UNIT dest[GVIZ_ARRAY_UNITS(nbits)];
  GVIZ_BIT_UNIT src[GVIZ_ARRAY_UNITS(nbits)];
  memset(dest, 0, sizeof(dest));
  memset(src, 0, sizeof(src));

  gvizSetBit(dest, nbits - 1);
  gvizSetBit(src, 0);

  gvizBitArrayOr(dest, src, nbits);
  TEST_ASSERT_TRUE(gvizTestBit(dest, 0));
  TEST_ASSERT_TRUE(gvizTestBit(dest, nbits - 1));

  gvizBitArrayAnd(dest, src, nbits);
  TEST_ASSERT_TRUE(gvizTestBit(dest, 0));
  TEST_ASSERT_FALSE(gvizTestBit(dest, nbits - 1));
}

// ============================================================================
// ITERATOR
// ============================================================================

void test_iterate_empty_array(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(16)];
  memset(arr, 0, sizeof(arr));

  gvizBitArrayIterator it = gvizBitArrayIteratorCreate(arr, 16);
  size_t idx;

  TEST_ASSERT_FALSE(gvizBitArrayIterate(&it, &idx));
}

void test_iterate_zero_nbits(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0xFF, sizeof(arr));

  gvizBitArrayIterator it = gvizBitArrayIteratorCreate(arr, 0);
  size_t idx;

  TEST_ASSERT_FALSE(gvizBitArrayIterate(&it, &idx));
}

void test_iterate_single_bit(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(64)];
  memset(arr, 0, sizeof(arr));
  gvizSetBit(arr, 17);

  size_t collected[4];
  size_t n = collect_set_bits(arr, 64, collected, 4);

  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL(17, collected[0]);
}

void test_iterate_multiple_bits_same_byte(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(16)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 2);
  gvizSetBit(arr, 5);
  gvizSetBit(arr, 7);

  size_t collected[8];
  size_t n = collect_set_bits(arr, 16, collected, 8);

  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(2, collected[0]);
  TEST_ASSERT_EQUAL(5, collected[1]);
  TEST_ASSERT_EQUAL(7, collected[2]);
}

void test_iterate_cross_byte_boundary(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(20)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 7);
  gvizSetBit(arr, 8);
  gvizSetBit(arr, 9);

  size_t collected[8];
  size_t n = collect_set_bits(arr, 20, collected, 8);

  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(7, collected[0]);
  TEST_ASSERT_EQUAL(8, collected[1]);
  TEST_ASSERT_EQUAL(9, collected[2]);
}

void test_iterate_cross_word_boundary(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(GVIZ_BITS_PER_WORD + 16)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, GVIZ_BITS_PER_WORD - 1);
  gvizSetBit(arr, GVIZ_BITS_PER_WORD);
  gvizSetBit(arr, GVIZ_BITS_PER_WORD + 5);

  size_t collected[8];
  size_t n = collect_set_bits(arr, GVIZ_BITS_PER_WORD + 16, collected, 8);

  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(GVIZ_BITS_PER_WORD - 1, collected[0]);
  TEST_ASSERT_EQUAL(GVIZ_BITS_PER_WORD, collected[1]);
  TEST_ASSERT_EQUAL(GVIZ_BITS_PER_WORD + 5, collected[2]);
}

void test_iterate_exhausted_returns_false(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(8)];
  memset(arr, 0, sizeof(arr));
  gvizSetBit(arr, 3);

  gvizBitArrayIterator it = gvizBitArrayIteratorCreate(arr, 8);
  size_t idx;

  TEST_ASSERT_TRUE(gvizBitArrayIterate(&it, &idx));
  TEST_ASSERT_EQUAL(3, idx);
  TEST_ASSERT_FALSE(gvizBitArrayIterate(&it, &idx));
}

void test_iterate_ascending_order(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(128)];
  memset(arr, 0, sizeof(arr));

  int indices[] = {99, 7, 42, 63, 64, 1, 120};
  int count = sizeof(indices) / sizeof(indices[0]);

  for (int i = 0; i < count; i++) {
    gvizSetBit(arr, (size_t)indices[i]);
  }

  size_t collected[16];
  size_t n = collect_set_bits(arr, 128, collected, 16);

  TEST_ASSERT_EQUAL(count, (int)n);
  for (size_t i = 1; i < n; i++) {
    TEST_ASSERT_LESS_THAN(collected[i], collected[i - 1]);
  }
}

void test_iterate_matches_test_bit_stride(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(200)];
  memset(arr, 0, sizeof(arr));

  for (size_t i = 0; i < 200; i += 7) {
    gvizSetBit(arr, i);
  }

  assert_bits_match_iterator(arr, 200);
}

void test_iterate_matches_test_bit_dense(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(80)];
  memset(arr, 0, sizeof(arr));

  for (size_t i = 0; i < 80; i++) {
    gvizSetBit(arr, i);
  }

  assert_bits_match_iterator(arr, 80);
}

void test_iterate_padding_bits_excluded(void) {
  size_t nbits = GVIZ_BITS_PER_WORD + 5;
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(nbits + 8)];
  memset(arr, 0xFF, sizeof(arr));

  size_t collected[128];
  size_t n = collect_set_bits(arr, nbits, collected, 128);

  TEST_ASSERT_EQUAL(nbits, n);
  for (size_t i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL(i, collected[i]);
  }
}

void test_iterate_sparse_large(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(1000)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, 0);
  gvizSetBit(arr, 500);
  gvizSetBit(arr, 999);

  assert_bits_match_iterator(arr, 1000);
}

// ============================================================================
// POPCOUNT
// ============================================================================

void test_popcount_empty(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(16)];
  memset(arr, 0, sizeof(arr));
  TEST_ASSERT_EQUAL_UINT64(0, gvizBitArrayPopcount(arr, 16));
}

void test_popcount_all_set(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(32)];
  memset(arr, 0xFF, sizeof(arr));
  TEST_ASSERT_EQUAL_UINT64(32, gvizBitArrayPopcount(arr, 32));
}

void test_popcount_sparse(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(100)];
  memset(arr, 0, sizeof(arr));
  gvizSetBit(arr, 0);
  gvizSetBit(arr, 50);
  gvizSetBit(arr, 99);
  TEST_ASSERT_EQUAL_UINT64(3, gvizBitArrayPopcount(arr, 100));
}

void test_popcount_range_middle(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(64)];
  memset(arr, 0, sizeof(arr));
  for (size_t i = 10; i < 20; i++)
    gvizSetBit(arr, i);
  gvizSetBit(arr, 0);
  gvizSetBit(arr, 63);

  TEST_ASSERT_EQUAL_UINT64(10, gvizBitArrayPopcountRange(arr, 10, 20));
  TEST_ASSERT_EQUAL_UINT64(0, gvizBitArrayPopcountRange(arr, 20, 10));
}

void test_popcount_range_cross_word(void) {
  size_t nbits = GVIZ_BITS_PER_WORD + 10;
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(nbits)];
  memset(arr, 0, sizeof(arr));

  gvizSetBit(arr, GVIZ_BITS_PER_WORD - 1);
  gvizSetBit(arr, GVIZ_BITS_PER_WORD);
  gvizSetBit(arr, GVIZ_BITS_PER_WORD + 1);

  TEST_ASSERT_EQUAL_UINT64(3, gvizBitArrayPopcountRange(arr, GVIZ_BITS_PER_WORD - 1,
                                                        GVIZ_BITS_PER_WORD + 2));
}

void test_iterator_range_matches_popcount(void) {
  GVIZ_BIT_UNIT arr[GVIZ_ARRAY_UNITS(80)];
  memset(arr, 0, sizeof(arr));
  for (size_t i = 5; i < 40; i += 3)
    gvizSetBit(arr, i);

  size_t count = 0;
  gvizBitArrayIterator it = gvizBitArrayIteratorCreateRange(arr, 5, 40);
  size_t idx;
  while (gvizBitArrayIterate(&it, &idx))
    count++;

  TEST_ASSERT_EQUAL_UINT64(gvizBitArrayPopcountRange(arr, 5, 40), count);
}

void test_bitarray_api_wrappers(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(16);
  TEST_ASSERT_NOT_NULL(arr);

  gvizBitArraySet(arr, 3);
  TEST_ASSERT_TRUE(gvizBitArrayTest(arr, 3));
  TEST_ASSERT_FALSE(gvizBitArrayTest(arr, 4));

  gvizBitArrayClear(arr, 3);
  TEST_ASSERT_FALSE(gvizBitArrayTest(arr, 3));

  GVIZ_BIT_ARRAY grown = gvizBitArrayResize(arr, 16, 32);
  TEST_ASSERT_NOT_NULL(grown);
  gvizBitArraySet(grown, 20);
  TEST_ASSERT_TRUE(gvizBitArrayTest(grown, 20));

  gvizBitArrayFree(grown);
}

void test_clear_range_small_buffer(void) {
  GVIZ_BIT_ARRAY arr = gvizBitArrayAlloc(6);
  TEST_ASSERT_NOT_NULL(arr);
  for (size_t i = 0; i < 6; i++)
    gvizBitArraySet(arr, i);
  gvizBitArrayClearRange(arr, 1, 5);
  TEST_ASSERT_TRUE(gvizBitArrayTest(arr, 0));
  TEST_ASSERT_FALSE(gvizBitArrayTest(arr, 1));
  TEST_ASSERT_FALSE(gvizBitArrayTest(arr, 4));
  TEST_ASSERT_TRUE(gvizBitArrayTest(arr, 5));
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

  RUN_TEST(test_or_disjoint_bits);
  RUN_TEST(test_or_overlapping_bits);
  RUN_TEST(test_or_preserves_dest_only_bits);
  RUN_TEST(test_and_disjoint_clears_all);
  RUN_TEST(test_and_keeps_common_bits);
  RUN_TEST(test_or_and_cross_word_boundary);
  RUN_TEST(test_or_and_partial_last_word);

  RUN_TEST(test_iterate_empty_array);
  RUN_TEST(test_iterate_zero_nbits);
  RUN_TEST(test_iterate_single_bit);
  RUN_TEST(test_iterate_multiple_bits_same_byte);
  RUN_TEST(test_iterate_cross_byte_boundary);
  RUN_TEST(test_iterate_cross_word_boundary);
  RUN_TEST(test_iterate_exhausted_returns_false);
  RUN_TEST(test_iterate_ascending_order);
  RUN_TEST(test_iterate_matches_test_bit_stride);
  RUN_TEST(test_iterate_matches_test_bit_dense);
  RUN_TEST(test_iterate_padding_bits_excluded);
  RUN_TEST(test_iterate_sparse_large);
  RUN_TEST(test_popcount_empty);
  RUN_TEST(test_popcount_all_set);
  RUN_TEST(test_popcount_sparse);
  RUN_TEST(test_popcount_range_middle);
  RUN_TEST(test_popcount_range_cross_word);
  RUN_TEST(test_iterator_range_matches_popcount);
  RUN_TEST(test_bitarray_api_wrappers);
  RUN_TEST(test_clear_range_small_buffer);

  return UNITY_END();
}
