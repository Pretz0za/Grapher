#include "dsa/gvizArray.h"
#include "unity/unity.h"
#include "utils/serializers.h"
#include <stdlib.h>
#include <string.h>

/**
 * Test fixture setup and teardown functions for gvizArray tests.
 */

void setUp(void) {
  // Called before each test
  //
}

void tearDown(void) {
  // Called after each test
}

// ============================================================================
// BASIC INITIALIZATION TESTS
// ============================================================================

void test_gvizArrayInit_WithValidPointer(void) {
  gvizArray arr;
  int result = gvizArrayInit(&arr, sizeof(int));

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_NOT_NULL(arr.arr);
  TEST_ASSERT_EQUAL_UINT64(sizeof(int), arr.elementSize);
  TEST_ASSERT_EQUAL_UINT64(0, arr.count);
  TEST_ASSERT_TRUE(arr.capacity > 0);

  gvizArrayRelease(&arr);
}

void test_gvizArrayInit_WithNullPointer(void) {
  // Attempting to initialize a NULL pointer should fail
  int result = gvizArrayInit(NULL, sizeof(int));
  TEST_ASSERT_EQUAL_INT(-1, result);
}

void test_gvizArrayInitAtCapacity_WithSpecificCapacity(void) {
  gvizArray arr;
  size_t initialCapacity = 100;
  int result = gvizArrayInitAtCapacity(&arr, sizeof(double), initialCapacity);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_NOT_NULL(arr.arr);
  TEST_ASSERT_EQUAL_UINT64(initialCapacity, arr.capacity);
  TEST_ASSERT_EQUAL_UINT64(0, arr.count);

  gvizArrayRelease(&arr);
}

void test_gvizArrayIsEmpty_OnNewArray(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int isEmpty = gvizArrayIsEmpty(&arr);
  TEST_ASSERT_EQUAL_INT(1, isEmpty);

  gvizArrayRelease(&arr);
}

// ============================================================================
// PUSH/POP BASIC TESTS
// ============================================================================

void test_gvizArrayPush_SingleInteger(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int value = 42;
  int result = gvizArrayPush(&arr, &value);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(1, arr.count);

  // Verify the value was stored correctly
  int *retrieved = (int *)gvizArrayAtIndex(&arr, 0);
  TEST_ASSERT_EQUAL_INT(42, *retrieved);

  gvizArrayRelease(&arr);
}

