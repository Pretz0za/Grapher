#include "dsa/gvizDeque.h"
#include "unity/unity.h"
#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

// ============================================================================
// INITIALIZATION
// ============================================================================

void test_dequeInit_int(void) {
  gvizDeque d;
  TEST_ASSERT_EQUAL_INT(0, gvizDequeInit(&d, sizeof(int)));
  TEST_ASSERT_NOT_NULL(d.arr);
  TEST_ASSERT_EQUAL_INT(sizeof(int), d.elementSize);
  TEST_ASSERT_GREATER_THAN(0, d.capacity);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeIsEmpty(&d));

  gvizDequeRelease(&d);
}

void test_dequeInit_double(void) {
  gvizDeque d;
  TEST_ASSERT_EQUAL_INT(0, gvizDequeInit(&d, sizeof(double)));
  TEST_ASSERT_EQUAL_INT(sizeof(double), d.elementSize);

  gvizDequeRelease(&d);
}

void test_dequeInit_struct(void) {
  typedef struct {
    int x;
    int y;
  } Point;
  gvizDeque d;

  TEST_ASSERT_EQUAL_INT(0, gvizDequeInit(&d, sizeof(Point)));
  TEST_ASSERT_EQUAL_INT(sizeof(Point), d.elementSize);

  gvizDequeRelease(&d);
}

void test_dequeInitAtCapacity(void) {
  gvizDeque d;
  TEST_ASSERT_EQUAL_INT(0, gvizDequeInitAtCapacity(&d, sizeof(int), 32));
  TEST_ASSERT_GREATER_OR_EQUAL(d.capacity, 32);

  gvizDequeRelease(&d);
}

void test_dequeSize_empty(void) {
  gvizDeque d;
  gvizDequeInit(&d, sizeof(int));

  TEST_ASSERT_EQUAL_INT(0, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

// ============================================================================
// PUSH & PEEK
// ============================================================================

void test_dequePush_single_int(void) {
  gvizDeque d;
  int val = 42;

  gvizDequeInit(&d, sizeof(int));
  TEST_ASSERT_EQUAL_INT(0, gvizDequePush(&d, &val));
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));
  TEST_ASSERT_EQUAL_INT(0, gvizDequeIsEmpty(&d));

  gvizDequeRelease(&d);
}

