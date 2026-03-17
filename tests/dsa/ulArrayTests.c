#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "dsa/uLongArray.h"
#include "unity/unity.h"

#ifndef ULARRAY_TEST_MAX_STRESS
#define ULARRAY_TEST_MAX_STRESS 20000u
#endif

// -------------------- Helpers --------------------

static void assert_invariants(const ULongArray *v) {
  // "Black-box" invariants consistent with a dynamic array.
  // We do not assume arr is non-NULL when capacity=0; both are acceptable.
  TEST_ASSERT_NOT_NULL(v);

  // count should never exceed capacity (if capacity is meaningful).
  // Some implementations may keep capacity=0 with non-NULL arr (rare but
  // possible); this check would fail in that case. If your implementation does
  // that, remove it.
  TEST_ASSERT_TRUE_MESSAGE(v->count <= v->capacity, "count exceeds capacity");

  if (v->arr == NULL) {
    // If arr is NULL, count must be 0 to avoid deref bugs.
    TEST_ASSERT_EQUAL_MESSAGE((size_t)0, v->count, "arr==NULL but count!=0");
    // capacity may be 0 or non-zero depending on how init failure is handled;
    // but after successful init, capacity should typically be >0 if arr!=NULL.
  }
}

static void fill_and_check_sequence(ULongArray *v, size_t n, size_t base) {
  for (size_t i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL_INT(0, ulArrayPush(v, base + i));
    assert_invariants(v);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(base + i), (uint64_t)v->arr[i]);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(i + 1), (uint64_t)v->count);
  }
}

static void pop_and_check_reverse(ULongArray *v, size_t n, size_t base) {
  for (size_t i = 0; i < n; i++) {
    size_t res = (size_t)0xDEADBEEF;
    TEST_ASSERT_EQUAL_INT(0, ulArrayPop(v, &res));
    assert_invariants(v);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(base + (n - 1 - i)), (uint64_t)res);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(n - 1 - i), (uint64_t)v->count);
  }
}

// -------------------- Unity fixtures --------------------

void setUp(void) {
#ifdef ULARRAY_TEST_WRAP_ALLOC
  ul_test_alloc_reset();
#endif
}
void tearDown(void) {}

// -------------------- Tests: init / release --------------------

void test_init_null_pointer_fails(void) {
  TEST_ASSERT_EQUAL_INT(-1, ulArrayInit(NULL));
}

void test_init_success_basic_invariants(void) {
  ULongArray v;
  memset(&v, 0xA5, sizeof(v)); // poison to catch partial init bugs

  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));
  assert_invariants(&v);

  // Empty after init
  TEST_ASSERT_EQUAL_INT(1, ulArrayIsEmpty(&v));
  TEST_ASSERT_EQUAL_UINT64((uint64_t)0, (uint64_t)v.count);

  ulArrayRelease(&v);
}

void test_init_at_capacity_zero_or_more(void) {
  ULongArray v;
  memset(&v, 0, sizeof(v));

  // API doesn't forbid 0 capacity; test it doesn't crash and remains valid if
  // succeeds.
  int rc = ulArrayInitAtCapacity(&v, 0);
  if (rc == 0) {
    assert_invariants(&v);
    TEST_ASSERT_EQUAL_INT(1, ulArrayIsEmpty(&v));
    ulArrayRelease(&v);
  } else {
    // If it returns -1, that's acceptable: allocation may treat 0 as error.
    TEST_ASSERT_EQUAL_INT(-1, rc);
  }
}

// -------------------- Tests: push / realloc / pop --------------------

void test_push_and_pop_single(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));

  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 123));
  assert_invariants(&v);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)v.count);
  TEST_ASSERT_EQUAL_UINT64(123, (uint64_t)v.arr[0]);

  size_t res = 0;
  TEST_ASSERT_EQUAL_INT(0, ulArrayPop(&v, &res));
  TEST_ASSERT_EQUAL_UINT64(123, (uint64_t)res);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)v.count);
  TEST_ASSERT_EQUAL_INT(1, ulArrayIsEmpty(&v));

  // popping from empty should fail and not modify res
  res = 0xCAFEBABE;
  TEST_ASSERT_EQUAL_INT(-1, ulArrayPop(&v, &res));
  TEST_ASSERT_EQUAL_UINT64((uint64_t)0xCAFEBABE, (uint64_t)res);

  ulArrayRelease(&v);
}

void test_push_many_stress_preserves_all_values(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));

  const size_t n = (size_t)ULARRAY_TEST_MAX_STRESS;
  for (size_t i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, i * 3u + 7u));
    assert_invariants(&v);
  }

  TEST_ASSERT_EQUAL_UINT64((uint64_t)n, (uint64_t)v.count);
  for (size_t i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(i * 3u + 7u), (uint64_t)v.arr[i]);
  }

  // Pop half and ensure remaining prefix is intact
  for (size_t i = 0; i < n / 2; i++) {
    size_t res = 0;
    TEST_ASSERT_EQUAL_INT(0, ulArrayPop(&v, &res));
    assert_invariants(&v);
  }

  TEST_ASSERT_EQUAL_UINT64((uint64_t)(n - n / 2), (uint64_t)v.count);
  for (size_t i = 0; i < v.count; i++) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(i * 3u + 7u), (uint64_t)v.arr[i]);
  }

  ulArrayRelease(&v);
}

