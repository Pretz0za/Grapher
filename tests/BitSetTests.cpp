// Port of tests/ds/gvizBitArrayTests.c against gviz::BitSet.
//
// Three groups of old scenarios have no analog here and are intentionally
// dropped rather than force-fitted, each for the same underlying reason:
// gviz::BitSet is a proper sized class backed by std::vector<uint64_t> word
// storage, not a raw byte pointer the caller tracks a separate size for.
//   - "ARRAY UNITS CALCULATION" tested the GVIZ_ARRAY_UNITS(nbits) byte-count
//     macro directly; BitSet has no such macro (word count is an
//     implementation detail of Resize()/the constructor, never observed).
//   - "MEMORY ISOLATION" and "LSB ORDERING" asserted exact raw byte values
//     (e.g. arr[0] == 0x80) against the old GVIZ_BIT_UNIT=unsigned char
//     layout. BitSet's word type is uint64_t and never exposes raw storage,
//     so there is no public equivalent to assert a byte pattern against;
//     the underlying behavior they were really checking (bits don't leak
//     into neighboring indices) is already covered by
//     test_clearBit_preserves_others and the boundary/crossing tests below.
//   - gvizBitArrayOr/And and gvizBitArrayCopyBits have no BitSet port: a
//     grep of ds/gvizGraph.c and ds/gvizSubgraph.c (BitSet's only consumers
//     so far) turned up zero call sites for any of the three, so per
//     "don't build unused API" they were left out of BitSet.hpp entirely.
//     test_copy_bits_preserves_tail and the seven OR/AND tests are dropped
//     accordingly.

#include "BitSet.hpp"

#include "unity/unity.h"

#include <vector>

void setUp(void) {}
void tearDown(void) {}

static size_t collect_set_bits(const gviz::BitSet &bs, size_t *out, size_t max_out) {
  size_t n = 0;
  for (size_t idx : bs) {
    if (n >= max_out)
      break;
    out[n++] = idx;
  }
  return n;
}