void test_gvizArrayPush_MultipleIntegers(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {10, 20, 30, 40, 50};
  for (int i = 0; i < 5; i++) {
    int result = gvizArrayPush(&arr, &values[i]);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  TEST_ASSERT_EQUAL_UINT64(5, arr.count);

  // Verify all values were stored correctly
  for (int i = 0; i < 5; i++) {
    int *retrieved = (int *)gvizArrayAtIndex(&arr, i);
    TEST_ASSERT_EQUAL_INT(values[i], *retrieved);
  }

  gvizArrayRelease(&arr);
}

void test_gvizArrayPop_SingleElement(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int value = 42;
  gvizArrayPush(&arr, &value);

  int popped = 0;
  int result = gvizArrayPop(&arr, &popped);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_INT(42, popped);
  TEST_ASSERT_EQUAL_UINT64(0, arr.count);

  gvizArrayRelease(&arr);
}

void test_gvizArrayPop_EmptyArray(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int popped = 0;
  int result = gvizArrayPop(&arr, &popped);

  // Pop on empty array should fail
  TEST_ASSERT_EQUAL_INT(-1, result);

  gvizArrayRelease(&arr);
}

void test_gvizArrayPop_MultipleElements(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  // Pop in LIFO order
  int popped = 0;
  gvizArrayPop(&arr, &popped);
  TEST_ASSERT_EQUAL_INT(30, popped);
  TEST_ASSERT_EQUAL_UINT64(2, arr.count);

  gvizArrayPop(&arr, &popped);
  TEST_ASSERT_EQUAL_INT(20, popped);
  TEST_ASSERT_EQUAL_UINT64(1, arr.count);

  gvizArrayPop(&arr, &popped);
  TEST_ASSERT_EQUAL_INT(10, popped);
  TEST_ASSERT_EQUAL_UINT64(0, arr.count);

  gvizArrayRelease(&arr);
}

// ============================================================================
// FIND TESTS
// ============================================================================

void test_gvizArrayFindOne_ElementExists(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {100, 200, 300, 400};
  for (int i = 0; i < 4; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int searchValue = 300;
  int index = gvizArrayFindOne(&arr, &searchValue);

  TEST_ASSERT_EQUAL_INT(2, index);

  gvizArrayRelease(&arr);
}

void test_gvizArrayFindOne_ElementNotFound(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {100, 200, 300};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int searchValue = 999;
  int index = gvizArrayFindOne(&arr, &searchValue);

  TEST_ASSERT_EQUAL_INT(-1, index);

  gvizArrayRelease(&arr);
}

void test_gvizArrayFindOne_FirstOfDuplicates(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {100, 200, 200, 300};
  for (int i = 0; i < 4; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int searchValue = 200;
  int index = gvizArrayFindOne(&arr, &searchValue);

  // Should return the first occurrence
  TEST_ASSERT_EQUAL_INT(1, index);

  gvizArrayRelease(&arr);
}

// ============================================================================
// DELETE TESTS
// ============================================================================

void test_gvizArrayFindOneAndDelete_ElementExists(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {10, 20, 30, 40};
  for (int i = 0; i < 4; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int deleteValue = 30;
  int index = gvizArrayFindOneAndDelete(&arr, &deleteValue);

  TEST_ASSERT_EQUAL_INT(2, index);
  TEST_ASSERT_EQUAL_UINT64(3, arr.count);

  // Verify remaining elements are correct
  int *elem0 = (int *)gvizArrayAtIndex(&arr, 0);
  int *elem1 = (int *)gvizArrayAtIndex(&arr, 1);
  int *elem2 = (int *)gvizArrayAtIndex(&arr, 2);

  TEST_ASSERT_EQUAL_INT(10, *elem0);
  TEST_ASSERT_EQUAL_INT(20, *elem1);
  TEST_ASSERT_EQUAL_INT(40, *elem2);

  gvizArrayRelease(&arr);
}

void test_gvizArrayFindOneAndDelete_ElementNotFound(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int deleteValue = 999;
  int index = gvizArrayFindOneAndDelete(&arr, &deleteValue);

  TEST_ASSERT_EQUAL_INT(-1, index);
  TEST_ASSERT_EQUAL_UINT64(3, arr.count);

  gvizArrayRelease(&arr);
}

void test_gvizArrayFindOneAndDelete_DeleteFirst(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int deleteValue = 10;
  int index = gvizArrayFindOneAndDelete(&arr, &deleteValue);

  TEST_ASSERT_EQUAL_INT(0, index);
  TEST_ASSERT_EQUAL_UINT64(2, arr.count);

  int *elem0 = (int *)gvizArrayAtIndex(&arr, 0);
  TEST_ASSERT_EQUAL_INT(20, *elem0);

  gvizArrayRelease(&arr);
}

void test_gvizArrayFindOneAndDelete_DeleteLast(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  int deleteValue = 30;
  int index = gvizArrayFindOneAndDelete(&arr, &deleteValue);

  TEST_ASSERT_EQUAL_INT(2, index);
  TEST_ASSERT_EQUAL_UINT64(2, arr.count);

  gvizArrayRelease(&arr);
}

// ============================================================================
// COPY TESTS
// ============================================================================

void test_gvizArrayCopy_Basic(void) {
  gvizArray src, dest;
  gvizArrayInit(&src, sizeof(int));
  gvizArrayInit(&dest, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&src, &values[i]);
  }

  int result = gvizArrayCopy(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(3, dest.count);

  // Verify values are copied (shallow copy)
  for (int i = 0; i < 3; i++) {
    int *srcElem = (int *)gvizArrayAtIndex(&src, i);
    int *destElem = (int *)gvizArrayAtIndex(&dest, i);
    TEST_ASSERT_EQUAL_INT(*srcElem, *destElem);
  }

  gvizArrayRelease(&src);
  gvizArrayRelease(&dest);
}

void test_gvizArrayClone_Basic(void) {
  gvizArray src, dest;
  gvizArrayInit(&src, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&src, &values[i]);
  }

  int result = gvizArrayClone(&dest, &src);

  TEST_ASSERT_EQUAL_INT(0, result);
  TEST_ASSERT_EQUAL_UINT64(3, dest.count);
  TEST_ASSERT_EQUAL_UINT64(sizeof(int), dest.elementSize);

  // Verify values are cloned
  for (int i = 0; i < 3; i++) {
    int *srcElem = (int *)gvizArrayAtIndex(&src, i);
    int *destElem = (int *)gvizArrayAtIndex(&dest, i);
    TEST_ASSERT_EQUAL_INT(*srcElem, *destElem);
  }

  gvizArrayRelease(&src);
  gvizArrayRelease(&dest);
}

void test_gvizArrayMove_SrcInvalidAfterMove(void) {
  gvizArray src, dest;
  gvizArrayInit(&src, sizeof(int));

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&src, &values[i]);
  }

  // Store original src pointer for verification
  void *originalArr = src.arr;

  gvizArrayMove(&dest, &src);

  // After move, dest should own the memory
  TEST_ASSERT_EQUAL_UINT64(3, dest.count);
  TEST_ASSERT_EQUAL_PTR(originalArr, dest.arr);

  gvizArrayRelease(&dest);
  // Don't release src since it's invalid after move
}

// ============================================================================
// CAPACITY AND REALLOCATION TESTS
// ============================================================================

void test_gvizArray_AutomaticReallocation(void) {
  gvizArray arr;
  gvizArrayInitAtCapacity(&arr, sizeof(int), 4);

  TEST_ASSERT_EQUAL_UINT64(4, arr.capacity);

  // Push more elements than initial capacity
  for (int i = 0; i < 10; i++) {
    int result = gvizArrayPush(&arr, &i);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  TEST_ASSERT_EQUAL_UINT64(10, arr.count);
  TEST_ASSERT_TRUE(arr.capacity >= 10);

  // Verify all elements are intact
  for (int i = 0; i < 10; i++) {
    int *elem = (int *)gvizArrayAtIndex(&arr, i);
    TEST_ASSERT_EQUAL_INT(i, *elem);
  }

  gvizArrayRelease(&arr);
}

// ============================================================================
// STRESS TESTS - EXTREME SIZES
// ============================================================================

void test_gvizArray_LargeNumberOfElements(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int largeSize = 10000;

  // Push many elements
  for (int i = 0; i < largeSize; i++) {
    int result = gvizArrayPush(&arr, &i);
    TEST_ASSERT_EQUAL_INT(0, result);
  }

  TEST_ASSERT_EQUAL_UINT64(largeSize, arr.count);

  // Verify random access works
  int *elem100 = (int *)gvizArrayAtIndex(&arr, 100);
  TEST_ASSERT_EQUAL_INT(100, *elem100);

  int *elem5000 = (int *)gvizArrayAtIndex(&arr, 5000);
  TEST_ASSERT_EQUAL_INT(5000, *elem5000);

  int *elemLast = (int *)gvizArrayAtIndex(&arr, largeSize - 1);
  TEST_ASSERT_EQUAL_INT(largeSize - 1, *elemLast);

  gvizArrayRelease(&arr);
}

void test_gvizArray_LargeElementSize(void) {
  // Test with a large element size (e.g., a struct)
  typedef struct {
    int id;
    char name[256];
    double values[10];
  } LargeStruct;

  gvizArray arr;
  gvizArrayInit(&arr, sizeof(LargeStruct));

  LargeStruct elem = {.id = 42};
  strcpy(elem.name, "test_element");

  int result = gvizArrayPush(&arr, &elem);
  TEST_ASSERT_EQUAL_INT(0, result);

  LargeStruct *retrieved = (LargeStruct *)gvizArrayAtIndex(&arr, 0);
  TEST_ASSERT_EQUAL_INT(42, retrieved->id);
  TEST_ASSERT_EQUAL_STRING("test_element", retrieved->name);

  gvizArrayRelease(&arr);
}

void test_gvizArray_RepeatedPushPopCycles(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int cycles = 1000;

  for (int cycle = 0; cycle < cycles; cycle++) {
    int value = cycle;
    gvizArrayPush(&arr, &value);

    int popped;
    int result = gvizArrayPop(&arr, &popped);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(cycle, popped);
  }

  TEST_ASSERT_EQUAL_UINT64(0, arr.count);

  gvizArrayRelease(&arr);
}

void test_gvizArray_DeleteFromLargeArray(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(int));

  int size = 1000;
  for (int i = 0; i < size; i++) {
    gvizArrayPush(&arr, &i);
  }

  // Delete elements at various positions
  int deleteValues[] = {0, 500, 999};

  for (int i = 0; i < 3; i++) {
    int idx = gvizArrayFindOneAndDelete(&arr, &deleteValues[i]);
    TEST_ASSERT_TRUE(idx >= 0);
  }

  TEST_ASSERT_EQUAL_UINT64(size - 3, arr.count);

  gvizArrayRelease(&arr);
}

// ============================================================================
// DIFFERENT DATA TYPES
// ============================================================================

void test_gvizArray_WithDoubles(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(double));

  double values[] = {3.14, 2.71, 1.41};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &values[i]);
  }

  double *elem = (double *)gvizArrayAtIndex(&arr, 0);
  TEST_ASSERT_DOUBLE_WITHIN(0.001, 3.14, *elem);

  gvizArrayRelease(&arr);
}

