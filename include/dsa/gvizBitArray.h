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

/**
 * Allocate a zero-initialized bit array sized to hold at least `nbits` bits.
 * Returns NULL when `nbits == 0`. Free with `gvizBitArrayFree`.
 */
GVIZ_BIT_ARRAY gvizBitArrayAlloc(size_t nbits);

/**
 * Free a bit array previously returned by `gvizBitArrayAlloc` /
 * `gvizBitArrayClone`. Safe to call with NULL.
 */
void gvizBitArrayFree(GVIZ_BIT_ARRAY arr);

/**
 * Return a freshly-allocated copy of `src` sized for `nbits` bits.
 * Returns NULL when `src` is NULL or `nbits == 0`.
 */
GVIZ_BIT_ARRAY gvizBitArrayClone(const GVIZ_BIT_ARRAY src, size_t nbits);

/**
 * Count the number of set bits in `arr` over the first `nbits` bits.
 */
size_t gvizBitArrayCount(const GVIZ_BIT_ARRAY arr, size_t nbits);

/**
 * Zero out all bits in `arr` over the first `nbits` bits.
 */
void gvizBitArrayClearAll(GVIZ_BIT_ARRAY arr, size_t nbits);

/**
 * Iterator that consumes set bits from a *copy* of a source bit array.
 *
 * Call `gvizBitArrayIterInit` to take a snapshot of the source bits, then
 * repeatedly call `gvizBitArrayIterNext` until it returns 0. Each call
 * returns the next set bit index (lowest first) and clears it from the
 * iterator's private buffer; the source bit array is never mutated.
 *
 * Always pair init with `gvizBitArrayIterRelease` to free the snapshot.
 */
typedef struct gvizBitArrayIter {
  GVIZ_BIT_ARRAY bits; /* owned snapshot of the source */
  size_t nbits;
  size_t units;
  size_t cursor; /* unit index to start scanning from */
} gvizBitArrayIter;

/**
 * Initialize the iterator by cloning `src` for the first `nbits` bits.
 * If `src` is NULL the iterator behaves as a fully-set range over [0, nbits).
 */
void gvizBitArrayIterInit(gvizBitArrayIter *iter, const GVIZ_BIT_ARRAY src,
                          size_t nbits);

/**
 * Advance the iterator to the next set bit. On success writes the bit index
 * to `*out` and returns 1; returns 0 when the iterator is exhausted.
 */
int gvizBitArrayIterNext(gvizBitArrayIter *iter, size_t *out);

/**
 * Release the iterator's internal snapshot. Safe to call multiple times.
 */
void gvizBitArrayIterRelease(gvizBitArrayIter *iter);

#endif
