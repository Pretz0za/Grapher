#include "dsa/gvizDeque.h"
#include "core/alloc.h"
#include <stdlib.h>
#include <string.h>

int gvizDequeInit(gvizDeque *d, size_t elementSize) {
  d->arr = GVIZ_ALLOC(elementSize * 8);
  if (!d->arr)
    return -1;

  d->capacity = 8;
  d->begin = d->arr;
  d->end = d->arr;
  d->elementSize = elementSize;

  return 0;
}
int gvizDequeInitAtCapacity(gvizDeque *d, size_t elementSize,
                            size_t initialCapacity) {
  d->arr = GVIZ_ALLOC(elementSize * initialCapacity);
  if (!d->arr)
    return -1;

  d->capacity = initialCapacity;
  d->begin = d->arr;
  d->end = d->arr;
  d->elementSize = elementSize;

  return 0;
}

size_t gvizDequeSize(const gvizDeque *d) {
  return d->end >= d->begin ? d->end - d->begin + 1
                            : d->capacity - (d->begin - d->end - 1);
}

int gvizDequeIsEmpty(const gvizDeque *d) { return d->begin == NULL; }

void *gvizDequeAtIndex(const gvizDeque *d, size_t idx) {
  size_t offset = (idx + (d->begin - d->arr)) % d->capacity;
  return (d->arr + (d->elementSize * offset));
}

void gvizDequePopLeft(gvizDeque *d, void *res) {
  memcpy(res, d->begin, d->elementSize);
  size_t idx = (d->begin - d->arr + 1) % d->capacity;
  d->begin = d->arr + idx;
  return 0;
};
int gvizDequePopRight(gvizDeque *d, void *res) {
  memcpy(res, d->end, d->elementSize);
  size_t idx = (d->end - d->arr - 1) % d->capacity;
  d->end = d->arr + idx;
  return 0;
};

void *gvizDequePeekLeft(const gvizDeque *d) { return d->begin; }
void *gvizDequePeekRight(const gvizDeque *d) { return d->end; }

int gvizDequePush(gvizDeque *d, void *item) {
  // resize if needed
  if (gvizDequeSize(d) == d->capacity) {
    void *newArr;
    size_t count = gvizDequeSize(d);
    if (d->end >= d->begin) {
      // simple case
      size_t tmp = d->begin - d->arr;
      newArr = GVIZ_REALLOC(d->arr, 2 * d->capacity * d->elementSize);
      if (!newArr)
        return -1;
      d->arr = newArr;
      d->begin = d->arr + tmp;
      d->end = d->begin + (d->elementSize * count);
    } else {
      // harder case with a loop around the deque
      newArr = GVIZ_ALLOC(2 * d->capacity * d->elementSize);
      if (!newArr)
        return -1;
      memcpy(newArr, d->begin, d->capacity - (d->begin - d->arr));
      memcpy(newArr + d->capacity - (d->begin - d->arr), d->arr,
             d->end - d->arr + 1);
      GVIZ_DEALLOC(d->arr);
      d->begin = newArr;
      d->end = d->begin + count;
      d->arr = newArr;
    }
    d->capacity = 2 * d->capacity;
  }

  size_t idx = (d->end - d->arr + 1) % d->capacity;
  d->end = d->arr + idx;
  memcpy(d->end, item, d->elementSize);
  return 0;
}

void gvizDequeRelease(gvizDeque *d) {
  if (d->arr)
    GVIZ_DEALLOC(d->arr);
}
