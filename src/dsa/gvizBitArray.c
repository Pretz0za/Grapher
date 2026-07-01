#include "dsa/gvizBitArray.h"
#include "core/alloc.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
static unsigned gviz_ctz(size_t w) {
  return (unsigned)__builtin_ctzll((unsigned long long)w);
}

static unsigned gviz_popcount_word(size_t w) {
  if (sizeof(size_t) >= 8)
    return (unsigned)__builtin_popcountll((unsigned long long)w);
  return (unsigned)__builtin_popcount((unsigned)w);
}
#elif defined(_MSC_VER)
#include <intrin.h>
static unsigned gviz_ctz(size_t w) {
  unsigned long idx;
  _BitScanForward64(&idx, (unsigned __int64)w);
  return (unsigned)idx;
}

static unsigned gviz_popcount_word(size_t w) {
  return (unsigned)__popcnt64((unsigned __int64)w);
}
#else
static unsigned gviz_ctz(size_t w) {
  static const unsigned char debruijn[64] = {
      0,  1,  2,  53, 3,  7,  54, 27, 4,  38, 41, 31, 55, 39, 28, 15,
      5,  42, 45, 32, 56, 49, 18, 40, 29, 16, 47, 50, 36, 43, 46, 33,
      57, 24, 12, 19, 17, 48, 11, 8,  51, 35, 22, 13, 14, 10, 9,  23,
      44, 52, 6,  26, 37, 30, 34, 21, 25, 20, 48, 21, 20, 52, 21, 48,
  };
  return debruijn[(size_t)((w & (size_t)(-(ptrdiff_t)w)) * 0x022fdd63cc95386dULL) >>
                  58];
}

static unsigned gviz_popcount_word(size_t w) {
  w = w - ((w >> 1) & (size_t)~0ULL / 3);
  w = (w & (size_t)~0ULL / 15 * 3) + ((w >> 2) & (size_t)~0ULL / 15 * 3);
  w = (w + (w >> 4)) & (size_t)~0ULL / 255 * 15;
  return (unsigned)((w * ((size_t)~0ULL / 255)) >> (sizeof(size_t) - 1) * 8);
}
#endif

gvizBitArrayIterator gvizBitArrayIteratorCreate(GVIZ_BIT_ARRAY arr, size_t size) {
  return (gvizBitArrayIterator){
      .arr = arr,
      .size = size,
      .idx = SIZE_MAX,
  };
}

gvizBitArrayIterator gvizBitArrayIteratorCreateRange(GVIZ_BIT_ARRAY arr,
                                                     size_t start, size_t end) {
  return (gvizBitArrayIterator){
      .arr = arr,
      .size = end,
      .idx = start == 0 ? SIZE_MAX : start - 1,
  };
}

bool gvizBitArrayIterate(gvizBitArrayIterator *it, size_t *out_idx) {
  size_t next = it->idx + 1;
  if (next >= it->size) {
    return false;
  }

  size_t units = GVIZ_ARRAY_UNITS(it->size);
  size_t num_words = (it->size + GVIZ_BITS_PER_WORD - 1) / GVIZ_BITS_PER_WORD;
  size_t word_idx = next / GVIZ_BITS_PER_WORD;

  while (word_idx < num_words) {
    size_t byte_off = word_idx * sizeof(size_t);
    size_t w = gvizBitArrayLoadWordBounded(it->arr, byte_off, units);

    size_t base = word_idx * GVIZ_BITS_PER_WORD;
    size_t bit_off = next - base;
    if (bit_off > 0) {
      w &= ~(((size_t)1 << bit_off) - 1);
    }

    size_t word_limit = it->size - base;
    if (word_limit < GVIZ_BITS_PER_WORD) {
      w &= ((size_t)1 << word_limit) - 1;
    }

    if (w != 0) {
      size_t idx = base + gviz_ctz(w);
      it->idx = idx;
      *out_idx = idx;
      return true;
    }

    word_idx++;
    next = word_idx * GVIZ_BITS_PER_WORD;
  }

  return false;
}

GVIZ_BIT_ARRAY gvizBitArrayAlloc(size_t nbits) {
  size_t units = GVIZ_ARRAY_UNITS(nbits);
  GVIZ_BIT_ARRAY arr = GVIZ_ALLOC(units * sizeof(GVIZ_BIT_UNIT));
  if (!arr)
    return NULL;
  memset(arr, 0, units * sizeof(GVIZ_BIT_UNIT));
  return arr;
}

void gvizBitArrayFree(GVIZ_BIT_ARRAY arr) { GVIZ_DEALLOC(arr); }