static void assert_bits_match_iterator(const gviz::BitSet &bs) {
  size_t collected[1024];
  size_t n = collect_set_bits(bs, collected, 1024);

  size_t expect = 0;
  for (size_t i = 0; i < bs.Size(); i++) {
    if (bs.Test(i)) {
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

static void test_setBit_single(void) {
  gviz::BitSet bs(8);
  bs.Set(0);
  TEST_ASSERT_TRUE(bs.Test(0));
}

static void test_setBit_multiple_same_unit(void) {
  gviz::BitSet bs(8);
  bs.Set(0);
  bs.Set(1);
  bs.Set(3);

  TEST_ASSERT_TRUE(bs.Test(0));
  TEST_ASSERT_TRUE(bs.Test(1));
  TEST_ASSERT_FALSE(bs.Test(2));
  TEST_ASSERT_TRUE(bs.Test(3));
  TEST_ASSERT_FALSE(bs.Test(4));
}

static void test_setBit_crosses_units(void) {
  gviz::BitSet bs(20);
  bs.Set(7);
  bs.Set(8);
  bs.Set(15);
  bs.Set(16);

  TEST_ASSERT_TRUE(bs.Test(7));
  TEST_ASSERT_TRUE(bs.Test(8));
  TEST_ASSERT_TRUE(bs.Test(15));
  TEST_ASSERT_TRUE(bs.Test(16));
}

static void test_testBit_unset(void) {
  gviz::BitSet bs(8);
  TEST_ASSERT_FALSE(bs.Test(0));
  TEST_ASSERT_FALSE(bs.Test(5));
  TEST_ASSERT_FALSE(bs.Test(7));
}

static void test_testBit_returns_true(void) {
  gviz::BitSet bs(8);
  bs.Set(3);
  TEST_ASSERT_TRUE(bs.Test(3));
}

// ============================================================================
// CLEAR BIT
// ============================================================================

static void test_clearBit_single(void) {
  gviz::BitSet bs(8);
  bs.Set(2);
  TEST_ASSERT_TRUE(bs.Test(2));
  bs.Clear(2);
  TEST_ASSERT_FALSE(bs.Test(2));
}

static void test_clearBit_preserves_others(void) {
  gviz::BitSet bs(8);
  bs.Set(1);
  bs.Set(2);
  bs.Set(3);
  bs.Clear(2);

  TEST_ASSERT_TRUE(bs.Test(1));
  TEST_ASSERT_FALSE(bs.Test(2));
  TEST_ASSERT_TRUE(bs.Test(3));
}

static void test_clearBit_unset_bit(void) {
  gviz::BitSet bs(8);
  bs.Clear(4);
  TEST_ASSERT_FALSE(bs.Test(4));
}

static void test_clearBit_crosses_units(void) {
  gviz::BitSet bs(20);
  bs.Set(7);
  bs.Set(8);
  bs.Set(9);
  bs.Clear(8);

  TEST_ASSERT_TRUE(bs.Test(7));
  TEST_ASSERT_FALSE(bs.Test(8));
  TEST_ASSERT_TRUE(bs.Test(9));
}

// ============================================================================
// SET & CLEAR PATTERNS
// ============================================================================

static void test_all_bits_in_word(void) {
  gviz::BitSet bs(64);
  for (int i = 0; i < 64; i++)
    bs.Set(i);
  for (int i = 0; i < 64; i++)
    TEST_ASSERT_TRUE(bs.Test(i));
}

static void test_alternating_bits(void) {
  gviz::BitSet bs(16);
  for (int i = 0; i < 16; i += 2)
    bs.Set(i);

  for (int i = 0; i < 16; i++) {
    if (i % 2 == 0)
      TEST_ASSERT_TRUE(bs.Test(i));
    else
      TEST_ASSERT_FALSE(bs.Test(i));
  }
}

static void test_set_then_clear_all(void) {
  gviz::BitSet bs(16);
  for (int i = 0; i < 16; i++)
    bs.Set(i);
  bs.ClearAll();
  for (int i = 0; i < 16; i++)
    TEST_ASSERT_FALSE(bs.Test(i));
}

static void test_toggle_pattern(void) {
  gviz::BitSet bs(8);
  for (int i = 0; i < 8; i++)
    bs.Set(i);
  for (int i = 0; i < 8; i += 2)
    bs.Clear(i);

  for (int i = 0; i < 8; i++) {
    if (i % 2 == 1)
      TEST_ASSERT_TRUE(bs.Test(i));
    else
      TEST_ASSERT_FALSE(bs.Test(i));
  }
}

// ============================================================================
// BOUNDARY CONDITIONS
// ============================================================================

static void test_single_bit_array(void) {
  gviz::BitSet bs(1);
  bs.Set(0);
  TEST_ASSERT_TRUE(bs.Test(0));
  bs.Clear(0);
  TEST_ASSERT_FALSE(bs.Test(0));
}

static void test_boundary_between_words(void) {
  gviz::BitSet bs(65);
  bs.Set(63);
  bs.Set(64);
  TEST_ASSERT_TRUE(bs.Test(63));
  TEST_ASSERT_TRUE(bs.Test(64));
}

static void test_large_bit_index(void) {
  gviz::BitSet bs(256);
  bs.Set(255);
  TEST_ASSERT_TRUE(bs.Test(255));
  bs.Clear(255);
  TEST_ASSERT_FALSE(bs.Test(255));
}

static void test_multiple_words_boundary(void) {
  gviz::BitSet bs(3 * 64);
  bs.Set(63);
  bs.Set(64);
  bs.Set(127);
  bs.Set(128);

  TEST_ASSERT_TRUE(bs.Test(63));
  TEST_ASSERT_TRUE(bs.Test(64));
  TEST_ASSERT_TRUE(bs.Test(127));
  TEST_ASSERT_TRUE(bs.Test(128));
}

// ============================================================================
// STRESS TESTS
// ============================================================================

static void test_sparse_large_array(void) {
  gviz::BitSet bs(1000);
  bs.Set(0);
  bs.Set(500);
  bs.Set(999);

  TEST_ASSERT_TRUE(bs.Test(0));
  TEST_ASSERT_TRUE(bs.Test(500));
  TEST_ASSERT_TRUE(bs.Test(999));

  for (int i = 1; i < 500; i++) {
    if (i != 500)
      TEST_ASSERT_FALSE(bs.Test(i));
  }
}

static void test_dense_large_array(void) {
  gviz::BitSet bs(512);
  for (int i = 0; i < 512; i += 3)
    bs.Set(i);

  for (int i = 0; i < 512; i++) {
    if (i % 3 == 0)
      TEST_ASSERT_TRUE(bs.Test(i));
    else
      TEST_ASSERT_FALSE(bs.Test(i));
  }
}

static void test_random_access_pattern(void) {
  gviz::BitSet bs(256);
  int indices[] = {7, 42, 15, 123, 200, 99, 1, 255, 64, 128};
  int count = sizeof(indices) / sizeof(indices[0]);

  for (int i = 0; i < count; i++)
    bs.Set(static_cast<size_t>(indices[i]));
  for (int i = 0; i < count; i++)
    TEST_ASSERT_TRUE(bs.Test(static_cast<size_t>(indices[i])));

  bs.Clear(static_cast<size_t>(indices[0]));
  TEST_ASSERT_FALSE(bs.Test(static_cast<size_t>(indices[0])));

  for (int i = 1; i < count; i++)
    TEST_ASSERT_TRUE(bs.Test(static_cast<size_t>(indices[i])));
}

// ============================================================================
// ITERATOR
// ============================================================================

static void test_iterate_empty_array(void) {
  gviz::BitSet bs(16);
  TEST_ASSERT_TRUE(bs.begin() == bs.end());
}

static void test_iterate_zero_nbits(void) {
  gviz::BitSet bs(0);
  TEST_ASSERT_TRUE(bs.begin() == bs.end());
}

static void test_iterate_single_bit(void) {
  gviz::BitSet bs(64);
  bs.Set(17);

  size_t collected[4];
  size_t n = collect_set_bits(bs, collected, 4);
  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL(17, collected[0]);
}

static void test_iterate_multiple_bits_same_word(void) {
  gviz::BitSet bs(16);
  bs.Set(2);
  bs.Set(5);
  bs.Set(7);

  size_t collected[8];
  size_t n = collect_set_bits(bs, collected, 8);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(2, collected[0]);
  TEST_ASSERT_EQUAL(5, collected[1]);
  TEST_ASSERT_EQUAL(7, collected[2]);
}

static void test_iterate_cross_word_boundary(void) {
  gviz::BitSet bs(80);
  bs.Set(63);
  bs.Set(64);
  bs.Set(69);

  size_t collected[8];
  size_t n = collect_set_bits(bs, collected, 8);
  TEST_ASSERT_EQUAL(3, n);
  TEST_ASSERT_EQUAL(63, collected[0]);
  TEST_ASSERT_EQUAL(64, collected[1]);
  TEST_ASSERT_EQUAL(69, collected[2]);
}

static void test_iterate_exhausted_returns_end(void) {
  gviz::BitSet bs(8);
  bs.Set(3);

  auto it = bs.begin();
  TEST_ASSERT_TRUE(it != bs.end());
  TEST_ASSERT_EQUAL(3, *it);
  ++it;
  TEST_ASSERT_TRUE(it == bs.end());
}

static void test_iterate_ascending_order(void) {
  gviz::BitSet bs(128);
  int indices[] = {99, 7, 42, 63, 64, 1, 120};
  int count = sizeof(indices) / sizeof(indices[0]);
  for (int i = 0; i < count; i++)
    bs.Set(static_cast<size_t>(indices[i]));

  size_t collected[16];
  size_t n = collect_set_bits(bs, collected, 16);
  TEST_ASSERT_EQUAL(count, (int)n);
  for (size_t i = 1; i < n; i++)
    TEST_ASSERT_LESS_THAN(collected[i], collected[i - 1]);
}

static void test_iterate_matches_test_bit_stride(void) {
  gviz::BitSet bs(200);
  for (size_t i = 0; i < 200; i += 7)
    bs.Set(i);
  assert_bits_match_iterator(bs);
}

static void test_iterate_matches_test_bit_dense(void) {
  gviz::BitSet bs(80);
  for (size_t i = 0; i < 80; i++)
    bs.Set(i);
  assert_bits_match_iterator(bs);
}

static void test_iterate_padding_bits_excluded(void) {
  // BitSet::Set is only ever called for bits < Size(); unlike the old raw
  // GVIZ_BIT_UNIT array (which could have stray 0xFF padding beyond nbits
  // that the iterator had to mask out per word), there is no way through
  // the public API to set a bit beyond Size() in the first place, so this
  // regresses to a plain "every requested bit shows up, nothing beyond
  // does" check via SetAll (which itself masks the tail, see BitSetTests
  // test_setAll_*).
  size_t nbits = 64 + 5;
  gviz::BitSet bs(nbits);
  bs.SetAll();

  size_t collected[128];
  size_t n = collect_set_bits(bs, collected, 128);
  TEST_ASSERT_EQUAL(nbits, n);
  for (size_t i = 0; i < n; i++)
    TEST_ASSERT_EQUAL(i, collected[i]);
}

static void test_iterate_sparse_large(void) {
  gviz::BitSet bs(1000);
  bs.Set(0);
  bs.Set(500);
  bs.Set(999);
  assert_bits_match_iterator(bs);
}

static void test_range_matches_iterate(void) {
  gviz::BitSet bs(80);
  for (size_t i = 5; i < 40; i += 3)
    bs.Set(i);

  size_t count = 0;
  for (size_t idx : bs.Range(5, 40)) {
    TEST_ASSERT_TRUE(idx >= 5 && idx < 40);
    count++;
  }
  TEST_ASSERT_EQUAL_UINT64(bs.PopcountRange(5, 40), count);
}

static void test_range_excludes_outside_bits(void) {
  gviz::BitSet bs(64);
  bs.Set(2);
  bs.Set(10);
  bs.Set(50);

  size_t collected[8];
  size_t n = 0;
  for (size_t idx : bs.Range(5, 20))
    collected[n++] = idx;

  TEST_ASSERT_EQUAL(1, n);
  TEST_ASSERT_EQUAL(10, collected[0]);
}

// ============================================================================
// POPCOUNT
// ============================================================================

static void test_popcount_empty(void) {
  gviz::BitSet bs(16);
  TEST_ASSERT_EQUAL_UINT64(0, bs.Popcount());
}

static void test_popcount_all_set(void) {
  gviz::BitSet bs(32);
  for (size_t i = 0; i < 32; i++)
    bs.Set(i);
  TEST_ASSERT_EQUAL_UINT64(32, bs.Popcount());
}

static void test_popcount_sparse(void) {
  gviz::BitSet bs(100);
  bs.Set(0);
  bs.Set(50);
  bs.Set(99);
  TEST_ASSERT_EQUAL_UINT64(3, bs.Popcount());
}

static void test_popcount_range_middle(void) {
  gviz::BitSet bs(64);
  for (size_t i = 10; i < 20; i++)
    bs.Set(i);
  bs.Set(0);
  bs.Set(63);

  TEST_ASSERT_EQUAL_UINT64(10, bs.PopcountRange(10, 20));
  TEST_ASSERT_EQUAL_UINT64(0, bs.PopcountRange(20, 10));
}

static void test_popcount_range_cross_word(void) {
  gviz::BitSet bs(64 + 10);
  bs.Set(63);
  bs.Set(64);
  bs.Set(65);
  TEST_ASSERT_EQUAL_UINT64(3, bs.PopcountRange(63, 66));
}

// ============================================================================
// RESIZE
// ============================================================================

static void test_resize_grows_and_zeroes(void) {
  gviz::BitSet bs(16);
  bs.Set(3);
  bs.Resize(32);
  TEST_ASSERT_EQUAL_UINT64(32, bs.Size());
  TEST_ASSERT_TRUE(bs.Test(3));
  bs.Set(20);
  TEST_ASSERT_TRUE(bs.Test(20));
}

static void test_resize_shrinks_and_masks_tail(void) {
  gviz::BitSet bs(80);
  bs.SetAll();
  bs.Resize(40);
  TEST_ASSERT_EQUAL_UINT64(40, bs.Size());
  TEST_ASSERT_EQUAL_UINT64(40, bs.Popcount());

  // Growing back must not resurrect the bits that were dropped on shrink.
  bs.Resize(80);
  TEST_ASSERT_EQUAL_UINT64(40, bs.Popcount());
}

// ============================================================================
// CLEAR RANGE / SET ALL
// ============================================================================

static void test_clear_range_small_buffer(void) {
  gviz::BitSet bs(6);
  bs.SetAll();
  bs.ClearRange(1, 5);
  TEST_ASSERT_TRUE(bs.Test(0));
  TEST_ASSERT_FALSE(bs.Test(1));
  TEST_ASSERT_FALSE(bs.Test(4));
  TEST_ASSERT_TRUE(bs.Test(5));
}

// Regression: a range that sits strictly inside one word, away from both
// word edges, in a buffer with more than one word -- e.g. clearing [10,20)
// out of a 1000-bit set. Both boundary conditions ("keep bits before
// start" and "keep bits at/after end") apply to the SAME word here; they
// must be OR'd together (the bits to preserve), not AND'd, or every bit in
// the word gets cleared instead of just [10, 20). The old C
// gvizBitArrayClearRange this was ported from combined them with AND and
// never caught it because every call site in the existing test suite
// happens to clear a range small enough to hit a different code path.
static void test_clearRange_middleOfWord_inLargeBuffer(void) {
  gviz::BitSet bs(1000);
  bs.SetAll();
  bs.ClearRange(10, 20);

  for (size_t i = 0; i < 10; i++)
    TEST_ASSERT_TRUE(bs.Test(i));
  for (size_t i = 10; i < 20; i++)
    TEST_ASSERT_FALSE(bs.Test(i));
  for (size_t i = 20; i < 64; i++)
    TEST_ASSERT_TRUE(bs.Test(i));
}

// Regression, same root cause: a range spanning multiple whole words in
// the middle (word 1 here) must clear every bit in those middle words, not
// leave them untouched.
static void test_clearRange_spansMultipleWords(void) {
  gviz::BitSet bs(200);
  bs.SetAll();
  bs.ClearRange(50, 150);

  for (size_t i = 0; i < 50; i++)
    TEST_ASSERT_TRUE(bs.Test(i));
  for (size_t i = 50; i < 150; i++)
    TEST_ASSERT_FALSE(bs.Test(i));
  for (size_t i = 150; i < 200; i++)
    TEST_ASSERT_TRUE(bs.Test(i));
}

static void test_setAll_fills_exactly_size_bits(void) {
  gviz::BitSet bs(64 + 5);
  bs.SetAll();
  TEST_ASSERT_EQUAL_UINT64(64 + 5, bs.Popcount());
  for (size_t i = 0; i < bs.Size(); i++)
    TEST_ASSERT_TRUE(bs.Test(i));
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main() {
  UNITY_BEGIN();

  RUN_TEST(test_setBit_single);
  RUN_TEST(test_setBit_multiple_same_unit);
  RUN_TEST(test_setBit_crosses_units);
  RUN_TEST(test_testBit_unset);
  RUN_TEST(test_testBit_returns_true);

  RUN_TEST(test_clearBit_single);
  RUN_TEST(test_clearBit_preserves_others);
  RUN_TEST(test_clearBit_unset_bit);
  RUN_TEST(test_clearBit_crosses_units);

  RUN_TEST(test_all_bits_in_word);
  RUN_TEST(test_alternating_bits);
  RUN_TEST(test_set_then_clear_all);
  RUN_TEST(test_toggle_pattern);

  RUN_TEST(test_single_bit_array);
  RUN_TEST(test_boundary_between_words);
  RUN_TEST(test_large_bit_index);
  RUN_TEST(test_multiple_words_boundary);

  RUN_TEST(test_sparse_large_array);
  RUN_TEST(test_dense_large_array);
  RUN_TEST(test_random_access_pattern);

  RUN_TEST(test_iterate_empty_array);
  RUN_TEST(test_iterate_zero_nbits);
  RUN_TEST(test_iterate_single_bit);
  RUN_TEST(test_iterate_multiple_bits_same_word);
  RUN_TEST(test_iterate_cross_word_boundary);
  RUN_TEST(test_iterate_exhausted_returns_end);
  RUN_TEST(test_iterate_ascending_order);
  RUN_TEST(test_iterate_matches_test_bit_stride);
  RUN_TEST(test_iterate_matches_test_bit_dense);
  RUN_TEST(test_iterate_padding_bits_excluded);
  RUN_TEST(test_iterate_sparse_large);
  RUN_TEST(test_range_matches_iterate);
  RUN_TEST(test_range_excludes_outside_bits);

  RUN_TEST(test_popcount_empty);
  RUN_TEST(test_popcount_all_set);
  RUN_TEST(test_popcount_sparse);
  RUN_TEST(test_popcount_range_middle);
  RUN_TEST(test_popcount_range_cross_word);

  RUN_TEST(test_resize_grows_and_zeroes);
  RUN_TEST(test_resize_shrinks_and_masks_tail);

  RUN_TEST(test_clear_range_small_buffer);
  RUN_TEST(test_clearRange_middleOfWord_inLargeBuffer);
  RUN_TEST(test_clearRange_spansMultipleWords);
  RUN_TEST(test_setAll_fills_exactly_size_bits);

  return UNITY_END();
}
