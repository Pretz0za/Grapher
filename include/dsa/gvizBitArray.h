#ifndef _GVIZ_BIT_ARRAY_
#define _GVIZ_BIT_ARRAY_

// LSB is the zero-th element of the array.
#include <stddef.h>
typedef unsigned char GVIZ_BIT_UNIT;
#define GVIZ_BIT_ARRAY GVIZ_BIT_UNIT *
#define GVIZ_BITS_PER_UNIT (sizeof(GVIZ_BIT_UNIT) * 8)

#define gvizSetBit(A, k)                                                       \
  (A[(k) / GVIZ_BITS_PER_UNIT] |= (1 << ((k) % GVIZ_BITS_PER_UNIT)))
#define gvizClearBit(A, k)                                                     \
  (A[(k) / GVIZ_BITS_PER_UNIT] &= ~(1 << ((k) % GVIZ_BITS_PER_UNIT)))
#define gvizTestBit(A, k)                                                      \
  (A[(k) / GVIZ_BITS_PER_UNIT] & (1 << ((k) % GVIZ_BITS_PER_UNIT)))

#define GVIZ_ARRAY_UNITS(N) ((N + GVIZ_BITS_PER_UNIT - 1) / GVIZ_BITS_PER_UNIT)

static inline void gvizBitArrayOr(GVIZ_BIT_ARRAY dest, const GVIZ_BIT_ARRAY src,
                                  size_t nbits) {
  size_t units = GVIZ_ARRAY_UNITS(nbits);

  size_t i = 0;

  // process 8 bytes at a time if possible
  for (; i + sizeof(size_t) <= units; i += sizeof(size_t)) {
    *(size_t *)(dest + i) |= *(const size_t *)(src + i);
  }

  // tail
  for (; i < units; i++) {
    dest[i] |= src[i];
  }
}

static inline void gvizBitArrayAnd(GVIZ_BIT_ARRAY dest,
                                   const GVIZ_BIT_ARRAY src, size_t nbits) {
  size_t units = GVIZ_ARRAY_UNITS(nbits);

  size_t i = 0;

  // process 8 bytes at a time if possible
  for (; i + sizeof(size_t) <= units; i += sizeof(size_t)) {
    *(size_t *)(dest + i) &= *(const size_t *)(src + i);
  }

  // tail
  for (; i < units; i++) {
    dest[i] &= src[i];
  }
}

#endif