GVIZ_BIT_ARRAY gvizBitArrayResize(GVIZ_BIT_ARRAY arr, size_t old_nbits,
                                  size_t new_nbits) {
  size_t old_units = GVIZ_ARRAY_UNITS(old_nbits);
  size_t new_units = GVIZ_ARRAY_UNITS(new_nbits);
  GVIZ_BIT_ARRAY neu = GVIZ_ALLOC(new_units * sizeof(GVIZ_BIT_UNIT));
  if (!neu)
    return NULL;
  memset(neu, 0, new_units * sizeof(GVIZ_BIT_UNIT));
  if (arr) {
    size_t copy_units = old_units < new_units ? old_units : new_units;
    memcpy(neu, arr, copy_units * sizeof(GVIZ_BIT_UNIT));
    if (new_nbits < old_nbits) {
      size_t rem = new_nbits % GVIZ_BITS_PER_UNIT;
      if (rem != 0)
        neu[new_units - 1] &= (GVIZ_BIT_UNIT)((1u << rem) - 1u);
    }
  }
  return neu;
}

bool gvizBitArrayTest(GVIZ_BIT_ARRAY arr, size_t k) {
  return gvizTestBit(arr, k) != 0;
}

void gvizBitArraySet(GVIZ_BIT_ARRAY arr, size_t k) { gvizSetBit(arr, k); }

void gvizBitArrayClear(GVIZ_BIT_ARRAY arr, size_t k) { gvizClearBit(arr, k); }

void gvizBitArrayClearRange(GVIZ_BIT_ARRAY arr, size_t start, size_t end) {
  if (start >= end)
    return;

  size_t units = GVIZ_ARRAY_UNITS(end);
  size_t start_word = start / GVIZ_BITS_PER_WORD;
  size_t end_word = (end - 1) / GVIZ_BITS_PER_WORD;

  for (size_t word_idx = start_word; word_idx <= end_word; word_idx++) {
    size_t byte_off = word_idx * sizeof(size_t);
    size_t base = word_idx * GVIZ_BITS_PER_WORD;

    if (byte_off + sizeof(size_t) > units) {
      for (size_t bit = start; bit < end; bit++)
        gvizBitArrayClear(arr, bit);
      return;
    }

    size_t w = gvizBitArrayLoadWordBounded(arr, byte_off, units);
    size_t mask = (size_t)~0;

    if (word_idx == start_word && start > base) {
      size_t bit_off = start - base;
      mask &= ((size_t)1 << bit_off) - 1;
    }
    if (word_idx == end_word) {
      size_t limit = end - base;
      if (limit < GVIZ_BITS_PER_WORD)
        mask &= ~(((size_t)1 << limit) - 1);
    }

    gvizBitArrayStoreWord(arr, byte_off, w & mask);
  }
}

size_t gvizBitArrayPopcount(GVIZ_BIT_ARRAY arr, size_t nbits) {
  if (nbits == 0)
    return 0;

  size_t units = GVIZ_ARRAY_UNITS(nbits);
  size_t count = 0;
  size_t num_words = (nbits + GVIZ_BITS_PER_WORD - 1) / GVIZ_BITS_PER_WORD;

  for (size_t word_idx = 0; word_idx < num_words; word_idx++) {
    size_t byte_off = word_idx * sizeof(size_t);
    size_t w = gvizBitArrayLoadWordBounded(arr, byte_off, units);
    size_t base = word_idx * GVIZ_BITS_PER_WORD;
    size_t word_bits = nbits - base;
    if (word_bits < GVIZ_BITS_PER_WORD)
      w &= ((size_t)1 << word_bits) - 1;
    count += gviz_popcount_word(w);
  }
  return count;
}

size_t gvizBitArrayPopcountRange(GVIZ_BIT_ARRAY arr, size_t start, size_t end) {
  if (start >= end)
    return 0;

  size_t units = GVIZ_ARRAY_UNITS(end);
  size_t count = 0;
  size_t start_word = start / GVIZ_BITS_PER_WORD;
  size_t end_word = (end - 1) / GVIZ_BITS_PER_WORD;

  for (size_t word_idx = start_word; word_idx <= end_word; word_idx++) {
    size_t byte_off = word_idx * sizeof(size_t);
    size_t w = gvizBitArrayLoadWordBounded(arr, byte_off, units);
    size_t base = word_idx * GVIZ_BITS_PER_WORD;

    if (word_idx == start_word && start > base) {
      size_t bit_off = start - base;
      w &= ~(((size_t)1 << bit_off) - 1);
    }
    if (word_idx == end_word) {
      size_t limit = end - base;
      if (limit < GVIZ_BITS_PER_WORD)
        w &= ((size_t)1 << limit) - 1;
    }
    count += gviz_popcount_word(w);
  }
  return count;
}
