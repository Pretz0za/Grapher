#include "../include/stack.h"
#include "../include/helpers.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

Vector *createVec() {
  Vector *v = malloc(sizeof(Vector));
  if (v == NULL)
    return NULL;
  v->arr = malloc(sizeof(size_t) * 8);
  if (v->arr == NULL)
    return NULL;
  v->capacity = 8;
  v->count = 0;
  return v;
}

int pushToVec(Vector *v, size_t item) {
  if (v->count >= v->capacity) {
    size_t newCapacity = v->capacity ? v->capacity * 2 : 8;
    size_t *newArr = realloc(v->arr, sizeof(size_t) * newCapacity);
    if (!newArr) {
      return 1;
    }
    v->capacity = newCapacity;
    v->arr = newArr;
  }
  v->arr[v->count++] = item;
  return 0;
}

size_t popVec(Vector *v) {
  if (v->count == 0)
    return 0;
  size_t out = v->arr[--v->count];
  return out;
}

int deleteFromVec(Vector *s, size_t item) {
  for (int idx = 0; idx < s->count; idx++) {
    if (s->arr[idx] == item) {
      xorSwap(s->arr + --s->count, s->arr + idx);
      return 0;
    }
  }
  return 1;
}

void destroyVec(Vector *v) {
  if (v->arr)
    free(v->arr);
  free(v);
}

int isEmpty(Vector *v) { return (v->count == 0 || v->capacity == 0); }

size_t *findInVec(Vector *v, size_t item) {
  for (int idx = 0; idx < v->count; idx++) {
    if (v->arr[idx] == item)
      return v->arr + idx;
  }
  return NULL;
}

int copyVec(Vector *dest, Vector *src) {
  if (dest->capacity < src->count) {
    size_t *newArr = realloc(dest->arr, sizeof(size_t) * src->capacity);
    if (newArr == NULL)
      return 1;
    dest->arr = newArr;
    dest->capacity = src->capacity;
  }
  for (int i = 0; i < src->count; i++) {
    dest->arr[i] = src->arr[i];
  }
  dest->count = src->count;
  return 0;
}