// -------------------- Tests: find --------------------

void test_find_one_found_and_not_found(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));

  // empty: not found
  TEST_ASSERT_EQUAL_INT(-1, ulArrayFindOne(&v, 10));

  // add duplicates
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 5));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 10));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 10));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 20));

  TEST_ASSERT_EQUAL_INT(1, ulArrayFindOne(&v, 10)); // first instance
  TEST_ASSERT_EQUAL_INT(0, ulArrayFindOne(&v, 5));
  TEST_ASSERT_EQUAL_INT(3, ulArrayFindOne(&v, 20));
  TEST_ASSERT_EQUAL_INT(-1, ulArrayFindOne(&v, 999));

  ulArrayRelease(&v);
}

// -------------------- Tests: delete-one --------------------

void test_find_one_and_delete_not_found_no_change(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));
  fill_and_check_sequence(&v, 10, 100);

  // Snapshot
  size_t before_count = v.count;
  size_t before0 = v.arr[0];
  int rc = ulArrayFindOneAndDelete(&v, 999999);
  TEST_ASSERT_EQUAL_INT(-1, rc);

  // no change expected
  TEST_ASSERT_EQUAL_UINT64((uint64_t)before_count, (uint64_t)v.count);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)before0, (uint64_t)v.arr[0]);

  // validate full sequence intact
  for (size_t i = 0; i < v.count; i++) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(100 + i), (uint64_t)v.arr[i]);
  }

  ulArrayRelease(&v);
}

void test_find_one_and_delete_first_middle_last_and_duplicates(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));

  // [1,2,3,2,4]
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 1));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 2));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 3));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 2));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 4));
  assert_invariants(&v);

  // delete first element (1)
  TEST_ASSERT_EQUAL_INT(0, ulArrayFindOneAndDelete(&v, 1));
  TEST_ASSERT_EQUAL_UINT64(4, (uint64_t)v.count);
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)v.arr[0]);

  // delete middle element (first 2 at index 0 now)
  TEST_ASSERT_EQUAL_INT(0, ulArrayFindOneAndDelete(&v, 2));
  TEST_ASSERT_EQUAL_UINT64(3, (uint64_t)v.count);
  // remaining should be [3,2,4] (stable order if implemented by shift-left)
  // Some implementations might swap-with-last; API does not specify stability.
  // So we check multiset instead of order.
  size_t seen3 = 0, seen2 = 0, seen4 = 0, seen1 = 0;
  for (size_t i = 0; i < v.count; i++) {
    if (v.arr[i] == 1)
      seen1++;
    if (v.arr[i] == 3)
      seen3++;
    if (v.arr[i] == 2)
      seen2++;
    if (v.arr[i] == 4)
      seen4++;
  }
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)seen1);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)seen3);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)seen2);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)seen4);

  // delete last remaining 4
  int idx4 = ulArrayFindOne(&v, 4);
  TEST_ASSERT_TRUE(idx4 >= 0);
  TEST_ASSERT_EQUAL_INT(idx4, ulArrayFindOneAndDelete(&v, 4));
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)v.count);
  TEST_ASSERT_EQUAL_INT(-1, ulArrayFindOne(&v, 4));

  ulArrayRelease(&v);
}

// -------------------- Tests: copy --------------------

void test_copy_creates_independent_buffer_and_equal_contents(void) {
  ULongArray src, dest;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&src));
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&dest));

  fill_and_check_sequence(&src, 128, 1000);

  TEST_ASSERT_EQUAL_INT(0, ulArrayCopy(&dest, &src));
  assert_invariants(&src);
  assert_invariants(&dest);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)src.count, (uint64_t)dest.count);

  // content equality
  for (size_t i = 0; i < src.count; i++) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)src.arr[i], (uint64_t)dest.arr[i]);
  }

  // Deep copy check: mutating dest should not affect src
  TEST_ASSERT_EQUAL_INT(
      0, ulArrayFindOneAndDelete(&dest, 1000)); // delete first element in dest
  TEST_ASSERT_EQUAL_UINT64(127, (uint64_t)dest.count);
  TEST_ASSERT_EQUAL_UINT64(128, (uint64_t)src.count);
  TEST_ASSERT_EQUAL_UINT64(1000, (uint64_t)src.arr[0]); // src unchanged

  ulArrayRelease(&src);
  ulArrayRelease(&dest);
}

// -------------------- Tests: move --------------------