void test_gvizArray_WithPointers(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(void *));

  int val1 = 100, val2 = 200, val3 = 300;
  int *ptr1 = &val1, *ptr2 = &val2, *ptr3 = &val3;

  gvizArrayPush(&arr, &ptr1);
  gvizArrayPush(&arr, &ptr2);
  gvizArrayPush(&arr, &ptr3);

  int **retrieved = (int **)gvizArrayAtIndex(&arr, 1);
  TEST_ASSERT_EQUAL_INT(200, **retrieved);

  gvizArrayRelease(&arr);
}

void test_gvizArray_WithStructs(void) {
  typedef struct {
    int x;
    int y;
  } Point;

  gvizArray arr;
  gvizArrayInit(&arr, sizeof(Point));

  Point points[] = {{1, 2}, {3, 4}, {5, 6}};
  for (int i = 0; i < 3; i++) {
    gvizArrayPush(&arr, &points[i]);
  }

  Point *retrieved = (Point *)gvizArrayAtIndex(&arr, 1);
  TEST_ASSERT_EQUAL_INT(3, retrieved->x);
  TEST_ASSERT_EQUAL_INT(4, retrieved->y);

  gvizArrayRelease(&arr);
}

void test_gvizArray_PrintUINT64(void) {
  gvizArray arr;
  gvizArrayInit(&arr, sizeof(size_t));
  size_t elems[5] = {1, 1, 1, 1, 1};

  char buf[8];
  for (int i = 0; i < 5; i++) {
    gvizArrayPush(&arr, elems + i);
    gvizSerializeUINT64(elems + i, buf, 8);
    TEST_ASSERT_EQUAL_STRING("1", buf);
  }

  gvizArrayPrint(&arr, stdout, gvizSerializeUINT64, 8);

  gvizArrayRelease(&arr);
}

