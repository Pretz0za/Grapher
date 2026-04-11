#include "../../include/dsa/gvizArray.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gvizArrayInit(gvizArray *arr, size_t elementSize) {
  if (arr == NULL)
    return -1;
  arr->arr = GVIZ_ALLOC(elementSize * 8);
  if (arr->arr == NULL)
    return -1;
  arr->capacity = 8;
  arr->count = 0;
  arr->elementSize = elementSize;
  return 0;
}

int gvizArrayInitAtCapacity(gvizArray *arr, size_t elementSize,
                            size_t initialCapacity) {
  if (arr == NULL)
    return -1;
  arr->arr = GVIZ_ALLOC(elementSize * initialCapacity);
  if (arr->arr == NULL)
    return -1;
  arr->capacity = initialCapacity;
  arr->count = 0;
  arr->elementSize = elementSize;
  return 0;
}

int gvizArrayPush(gvizArray *v, void *item) {
  if (v->count >= v->capacity) {
    size_t newCapacity = v->capacity ? v->capacity * 2 : 8;
    size_t *newArr = GVIZ_REALLOC(v->arr, v->elementSize * newCapacity);
    if (!newArr) {
      return -1;
    }
    v->capacity = newCapacity;
    v->arr = newArr;
  }
  char *arr = v->arr;
  memcpy(arr + (v->count++) * v->elementSize, item, v->elementSize);
  return 0;
}

int gvizArrayInsert(gvizArray *v, void *item, size_t idx) {
  char *arr = (char *)v->arr;

  if (v->count >= v->capacity) {
    size_t newCapacity = v->capacity ? v->capacity * 2 : 8;
    size_t *newArr = GVIZ_REALLOC(v->arr, v->elementSize * newCapacity);
    if (!newArr) {
      return -1;
    }
    v->capacity = newCapacity;
    v->arr = newArr;
  }

  memmove(arr + (idx + 1) * v->elementSize, arr + idx * v->elementSize,
          v->elementSize * (v->count - idx));
  memcpy(arr + idx * v->elementSize, item, v->elementSize);
  v->count++;
  return 0;
}

void gvizArraySwapDelete(gvizArray *v, size_t idx) {
  if (v->count == 0)
    return;

  size_t last = v->count - 1;

  if (idx != last) {
    // compute byte pointers
    char *base = (char *)v->arr;
    void *dst = base + idx * v->elementSize;
    void *src = base + last * v->elementSize;

    // swap element contents
    unsigned char
        tmp[v->elementSize]; // or allocate on heap if elementSize > 256

    memcpy(tmp, src, v->elementSize);
    memcpy(src, dst, v->elementSize);
    memcpy(dst, tmp, v->elementSize);
  }

  v->count--;
}

int gvizArrayPop(gvizArray *v, void *res) {
  if (v->count == 0)
    return -1;
  char *arr = v->arr;
  memcpy(res, arr + (--v->count) * v->elementSize, v->elementSize);
  return 0;
}

int gvizArrayFindOneAndDelete(gvizArray *v, void *item) {
  char *arr = v->arr;
  for (int i = 0; i < v->count; i++) {
    if (!memcmp(arr + i * v->elementSize, item, v->elementSize)) {
      memmove(arr + i * v->elementSize, arr + (i + 1) * v->elementSize,
              v->elementSize * (v->count - i - 1));
      v->count--;
      return i;
    }
  }
  return -1;
}

void gvizArrayDeleteAtIndex(gvizArray *v, size_t i) {
  char *arr = v->arr;
  memmove(arr + i * v->elementSize, arr + (i + 1) * v->elementSize,
          v->elementSize * (v->count - i - 1));
  v->count--;
}

void gvizArrayRelease(gvizArray *v) {
  if (v->arr)
    GVIZ_DEALLOC(v->arr);
}

int gvizArrayIsEmpty(const gvizArray *v) {
  return (v->count == 0 || v->capacity == 0);
}

int gvizArrayFindOne(const gvizArray *v, void *item) {
  char *arr = v->arr;
  for (int i = 0; i < v->count; i++) {
    if (!memcmp(arr + i * v->elementSize, item, v->elementSize))
      return i;
  }
  return -1;
}

int gvizArrayCopy(gvizArray *dest, const gvizArray *src) {
  if (dest->capacity * dest->elementSize < src->count * src->elementSize) {
    size_t *newArr = GVIZ_REALLOC(dest->arr, src->elementSize * src->count);
    if (newArr == NULL)
      return -1;
    dest->arr = newArr;
    dest->capacity = src->capacity;
    dest->elementSize = src->elementSize;
  }

  memcpy(dest->arr, src->arr, src->elementSize * src->count);

  dest->count = src->count;
  return 0;
}

int gvizArrayClone(gvizArray *dest, const gvizArray *src) {
  int err = 0;

  if (!src->count) {
    err = gvizArrayInit(dest, src->elementSize);
    if (err < 0)
      return err;
    dest->elementSize = src->elementSize;
    dest->count = 0;
    return 0;
  }

  err = gvizArrayInitAtCapacity(dest, src->elementSize, src->count);
  if (err < 0)
    return err;

  memcpy(dest->arr, src->arr, src->elementSize * src->count);

  dest->elementSize = src->elementSize;
  dest->count = src->count;
  return 0;
}

void gvizArrayMove(gvizArray *dest, gvizArray *src) {
  memcpy(dest, src, sizeof(gvizArray));
  src->arr = NULL;
}

void gvizArrayPrint(const gvizArray *v, FILE *stream,
                    gvizSerializeDatum *serialize, size_t bufsize) {
  char *arr = v->arr;
  char buf[bufsize];
  fprintf(stream, "[ ");
  for (int i = 0; i < v->count; i++) {
    serialize(arr + i * v->elementSize, buf, bufsize);
    fprintf(stream, "%s, ", buf);
  }
  fprintf(stream, " ]\n");
}

void *gvizArrayAtIndex(const gvizArray *v, size_t i) {
  return ((char *)v->arr) + i * v->elementSize;
}

void *gvizArrayTail(const gvizArray *v) {
  return ((char *)v->arr) + (v->elementSize * (v->count - 1));
}
