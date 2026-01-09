#include "../include/helpers.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

void xorSwap(size_t *a, size_t *b) {
  assert(a && b);
  if (a != b) {
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
  }
}

int inBoundsVertex(Graph *g, size_t idx) {
  if (idx < 0 || idx >= g->size)
    return 0;
  return 1;
}

int inBoundsVertices(Graph *g, size_t idx1, size_t idx2) {
  if (idx1 < 0 || idx2 < 0 || idx2 >= g->size || idx2 >= g->size)
    return 0;
  return 1;
}

int pushToNeighbors(Vertex *v, size_t other) {
  if (!(v->count < v->capacity)) {
    long newSize = sizeof(size_t) * (v->capacity ? v->capacity * 2 : 4);
    size_t *newArr = realloc(v->neighbors, newSize);
    if (!newArr) {
      return 1;
    }
    v->neighbors = newArr;
  }
  v->neighbors[v->count++] = other;
  return 0;
}

int removeFromNeighbors(Vertex *v, size_t other) {
  if (v->count == 0)
    return 0;
  size_t *ptr = NULL;
  for (int idx = 0; idx < v->count; idx++) {
    if (v->neighbors[idx] == other) {
      ptr = v->neighbors + idx;
      break;
    }
  }
  if (!ptr)
    return 1;
  xorSwap(ptr, v->neighbors + --v->count);
  return 0;
}
