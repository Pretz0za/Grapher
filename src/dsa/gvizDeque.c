#include "dsa/gvizDeque.h"
#include "core/alloc.h"
#include <stdlib.h>
#include <string.h>

int gvizDequeInit(gvizDeque *d, size_t elementSize) {
  d->arr = GVIZ_ALLOC(elementSize * 8);
  if (!d->arr)
    return -1;

  d->capacity = 8;
  d->begin = NULL;
  d->count = 0;
  d->elementSize = elementSize;

  return 0;
}
int gvizDequeInitAtCapacity(gvizDeque *d, size_t elementSize,
                            size_t initialCapacity) {
  d->arr = GVIZ_ALLOC(elementSize * initialCapacity);
  if (!d->arr)
    return -1;

  d->capacity = initialCapacity;
  d->begin = NULL;
  d->count = 0;
  d->elementSize = elementSize;

  return 0;
}

size_t gvizDequeSize(const gvizDeque *d) { return d->count; }

int gvizDequeIsEmpty(const gvizDeque *d) { return !d->count; }

void *gvizDequeAtIndex(const gvizDeque *d, size_t idx) {
  if (idx >= d->count)
    return NULL;
  size_t offset = (idx + (d->begin - d->arr)) % d->capacity;
  return (d->arr + (d->elementSize * offset));
}

void gvizDequePopLeft(gvizDeque *d, void *res) {
  memcpy(res, d->begin, d->elementSize);
  d->count--;

  if (d->count) {
    size_t headIndex = ((char *)d->begin - ((char *)d->arr)) / d->elementSize;
    d->begin =
        ((char *)d->arr) + (((headIndex + 1) % d->capacity) * d->elementSize);
  } else
    d->begin = NULL;
};

void gvizDequePopRight(gvizDeque *d, void *res) {
  size_t headIndex = ((char *)d->begin - ((char *)d->arr)) / d->elementSize;
  size_t tailIndex = (headIndex + d->count - 1) % d->capacity;
  char *end = ((char *)d->arr) + (tailIndex * d->elementSize);
  memcpy(res, end, d->elementSize);
  d->count--;
  if (!d->count)
    d->begin = NULL;
};

void *gvizDequePeekLeft(const gvizDeque *d) { return d->begin; }
void *gvizDequePeekRight(const gvizDeque *d) {
  return d->count ? ((char *)d->arr) + (d->count - 1) * d->elementSize : NULL;
}

int gvizDequePush(gvizDeque *d, void *item) {
  // On first use, capacity and arr must already be initialized
  if (d->capacity == 0 || d->elementSize == 0 || d->arr == NULL) {
    return -1;
  }

  // Grow if full
  if (d->count == d->capacity) {
    size_t newCapacity = d->capacity * 2;
    size_t oldCapacity = d->capacity;

    char *oldArr = (char *)d->arr;
    char *newArr = (char *)GVIZ_ALLOC(newCapacity * d->elementSize);
    if (!newArr) {
      return -1;
    }

    if (d->count == 0) {
      // Nothing to copy
      d->arr = newArr;
      d->begin = NULL;
      d->capacity = newCapacity;
    } else {
      // Compute head index (in elements)
      size_t headIndex = ((char *)d->begin - oldArr) / d->elementSize;

      // If contiguous: elements are from headIndex .. headIndex + count - 1
      if (headIndex + d->count <= oldCapacity) {
        memcpy(newArr, oldArr + headIndex * d->elementSize,
               d->count * d->elementSize);
      } else {
        // Wrapped: copy tail then head
        size_t firstPartCount = oldCapacity - headIndex;    // elements
        size_t secondPartCount = d->count - firstPartCount; // elements

        memcpy(newArr, oldArr + headIndex * d->elementSize,
               firstPartCount * d->elementSize);
        memcpy(newArr + firstPartCount * d->elementSize, oldArr,
               secondPartCount * d->elementSize);
      }

      GVIZ_DEALLOC(d->arr);

      d->arr = newArr;
      d->begin = newArr; // head element now at index 0
      d->capacity = newCapacity;
    }
  }

  char *base = (char *)d->arr;

  if (d->count == 0) {
    // First element
    d->begin = base;
    memcpy(d->begin, item, d->elementSize);
    d->count = 1;
    return 0;
  }

  // Compute indices in elements
  size_t headIndex = ((char *)d->begin - base) / d->elementSize;
  size_t tailIndex = (headIndex + d->count - 1) % d->capacity;
  size_t newTailIndex = (tailIndex + 1) % d->capacity;

  // Write to new tail
  void *dest = base + newTailIndex * d->elementSize;
  memcpy(dest, item, d->elementSize);

  d->count += 1;
  return 0;
}

void gvizDequeRelease(gvizDeque *d) {
  if (d->arr)
    GVIZ_DEALLOC(d->arr);
}