void test_move_transfers_ownership_and_contents(void) {
  ULongArray src, dest;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&src));

  fill_and_check_sequence(&src, 64, 500);

  size_t *src_ptr = src.arr;
  size_t src_count = src.count;

  ulArrayMove(&dest, &src);
  assert_invariants(&dest);

  TEST_ASSERT_EQUAL_PTR(src_ptr, dest.arr);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)src_count, (uint64_t)dest.count);

  for (size_t i = 0; i < dest.count; i++) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)(500 + i), (uint64_t)dest.arr[i]);
  }

  // src is "invalid" after move per API; we only do the safest black-box check:
  // it should not still own the same pointer.
  TEST_ASSERT_TRUE_MESSAGE(src.arr != dest.arr,
                           "src still aliases dest after move");

  // We will not call ulArrayRelease(&src) because API says src is invalid.
  ulArrayRelease(&dest);
}

// -------------------- Tests: print --------------------

void test_print_format_basic(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 1));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 2));
  TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, 3));

  // Use tmpfile to avoid platform-specific open_memstream
  FILE *f = tmpfile();
  TEST_ASSERT_NOT_NULL(f);

  ulArrayPrint(&v, f);
  fflush(f);
  rewind(f);

  char buf[256];
  memset(buf, 0, sizeof(buf));
  size_t read = fread(buf, 1, sizeof(buf) - 1, f);
  (void)read;
  fclose(f);

  // Very loose black-box expectations: must contain brackets and all numbers.
  TEST_ASSERT_NOT_NULL(strstr(buf, "["));
  TEST_ASSERT_NOT_NULL(strstr(buf, "]"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "1"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "2"));
  TEST_ASSERT_NOT_NULL(strstr(buf, "3"));

  ulArrayRelease(&v);
}

// -------------------- Tests: allocation failure injection (optional)
// --------------------

#ifdef ULARRAY_TEST_WRAP_ALLOC
void test_push_realloc_failure_keeps_vector_valid_and_unchanged(void) {
  ULongArray v;
  TEST_ASSERT_EQUAL_INT(0, ulArrayInit(&v));

  // Fill until at least one realloc likely happens. Since we don't know growth,
  // we do it adaptively by watching capacity changes.
  size_t initial_cap = v.capacity;

  // Ensure there's at least some content.
  for (size_t i = 0; i < 64; i++) {
    TEST_ASSERT_EQUAL_INT(0, ulArrayPush(&v, i));
  }
  assert_invariants(&v);

  // Now set realloc to fail on next realloc call and keep pushing until it
  // fails.
  g_fail_realloc_at = g_realloc_count + 1;

  size_t before_count = v.count;
  size_t before_cap = v.capacity;
  size_t *before_ptr = v.arr;

  int rc = 0;
  // Keep trying pushes; the first one that triggers realloc should fail.
  // Bound the loop so test doesn't hang if push never reallocs (shouldn't
  // happen).
  for (size_t tries = 0; tries < 100000; tries++) {
    rc = ulArrayPush(&v, 0xABCDEFu);
    if (rc == -1)
      break;
  }
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      -1, rc, "Expected a push failure due to forced realloc failure");

  // Black-box postcondition per API: v is still valid
  assert_invariants(&v);

  // And should be unchanged (common strong guarantee); if your implementation
  // offers only basic guarantee, relax these asserts.
  TEST_ASSERT_EQUAL_UINT64((uint64_t)before_count, (uint64_t)v.count);
  TEST_ASSERT_EQUAL_UINT64((uint64_t)before_cap, (uint64_t)v.capacity);
  TEST_ASSERT_EQUAL_PTR(before_ptr, v.arr);

  // Content unchanged
  for (size_t i = 0; i < v.count; i++) {
    TEST_ASSERT_EQUAL_UINT64((uint64_t)i, (uint64_t)v.arr[i]);
  }

  ulArrayRelease(&v);

  // If allocator wrapping is active in the vector implementation too, this
  // catches leaks.
  ul_test_expect_no_leak();

  (void)initial_cap;
}
#endif

// -------------------- Test runner --------------------

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_init_null_pointer_fails);
  RUN_TEST(test_init_success_basic_invariants);
  RUN_TEST(test_init_at_capacity_zero_or_more);

  RUN_TEST(test_push_and_pop_single);
  RUN_TEST(test_push_many_stress_preserves_all_values);

  RUN_TEST(test_find_one_found_and_not_found);
  RUN_TEST(test_find_one_and_delete_not_found_no_change);
  RUN_TEST(test_find_one_and_delete_first_middle_last_and_duplicates);

  RUN_TEST(test_copy_creates_independent_buffer_and_equal_contents);
  RUN_TEST(test_move_transfers_ownership_and_contents);

  RUN_TEST(test_print_format_basic);

#ifdef ULARRAY_TEST_WRAP_ALLOC
  RUN_TEST(test_push_realloc_failure_keeps_vector_valid_and_unchanged);
#endif

  return UNITY_END();
}
