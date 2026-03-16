#include "../../include/dsa/uLongArray.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ulArrayInit(ULongArray *arr) {
  if (arr == NULL)
    return -1;
  arr->arr = ULA_ALLOC(sizeof(size_t) * 8);
  if (arr->arr == NULL)
    return -1;
  arr->capacity = 8;
  arr->count = 0;
  return 0;
}

int ulArrayInitAtCapacity(ULongArray *arr, size_t initialCapacity) {
  if (arr == NULL)
    return -1;
  arr->arr = ULA_ALLOC(sizeof(size_t) * initialCapacity);
  if (arr->arr == NULL)
    return -1;
  arr->capacity = initialCapacity;
  arr->count = 0;
  return 0;
}

int ulArrayPush(ULongArray *v, size_t item) {
  if (v->count >= v->capacity) {
    size_t newCapacity = v->capacity ? v->capacity * 2 : 8;
    size_t *newArr = ULA_REALLOC(v->arr, sizeof(size_t) * newCapacity);
    if (!newArr) {
      return -1;
    }
    v->capacity = newCapacity;
    v->arr = newArr;
  }
  v->arr[v->count++] = item;
  return 0;
}

int ulArrayPop(ULongArray *v, size_t *res) {
  if (v->count == 0)
    return -1;
  *res = v->arr[--v->count];
  return 0;
}

int ulArrayFindOneAndDelete(ULongArray *s, size_t item) {
  for (int idx = 0; idx < s->count; idx++) {
    if (s->arr[idx] == item) {
      for (int j = idx + 1; j < s->count; j++) {
        s->arr[j - 1] = s->arr[j];
      }
      s->count--;
      return idx;
    }
  }
  return -1;
}

void ulArrayRelease(ULongArray *v) {
  if (v->arr)
    ULA_DEALLOC(v->arr);
}

int ulArrayIsEmpty(ULongArray *v) {
  return (v->count == 0 || v->capacity == 0);
}

int ulArrayFindOne(ULongArray *v, size_t item) {
  for (int idx = 0; idx < v->count; idx++) {
    if (v->arr[idx] == item)
      return idx;
  }
  return -1;
}

int ulArrayCopy(ULongArray *dest, const ULongArray *src) {
  if (dest->capacity < src->count) {
    size_t *newArr = ULA_REALLOC(dest->arr, sizeof(size_t) * src->capacity);
    if (newArr == NULL)
      return -1;
    dest->arr = newArr;
    dest->capacity = src->capacity;
  }

  memcpy(dest, src, sizeof(size_t) * src->count);

  dest->count = src->count;
  return 0;
}

int ulArrayClone(ULongArray *dest, const ULongArray *src) {
  int err = 0;
  err = ulArrayInitAtCapacity(dest, src->count);
  if (err < 0)
    return err;

  memcpy(dest, src, sizeof(size_t) * src->count);

  dest->count = src->count;
  return 0;
}

void ulArrayMove(ULongArray *dest, ULongArray *src) {
  memcpy(dest, src, sizeof(ULongArray));
  src->arr = NULL;
}

void ulArrayPrint(ULongArray *v, FILE *stream) {
  fprintf(stream, "[ ");
  for (int i = 0; i < v->count; i++) {
    fprintf(stream, "%zu, ", v->arr[i]);
  }
  fprintf(stream, "]\n");
}
