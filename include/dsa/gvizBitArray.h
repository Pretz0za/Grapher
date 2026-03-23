#ifndef _GVIZ_BIT_ARRAY_
#define _GVIZ_BIT_ARRAY_

// LSB is the zero-th element of the array.
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

#endif
