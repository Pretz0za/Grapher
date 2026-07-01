#include "dsa/gvizBitArray.h"

#if defined(__GNUC__) || defined(__clang__)
static unsigned gviz_ctz(size_t w) {
  return (unsigned)__builtin_ctzll((unsigned long long)w);
}
#elif defined(_MSC_VER)
#include <intrin.h>
static unsigned gviz_ctz(size_t w) {
  unsigned long idx;
  _BitScanForward64(&idx, (unsigned __int64)w);
  return (unsigned)idx;
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
#endif

gvizBitArrayIterator gvizBitArrayIteratorCreate(GVIZ_BIT_ARRAY arr, size_t size) {
  return (gvizBitArrayIterator){
      .arr = arr,
      .size = size,
      .idx = SIZE_MAX,
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