void test_dequePush_multiple(void) {
  gvizDeque d;
  int vals[] = {10, 20, 30, 40, 50};

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 5; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  TEST_ASSERT_EQUAL_INT(5, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_dequePush_capacity_growth(void) {
  gvizDeque d;
  int val = 1;

  gvizDequeInitAtCapacity(&d, sizeof(int), 2);
  size_t initial_capacity = d.capacity;

  gvizDequePush(&d, &val);
  gvizDequePush(&d, &val);
  gvizDequePush(&d, &val);

  TEST_ASSERT_EQUAL_INT(3, gvizDequeSize(&d));
  TEST_ASSERT_GREATER_THAN(initial_capacity, d.capacity);

  gvizDequeRelease(&d);
}

void test_dequePeekLeft_single(void) {
  gvizDeque d;
  int val = 99;

  gvizDequeInit(&d, sizeof(int));
  gvizDequePush(&d, &val);

  int *peeked = (int *)gvizDequePeekLeft(&d);
  TEST_ASSERT_NOT_NULL(peeked);
  TEST_ASSERT_EQUAL_INT(99, *peeked);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_dequePeekRight_single(void) {
  gvizDeque d;
  int val = 77;

  gvizDequeInit(&d, sizeof(int));
  gvizDequePush(&d, &val);

  int *peeked = (int *)gvizDequePeekRight(&d);
  TEST_ASSERT_NOT_NULL(peeked);
  TEST_ASSERT_EQUAL_INT(77, *peeked);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_dequePeekLeft_multiple(void) {
  gvizDeque d;
  int vals[] = {1, 2, 3};

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  int *left = (int *)gvizDequePeekLeft(&d);
  TEST_ASSERT_EQUAL_INT(1, *left);

  gvizDequeRelease(&d);
}

void test_dequePeekRight_multiple(void) {
  gvizDeque d;
  int vals[] = {10, 20, 30};

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  int *right = (int *)gvizDequePeekRight(&d);
  TEST_ASSERT_EQUAL_INT(30, *right);

  gvizDequeRelease(&d);
}

void test_dequePeekLeft_empty(void) {
  gvizDeque d;
  gvizDequeInit(&d, sizeof(int));

  TEST_ASSERT_NULL(gvizDequePeekLeft(&d));

  gvizDequeRelease(&d);
}

void test_dequePeekRight_empty(void) {
  gvizDeque d;
  gvizDequeInit(&d, sizeof(int));

  TEST_ASSERT_NULL(gvizDequePeekRight(&d));

  gvizDequeRelease(&d);
}

// ============================================================================
// POP
// ============================================================================

void test_dequePopLeft_single(void) {
  gvizDeque d;
  int val = 55;
  int res;

  gvizDequeInit(&d, sizeof(int));
  gvizDequePush(&d, &val);

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(55, res);
  TEST_ASSERT_EQUAL_INT(0, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_dequePopRight_single(void) {
  gvizDeque d;
  int val = 88;
  int res;

  gvizDequeInit(&d, sizeof(int));
  gvizDequePush(&d, &val);

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(88, res);
  TEST_ASSERT_EQUAL_INT(0, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_dequePopLeft_multiple(void) {
  gvizDeque d;
  int vals[] = {5, 10, 15};
  int res;

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(5, res);
  TEST_ASSERT_EQUAL_INT(2, gvizDequeSize(&d));

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(10, res);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_dequePopRight_multiple(void) {
  gvizDeque d;
  int vals[] = {100, 200, 300};
  int res;

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(300, res);
  TEST_ASSERT_EQUAL_INT(2, gvizDequeSize(&d));

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(200, res);

  gvizDequeRelease(&d);
}

void test_dequePopLeft_and_PopRight_mixed(void) {
  gvizDeque d;
  int vals[] = {1, 2, 3, 4, 5};
  int res;

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 5; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(1, res);

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(5, res);

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(2, res);

  TEST_ASSERT_EQUAL_INT(2, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

// ============================================================================
// AT INDEX
// ============================================================================

void test_dequeAtIndex_valid(void) {
  gvizDeque d;
  int vals[] = {11, 22, 33, 44};

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 4; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  int *elem0 = (int *)gvizDequeAtIndex(&d, 0);
  int *elem2 = (int *)gvizDequeAtIndex(&d, 2);
  int *elem3 = (int *)gvizDequeAtIndex(&d, 3);

  TEST_ASSERT_EQUAL_INT(11, *elem0);
  TEST_ASSERT_EQUAL_INT(33, *elem2);
  TEST_ASSERT_EQUAL_INT(44, *elem3);

  gvizDequeRelease(&d);
}

void test_dequeAtIndex_out_of_bounds(void) {
  gvizDeque d;
  int val = 5;

  gvizDequeInit(&d, sizeof(int));
  gvizDequePush(&d, &val);

  TEST_ASSERT_NULL(gvizDequeAtIndex(&d, 1));
  TEST_ASSERT_NULL(gvizDequeAtIndex(&d, 100));

  gvizDequeRelease(&d);
}

void test_dequeAtIndex_empty(void) {
  gvizDeque d;
  gvizDequeInit(&d, sizeof(int));

  TEST_ASSERT_NULL(gvizDequeAtIndex(&d, 0));

  gvizDequeRelease(&d);
}

// ============================================================================
// DIFFERENT DATA TYPES
// ============================================================================

void test_deque_double(void) {
  gvizDeque d;
  double vals[] = {1.5, 2.7, 3.14};

  gvizDequeInit(&d, sizeof(double));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  double *peek = (double *)gvizDequePeekLeft(&d);
  TEST_ASSERT_EQUAL_DOUBLE(1.5, *peek);

  double res;
  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_DOUBLE(3.14, res);

  gvizDequeRelease(&d);
}

void test_deque_struct(void) {
  typedef struct {
    int id;
    char name[16];
  } Record;
  gvizDeque d;

  Record r1 = {1, "Alice"};
  Record r2 = {2, "Bob"};

  gvizDequeInit(&d, sizeof(Record));
  gvizDequePush(&d, &r1);
  gvizDequePush(&d, &r2);

  Record *peek = (Record *)gvizDequePeekLeft(&d);
  TEST_ASSERT_EQUAL_INT(1, peek->id);
  TEST_ASSERT_EQUAL_STRING("Alice", peek->name);

  Record res;
  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(2, res.id);
  TEST_ASSERT_EQUAL_STRING("Bob", res.name);

  gvizDequeRelease(&d);
}

void test_deque_char(void) {
  gvizDeque d;
  char chars[] = {'a', 'b', 'c', 'd'};

  gvizDequeInit(&d, sizeof(char));
  for (int i = 0; i < 4; i++) {
    gvizDequePush(&d, &chars[i]);
  }

  char *left = (char *)gvizDequePeekLeft(&d);
  TEST_ASSERT_EQUAL_CHAR('a', *left);

  char res;
  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_CHAR('a', res);

  gvizDequeRelease(&d);
}

void test_deque_pointer(void) {
  gvizDeque d;
  int a = 10, b = 20;
  int *ptrs[] = {&a, &b};

  gvizDequeInit(&d, sizeof(int *));
  for (int i = 0; i < 2; i++) {
    gvizDequePush(&d, &ptrs[i]);
  }

  int **p = (int **)gvizDequePeekLeft(&d);
  TEST_ASSERT_EQUAL_INT(10, **p);

  gvizDequeRelease(&d);
}

// ============================================================================
// QUEUE BEHAVIOR (FIFO)
// ============================================================================

void test_deque_as_queue(void) {
  gvizDeque d;
  int vals[] = {1, 2, 3, 4, 5};
  int res;

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 5; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(1, res);
  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(2, res);
  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(3, res);

  gvizDequeRelease(&d);
}

// ============================================================================
// STACK BEHAVIOR (LIFO)
// ============================================================================

void test_deque_as_stack(void) {
  gvizDeque d;
  int vals[] = {10, 20, 30};
  int res;

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(30, res);
  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(20, res);
  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(10, res);

  gvizDequeRelease(&d);
}

// ============================================================================
// ALTERNATING OPERATIONS
// ============================================================================

void test_deque_alternating_push_pop(void) {
  gvizDeque d;
  int res;

  gvizDequeInit(&d, sizeof(int));

  int v1 = 1;
  gvizDequePush(&d, &v1);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));

  int v2 = 2;
  gvizDequePush(&d, &v2);
  TEST_ASSERT_EQUAL_INT(2, gvizDequeSize(&d));

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(1, res);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));

  int v3 = 3;
  gvizDequePush(&d, &v3);
  TEST_ASSERT_EQUAL_INT(2, gvizDequeSize(&d));

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(3, res);
  TEST_ASSERT_EQUAL_INT(1, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_deque_empty_after_all_pops(void) {
  gvizDeque d;
  int vals[] = {7, 8, 9};
  int res;

  gvizDequeInit(&d, sizeof(int));
  for (int i = 0; i < 3; i++) {
    gvizDequePush(&d, &vals[i]);
  }

  gvizDequePopLeft(&d, &res);
  gvizDequePopLeft(&d, &res);
  gvizDequePopLeft(&d, &res);

  TEST_ASSERT_EQUAL_INT(0, gvizDequeSize(&d));
  TEST_ASSERT_EQUAL_INT(1, gvizDequeIsEmpty(&d));

  gvizDequeRelease(&d);
}

// ============================================================================
// WRAPAROUND & CIRCULAR BEHAVIOR
// ============================================================================

void test_deque_wraparound_left_pops(void) {
  gvizDeque d;
  int res;

  gvizDequeInitAtCapacity(&d, sizeof(int), 4);

  for (int i = 1; i <= 6; i++) {
    gvizDequePush(&d, &i);
  }

  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(1, res);
  gvizDequePopLeft(&d, &res);
  TEST_ASSERT_EQUAL_INT(2, res);

  for (int i = 7; i <= 8; i++) {
    gvizDequePush(&d, &i);
  }

  TEST_ASSERT_EQUAL_INT(6, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

void test_deque_wraparound_right_pops(void) {
  gvizDeque d;
  int res;

  gvizDequeInitAtCapacity(&d, sizeof(int), 4);

  for (int i = 10; i <= 15; i++) {
    gvizDequePush(&d, &i);
  }

  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(15, res);
  gvizDequePopRight(&d, &res);
  TEST_ASSERT_EQUAL_INT(14, res);

  for (int i = 16; i <= 17; i++) {
    gvizDequePush(&d, &i);
  }

  TEST_ASSERT_EQUAL_INT(6, gvizDequeSize(&d));

  gvizDequeRelease(&d);
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_dequeInit_int);
  RUN_TEST(test_dequeInit_double);
  RUN_TEST(test_dequeInit_struct);
  RUN_TEST(test_dequeInitAtCapacity);
  RUN_TEST(test_dequeSize_empty);

  RUN_TEST(test_dequePush_single_int);
  RUN_TEST(test_dequePush_multiple);
  RUN_TEST(test_dequePush_capacity_growth);

  RUN_TEST(test_dequePeekLeft_single);
  RUN_TEST(test_dequePeekRight_single);
  RUN_TEST(test_dequePeekLeft_multiple);
  RUN_TEST(test_dequePeekRight_multiple);
  RUN_TEST(test_dequePeekLeft_empty);
  RUN_TEST(test_dequePeekRight_empty);

  RUN_TEST(test_dequePopLeft_single);
  RUN_TEST(test_dequePopRight_single);
  RUN_TEST(test_dequePopLeft_multiple);
  RUN_TEST(test_dequePopRight_multiple);
  RUN_TEST(test_dequePopLeft_and_PopRight_mixed);

  RUN_TEST(test_dequeAtIndex_valid);
  RUN_TEST(test_dequeAtIndex_out_of_bounds);
  RUN_TEST(test_dequeAtIndex_empty);

  RUN_TEST(test_deque_double);
  RUN_TEST(test_deque_struct);
  RUN_TEST(test_deque_char);
  RUN_TEST(test_deque_pointer);

  RUN_TEST(test_deque_as_queue);
  RUN_TEST(test_deque_as_stack);

  RUN_TEST(test_deque_alternating_push_pop);
  RUN_TEST(test_deque_empty_after_all_pops);

  RUN_TEST(test_deque_wraparound_left_pops);
  RUN_TEST(test_deque_wraparound_right_pops);

  return UNITY_END();
}
