#include "core/gvizThreadPool.h"
#include "unity/unity.h"
#include "unity/unity_internals.h"
#include <stdatomic.h>
#include <string.h>

#define RANGE_N 100000

void setUp(void) {}
void tearDown(void) {}

static _Atomic size_t taskCounter;

static void incrementTask(void *arg) {
  (void)arg;
  atomic_fetch_add(&taskCounter, 1);
}

static void addArgTask(void *arg) {
  atomic_fetch_add(&taskCounter, *(size_t *)arg);
}

typedef struct {
  unsigned char *touched;
  _Atomic size_t sliceCalls;
} rangeCtx;

static void markRange(void *ctx, size_t begin, size_t end) {
  rangeCtx *rc = ctx;
  atomic_fetch_add(&rc->sliceCalls, 1);
  for (size_t i = begin; i < end; i++)
    rc->touched[i]++;
}

void test_create_and_destroy(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(4);
  TEST_ASSERT_NOT_NULL(pool);
  TEST_ASSERT_EQUAL_UINT64(4, gvizThreadPoolThreadCount(pool));
  gvizThreadPoolDestroy(pool);
}

void test_create_default_thread_count(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(0);
  TEST_ASSERT_NOT_NULL(pool);
  TEST_ASSERT_GREATER_THAN(0, gvizThreadPoolThreadCount(pool));
  gvizThreadPoolDestroy(pool);
}

void test_null_safety(void) {
  TEST_ASSERT_EQUAL_UINT64(0, gvizThreadPoolThreadCount(NULL));
  TEST_ASSERT_EQUAL_INT(-1, gvizThreadPoolSubmit(NULL, incrementTask, NULL));
  gvizThreadPoolWait(NULL);
  gvizThreadPoolDestroy(NULL);
}

void test_submit_runs_all_tasks(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(4);
  TEST_ASSERT_NOT_NULL(pool);

  atomic_store(&taskCounter, 0);
  for (size_t i = 0; i < 1000; i++)
    TEST_ASSERT_EQUAL_INT(0, gvizThreadPoolSubmit(pool, incrementTask, NULL));

  gvizThreadPoolWait(pool);
  TEST_ASSERT_EQUAL_UINT64(1000, atomic_load(&taskCounter));

  gvizThreadPoolDestroy(pool);
}

void test_submit_passes_argument(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(2);
  TEST_ASSERT_NOT_NULL(pool);

  atomic_store(&taskCounter, 0);
  size_t args[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  for (size_t i = 0; i < 8; i++)
    TEST_ASSERT_EQUAL_INT(0, gvizThreadPoolSubmit(pool, addArgTask, &args[i]));

  gvizThreadPoolWait(pool);
  TEST_ASSERT_EQUAL_UINT64(36, atomic_load(&taskCounter));

  gvizThreadPoolDestroy(pool);
}

void test_destroy_drains_pending_tasks(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(2);
  TEST_ASSERT_NOT_NULL(pool);

  atomic_store(&taskCounter, 0);
  for (size_t i = 0; i < 500; i++)
    TEST_ASSERT_EQUAL_INT(0, gvizThreadPoolSubmit(pool, incrementTask, NULL));

  gvizThreadPoolDestroy(pool);
  TEST_ASSERT_EQUAL_UINT64(500, atomic_load(&taskCounter));
}

static void checkForRangeCoversAll(gvizThreadPool *pool, size_t grain) {
  unsigned char touched[RANGE_N];
  memset(touched, 0, sizeof(touched));
  rangeCtx rc = {touched, 0};

  TEST_ASSERT_EQUAL_INT(
      0, gvizThreadPoolForRange(pool, 0, RANGE_N, grain, markRange, &rc));

  for (size_t i = 0; i < RANGE_N; i++)
    TEST_ASSERT_EQUAL_UINT8(1, touched[i]);
}

void test_forRange_covers_range_exactly_once(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(8);
  TEST_ASSERT_NOT_NULL(pool);

  checkForRangeCoversAll(pool, 1);
  checkForRangeCoversAll(pool, 64);
  checkForRangeCoversAll(pool, RANGE_N + 1); // single serial slice
  checkForRangeCoversAll(pool, 0);           // grain 0 -> grain 1

  gvizThreadPoolDestroy(pool);
}

void test_forRange_null_pool_runs_serially(void) {
  unsigned char touched[257];
  memset(touched, 0, sizeof(touched));
  rangeCtx rc = {touched, 0};

  TEST_ASSERT_EQUAL_INT(
      0, gvizThreadPoolForRange(NULL, 0, 257, 16, markRange, &rc));

  TEST_ASSERT_EQUAL_UINT64(1, atomic_load(&rc.sliceCalls));
  for (size_t i = 0; i < 257; i++)
    TEST_ASSERT_EQUAL_UINT8(1, touched[i]);
}

void test_forRange_subrange_and_empty(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(4);
  TEST_ASSERT_NOT_NULL(pool);

  unsigned char touched[1024];
  memset(touched, 0, sizeof(touched));
  rangeCtx rc = {touched, 0};

  TEST_ASSERT_EQUAL_INT(
      0, gvizThreadPoolForRange(pool, 100, 900, 7, markRange, &rc));
  for (size_t i = 0; i < 1024; i++)
    TEST_ASSERT_EQUAL_UINT8(i >= 100 && i < 900 ? 1 : 0, touched[i]);

  TEST_ASSERT_EQUAL_INT(
      0, gvizThreadPoolForRange(pool, 900, 900, 7, markRange, &rc));
  TEST_ASSERT_EQUAL_INT(-1,
                        gvizThreadPoolForRange(pool, 0, 10, 1, NULL, NULL));

  gvizThreadPoolDestroy(pool);
}

void test_forRange_reusable_across_calls(void) {
  gvizThreadPool *pool = gvizThreadPoolCreate(4);
  TEST_ASSERT_NOT_NULL(pool);

  for (size_t round = 0; round < 50; round++)
    checkForRangeCoversAll(pool, 32);

  gvizThreadPoolDestroy(pool);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_create_and_destroy);
  RUN_TEST(test_create_default_thread_count);
  RUN_TEST(test_null_safety);
  RUN_TEST(test_submit_runs_all_tasks);
  RUN_TEST(test_submit_passes_argument);
  RUN_TEST(test_destroy_drains_pending_tasks);
  RUN_TEST(test_forRange_covers_range_exactly_once);
  RUN_TEST(test_forRange_null_pool_runs_serially);
  RUN_TEST(test_forRange_subrange_and_empty);
  RUN_TEST(test_forRange_reusable_across_calls);
  return UNITY_END();
}
