#ifndef _GVIZ_QUEUE_H_
#define _GVIZ_QUEUE_H_

#include <stddef.h>
typedef struct gvizDeque {
  void *arr;
  void *begin;
  size_t count;
  size_t capacity;
  size_t elementSize;
} gvizDeque;

/**
 * Initializes an empty deque with default capacity 8.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizDequeInit(gvizDeque *d, size_t elementSize);

/**
 * Like gvizDequeInit, but pre-allocates to @p initialCapacity elements.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizDequeInitAtCapacity(gvizDeque *d, size_t elementSize,
                            size_t initialCapacity);

/** Returns 1 if @p d is empty, 0 otherwise. */
int gvizDequeIsEmpty(const gvizDeque *d);

/** Returns the number of elements in @p d. */
size_t gvizDequeSize(const gvizDeque *d);

/** Returns a pointer to the element at @p idx, or NULL if out of bounds. */
void *gvizDequeAtIndex(const gvizDeque *d, size_t idx);

/** Removes and copies the leftmost element into @p res. @p d must be non-empty. */
void gvizDequePopLeft(gvizDeque *d, void *res);

/** Removes and copies the rightmost element into @p res. @p d must be non-empty. */
void gvizDequePopRight(gvizDeque *d, void *res);

/** Returns a pointer to the leftmost element, or NULL if empty. */
void *gvizDequePeekLeft(const gvizDeque *d);

/** Returns a pointer to the rightmost element, or NULL if empty. */
void *gvizDequePeekRight(const gvizDeque *d);

/**
 * Appends @p item to the right end of @p d, growing if necessary.
 *
 * @return 0 on success, -1 if @p d is uninitialized or reallocation fails.
 */
int gvizDequePush(gvizDeque *d, void *item);

/** Frees the backing array of @p d. */
void gvizDequeRelease(gvizDeque *d);

#endif
