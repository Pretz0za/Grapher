// Port of tests/core/gvizThreadPoolTests.c against gviz::ThreadPool.
//
// Two scenarios from the old suite have no analog here and are intentionally
// dropped rather than force-fitted: test_null_safety and
// test_forRange_null_pool_runs_serially exercised passing a NULL
// gvizThreadPool* to every function. gviz::ThreadPool is a real RAII object
// now -- construction either succeeds (a fully-usable pool) or throws
// (nothing to call methods on), so there is no "null pool" state reachable
// through the public API to exercise. WorkerSlot()'s NULL-pool case is
// dropped for the same reason; its "calling thread gets ThreadCount()" case
// is still covered by test_workerSlot_boundsAndNullSafety below.

#include "ThreadPool.hpp"

#include "unity/unity.h"

#include <atomic>
#include <vector>

#define RANGE_N 100000

void setUp(void) {}
void tearDown(void) {}

static std::atomic<size_t> taskCounter;

static void checkForRangeCoversAll(gviz::ThreadPool &pool, size_t grain) {
  std::vector<unsigned char> touched(RANGE_N, 0);
  std::atomic<size_t> sliceCalls{0};

  pool.ForRange(0, RANGE_N, grain,
                [&](size_t begin, size_t end) {
                  sliceCalls.fetch_add(1);
                  for (size_t i = begin; i < end; i++)
                    touched[i]++;
                });

  for (size_t i = 0; i < RANGE_N; i++)
    TEST_ASSERT_EQUAL_UINT8(1, touched[i]);
}

static void test_create_and_destroy(void) {
  gviz::ThreadPool pool(4);
  TEST_ASSERT_EQUAL_UINT64(4, pool.ThreadCount());
}

static void test_create_default_thread_count(void) {
  gviz::ThreadPool pool(0);
  TEST_ASSERT_GREATER_THAN(0, pool.ThreadCount());
}

static void test_submit_runs_all_tasks(void) {
  gviz::ThreadPool pool(4);

  taskCounter.store(0);
  for (size_t i = 0; i < 1000; i++)
    pool.Submit([] { taskCounter.fetch_add(1); });

  pool.Wait();
  TEST_ASSERT_EQUAL_UINT64(1000, taskCounter.load());
}

static void test_submit_passes_argument(void) {
  gviz::ThreadPool pool(2);

  taskCounter.store(0);
  size_t args[8] = {1, 2, 3, 4, 5, 6, 7, 8};
  for (size_t i = 0; i < 8; i++) {
    size_t value = args[i];
    pool.Submit([value] { taskCounter.fetch_add(value); });
  }

  pool.Wait();
  TEST_ASSERT_EQUAL_UINT64(36, taskCounter.load());
}

static void test_destroy_drains_pending_tasks(void) {
  taskCounter.store(0);
  {
    gviz::ThreadPool pool(2);
    for (size_t i = 0; i < 500; i++)
      pool.Submit([] { taskCounter.fetch_add(1); });
  } // ~ThreadPool() drains and joins here.
  TEST_ASSERT_EQUAL_UINT64(500, taskCounter.load());
}

static void test_forRange_covers_range_exactly_once(void) {
  gviz::ThreadPool pool(8);

  checkForRangeCoversAll(pool, 1);
  checkForRangeCoversAll(pool, 64);
  checkForRangeCoversAll(pool, RANGE_N + 1); // single serial slice
  checkForRangeCoversAll(pool, 0);           // grain 0 -> grain 1
}

static void test_forRange_subrange_and_empty(void) {
  gviz::ThreadPool pool(4);

  std::vector<unsigned char> touched(1024, 0);

  pool.ForRange(100, 900, 7, [&](size_t begin, size_t end) {
    for (size_t i = begin; i < end; i++)
      touched[i]++;
  });
  for (size_t i = 0; i < 1024; i++)
    TEST_ASSERT_EQUAL_UINT8(i >= 100 && i < 900 ? 1 : 0, touched[i]);

  bool emptyRangeCallbackRan = false;
  pool.ForRange(900, 900, 7,
                [&](size_t, size_t) { emptyRangeCallbackRan = true; });
  TEST_ASSERT_FALSE(emptyRangeCallbackRan);

  // Empty std::function is the new "task is NULL" case: silent no-op, no
  // exception, no crash.
  pool.ForRange(0, 10, 1, std::function<void(size_t, size_t)>());
}

static void test_forRange_reusable_across_calls(void) {
  gviz::ThreadPool pool(4);

  for (size_t round = 0; round < 50; round++)
    checkForRangeCoversAll(pool, 32);
}

struct SlotCtx {
  const gviz::ThreadPool *pool;
  std::atomic<int> sawInvalidSlot{0};
  size_t threadCount;
};

static void test_workerSlot_boundsAndNullSafety(void) {
  gviz::ThreadPool pool(3);

  // Calling thread (not a worker) gets slot == threadCount.
  TEST_ASSERT_EQUAL_size_t(3, pool.WorkerSlot());

  SlotCtx sc{&pool, {0}, pool.ThreadCount()};
  pool.ForRange(0, 1000, 1, [&](size_t, size_t) {
    size_t slot = sc.pool->WorkerSlot();
    // Workers get 0..threadCount-1; the calling thread gets threadCount.
    if (slot > sc.threadCount)
      sc.sawInvalidSlot.store(1);
  });
  TEST_ASSERT_EQUAL_INT(0, sc.sawInvalidSlot.load());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_create_and_destroy);
  RUN_TEST(test_create_default_thread_count);
  RUN_TEST(test_submit_runs_all_tasks);
  RUN_TEST(test_submit_passes_argument);
  RUN_TEST(test_destroy_drains_pending_tasks);
  RUN_TEST(test_forRange_covers_range_exactly_once);
  RUN_TEST(test_forRange_subrange_and_empty);
  RUN_TEST(test_forRange_reusable_across_calls);
  RUN_TEST(test_workerSlot_boundsAndNullSafety);
  return UNITY_END();
}
