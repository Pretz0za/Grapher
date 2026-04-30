#include "dsa/gvizBitArray.h"
#include "core/alloc.h"
#include <stdlib.h>
#include <string.h>

GVIZ_BIT_ARRAY gvizBitArrayAlloc(size_t nbits) {
  if (nbits == 0) {
    return NULL;
  }
  size_t units = GVIZ_ARRAY_UNITS(nbits);
  GVIZ_BIT_ARRAY arr = (GVIZ_BIT_ARRAY)GVIZ_ALLOC(units * sizeof(GVIZ_BIT_UNIT));
  if (arr) {
    memset(arr, 0, units * sizeof(GVIZ_BIT_UNIT));
  }
  return arr;
}

void gvizBitArrayFree(GVIZ_BIT_ARRAY arr) {
  if (arr) {
    GVIZ_DEALLOC(arr);
  }
}

GVIZ_BIT_ARRAY gvizBitArrayClone(const GVIZ_BIT_ARRAY src, size_t nbits) {
  if (!src || nbits == 0) {
    return NULL;
  }
  size_t units = GVIZ_ARRAY_UNITS(nbits);
  GVIZ_BIT_ARRAY out = (GVIZ_BIT_ARRAY)GVIZ_ALLOC(units * sizeof(GVIZ_BIT_UNIT));
  if (out) {
    memcpy(out, src, units * sizeof(GVIZ_BIT_UNIT));
  }
  return out;
}

static inline int popcount_unit(GVIZ_BIT_UNIT u) {
  return __builtin_popcount((unsigned int)u);
}

size_t gvizBitArrayCount(const GVIZ_BIT_ARRAY arr, size_t nbits) {
  if (!arr || nbits == 0) {
    return 0;
  }
  size_t units = GVIZ_ARRAY_UNITS(nbits);
  size_t total = 0;
  size_t whole_units = nbits / GVIZ_BITS_PER_UNIT;
  for (size_t i = 0; i < whole_units; i++) {
    total += (size_t)popcount_unit(arr[i]);
  }
  if (whole_units < units) {
    size_t leftover = nbits - whole_units * GVIZ_BITS_PER_UNIT;
    GVIZ_BIT_UNIT mask = (GVIZ_BIT_UNIT)((1u << leftover) - 1u);
    total += (size_t)popcount_unit(arr[whole_units] & mask);
  }
  return total;
}

void gvizBitArrayClearAll(GVIZ_BIT_ARRAY arr, size_t nbits) {
  if (!arr || nbits == 0) {
    return;
  }
  size_t units = GVIZ_ARRAY_UNITS(nbits);
  memset(arr, 0, units * sizeof(GVIZ_BIT_UNIT));
}

void gvizBitArrayIterInit(gvizBitArrayIter *iter, const GVIZ_BIT_ARRAY src,
                          size_t nbits) {
  if (!iter) {
    return;
  }
  iter->nbits = nbits;
  iter->units = GVIZ_ARRAY_UNITS(nbits);
  iter->cursor = 0;
  if (nbits == 0) {
    iter->bits = NULL;
    return;
  }
  if (src) {
    iter->bits = gvizBitArrayClone(src, nbits);
  } else {
    iter->bits = (GVIZ_BIT_ARRAY)GVIZ_ALLOC(iter->units * sizeof(GVIZ_BIT_UNIT));
    if (iter->bits) {
      memset(iter->bits, 0xFF, iter->units * sizeof(GVIZ_BIT_UNIT));
      size_t leftover = nbits % GVIZ_BITS_PER_UNIT;
      if (leftover != 0) {
        GVIZ_BIT_UNIT mask = (GVIZ_BIT_UNIT)((1u << leftover) - 1u);
        iter->bits[iter->units - 1] = mask;
      }
    }
  }
}

int gvizBitArrayIterNext(gvizBitArrayIter *iter, size_t *out) {
  if (!iter || !iter->bits || !out) {
    return 0;
  }
  while (iter->cursor < iter->units) {
    GVIZ_BIT_UNIT u = iter->bits[iter->cursor];
    if (u != 0) {
      int b = __builtin_ctz((unsigned int)u);
      size_t idx = iter->cursor * GVIZ_BITS_PER_UNIT + (size_t)b;
      if (idx >= iter->nbits) {
        return 0;
      }
      iter->bits[iter->cursor] = (GVIZ_BIT_UNIT)(u & (u - 1));
      *out = idx;
      return 1;
    }
    iter->cursor++;
  }
  return 0;
}

void gvizBitArrayIterRelease(gvizBitArrayIter *iter) {
  if (!iter) {
    return;
  }
  if (iter->bits) {
    GVIZ_DEALLOC(iter->bits);
    iter->bits = NULL;
  }
  iter->cursor = 0;
  iter->units = 0;
  iter->nbits = 0;
}