int main(void) {

  UNITY_BEGIN();

  RUN_TEST(test_gvizArrayInit_WithValidPointer);
  RUN_TEST(test_gvizArrayInit_WithNullPointer);
  RUN_TEST(test_gvizArrayInitAtCapacity_WithSpecificCapacity);
  RUN_TEST(test_gvizArrayIsEmpty_OnNewArray);
  RUN_TEST(test_gvizArrayPush_SingleInteger);
  RUN_TEST(test_gvizArrayPush_MultipleIntegers);
  RUN_TEST(test_gvizArrayPop_SingleElement);
  RUN_TEST(test_gvizArrayPop_EmptyArray);
  RUN_TEST(test_gvizArrayPop_MultipleElements);
  RUN_TEST(test_gvizArrayFindOne_ElementExists);
  RUN_TEST(test_gvizArrayFindOne_ElementNotFound);
  RUN_TEST(test_gvizArrayFindOne_FirstOfDuplicates);
  RUN_TEST(test_gvizArrayFindOneAndDelete_ElementExists);
  RUN_TEST(test_gvizArrayFindOneAndDelete_ElementNotFound);
  RUN_TEST(test_gvizArrayFindOneAndDelete_DeleteFirst);
  RUN_TEST(test_gvizArrayFindOneAndDelete_DeleteLast);
  RUN_TEST(test_gvizArrayCopy_Basic);
  RUN_TEST(test_gvizArrayClone_Basic);
  RUN_TEST(test_gvizArrayMove_SrcInvalidAfterMove);
  RUN_TEST(test_gvizArray_AutomaticReallocation);
  RUN_TEST(test_gvizArray_LargeNumberOfElements);
  RUN_TEST(test_gvizArray_LargeElementSize);
  RUN_TEST(test_gvizArray_RepeatedPushPopCycles);
  RUN_TEST(test_gvizArray_DeleteFromLargeArray);
  RUN_TEST(test_gvizArray_WithDoubles);
  RUN_TEST(test_gvizArray_WithPointers);
  RUN_TEST(test_gvizArray_WithStructs);
  RUN_TEST(test_gvizArray_PrintUINT64);

  int result = UNITY_END();

  printf("\n");

  return result;
}
