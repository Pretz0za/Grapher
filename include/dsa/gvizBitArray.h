#ifndef _GVIZ_BIT_ARRAY_
#define _GVIZ_BIT_ARRAY_

// LSB is the zero-th element of the array.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef unsigned char GVIZ_BIT_UNIT;
#define GVIZ_BIT_ARRAY GVIZ_BIT_UNIT *
#define GVIZ_BITS_PER_UNIT (sizeof(GVIZ_BIT_UNIT) * 8)
#define GVIZ_BITS_PER_WORD (sizeof(size_t) * 8)

#define gvizSetBit(A, k)                                                       \
  (A[(k) / GVIZ_BITS_PER_UNIT] |= (1 << ((k) % GVIZ_BITS_PER_UNIT)))
#define gvizClearBit(A, k)                                                     \
  (A[(k) / GVIZ_BITS_PER_UNIT] &= ~(1 << ((k) % GVIZ_BITS_PER_UNIT)))
#define gvizTestBit(A, k)                                                      \
  (A[(k) / GVIZ_BITS_PER_UNIT] & (1 << ((k) % GVIZ_BITS_PER_UNIT)))

#define GVIZ_ARRAY_UNITS(N) ((N + GVIZ_BITS_PER_UNIT - 1) / GVIZ_BITS_PER_UNIT)

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define GVIZ_BITARRAY_BIG_ENDIAN 1
#elif defined(__BIG_ENDIAN__)
#define GVIZ_BITARRAY_BIG_ENDIAN 1
#else
#define GVIZ_BITARRAY_BIG_ENDIAN 0
#endif

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFULL
static inline size_t gvizBitArraySwapWord(size_t w) {
  return (size_t)__builtin_bswap64((uint64_t)w);
}
#else
static inline size_t gvizBitArraySwapWord(size_t w) {
  return (size_t)__builtin_bswap32((uint32_t)w);
}
#endif

static inline size_t gvizBitArrayLoadWord(const GVIZ_BIT_UNIT *arr,
                                          size_t byte_off) {
#if GVIZ_BITARRAY_BIG_ENDIAN
  size_t w;
  memcpy(&w, arr + byte_off, sizeof(w));
  return gvizBitArraySwapWord(w);
#else
  return *(const size_t *)(arr + byte_off);
#endif
}

static inline void gvizBitArrayStoreWord(GVIZ_BIT_UNIT *arr, size_t byte_off,
                                         size_t w) {
#if GVIZ_BITARRAY_BIG_ENDIAN
  w = gvizBitArraySwapWord(w);
  memcpy(arr + byte_off, &w, sizeof(w));
#else
  *(size_t *)(arr + byte_off) = w;
#endif
}

static inline size_t gvizBitArrayLoadWordBounded(const GVIZ_BIT_UNIT *arr,
                                                 size_t byte_off, size_t units) {
  if (byte_off + sizeof(size_t) <= units) {
    return gvizBitArrayLoadWord(arr, byte_off);
  }

  size_t w = 0;
  for (size_t i = byte_off; i < units; i++) {
    w |= (size_t)arr[i] << (8 * (i - byte_off));
  }
  return w;
}

static inline void gvizBitArrayOr(GVIZ_BIT_ARRAY dest, const GVIZ_BIT_ARRAY src,
                                  size_t nbits) {
  size_t units = GVIZ_ARRAY_UNITS(nbits);

  size_t i = 0;

  for (; i + sizeof(size_t) <= units; i += sizeof(size_t)) {
    size_t d = gvizBitArrayLoadWord(dest, i);
    size_t s = gvizBitArrayLoadWord(src, i);
    gvizBitArrayStoreWord(dest, i, d | s);
  }

  for (; i < units; i++) {
    dest[i] |= src[i];
  }
}

static inline void gvizBitArrayAnd(GVIZ_BIT_ARRAY dest,
                                   const GVIZ_BIT_ARRAY src, size_t nbits) {
  size_t units = GVIZ_ARRAY_UNITS(nbits);

  size_t i = 0;

  for (; i + sizeof(size_t) <= units; i += sizeof(size_t)) {
    size_t d = gvizBitArrayLoadWord(dest, i);
    size_t s = gvizBitArrayLoadWord(src, i);
    gvizBitArrayStoreWord(dest, i, d & s);
  }

  for (; i < units; i++) {
    dest[i] &= src[i];
  }
}

/** Iterator state for visiting set bits in ascending index order. */
typedef struct {
  GVIZ_BIT_ARRAY arr;
  size_t size; /**< Total number of bits (nbits). */
  size_t idx;  /**< Last index returned; SIZE_MAX before the first call. */
} gvizBitArrayIterator;

/** Creates an iterator over @p arr with @p size bits. */
gvizBitArrayIterator gvizBitArrayIteratorCreate(GVIZ_BIT_ARRAY arr, size_t size);

/**
 * Creates an iterator over the half-open bit index range [@p start, @p end).
 */
gvizBitArrayIterator gvizBitArrayIteratorCreateRange(GVIZ_BIT_ARRAY arr,
                                                     size_t start, size_t end);

/**
 * Returns the next set bit index in @p *out_idx, or false when none remain.
 * Skips zero words and uses trailing-zero count within nonzero words.
 */
bool gvizBitArrayIterate(gvizBitArrayIterator *it, size_t *out_idx);

GVIZ_BIT_ARRAY gvizBitArrayAlloc(size_t nbits);
void gvizBitArrayFree(GVIZ_BIT_ARRAY arr);
GVIZ_BIT_ARRAY gvizBitArrayResize(GVIZ_BIT_ARRAY arr, size_t old_nbits,
                                  size_t new_nbits);

bool gvizBitArrayTest(GVIZ_BIT_ARRAY arr, size_t k);
void gvizBitArraySet(GVIZ_BIT_ARRAY arr, size_t k);
void gvizBitArrayClear(GVIZ_BIT_ARRAY arr, size_t k);
void gvizBitArrayClearRange(GVIZ_BIT_ARRAY arr, size_t start, size_t end);

size_t gvizBitArrayPopcount(GVIZ_BIT_ARRAY arr, size_t nbits);
size_t gvizBitArrayPopcountRange(GVIZ_BIT_ARRAY arr, size_t start, size_t end);

#endif
