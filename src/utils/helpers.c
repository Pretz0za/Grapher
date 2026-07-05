#include "utils/helpers.h"
#include "core/gvizVec.h"
#include "ds/gvizArray.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void xorSwap(size_t *a, size_t *b) {
  assert(a && b);
  if (a != b) {
    *a ^= *b;
    *b ^= *a;
    *a ^= *b;
  }
}

int inBoundsVertex(const gvizGraph *g, size_t idx) {
  if (idx < 0 || idx >= g->vertices.count)
    return 0;
  return 1;
}

int inBoundsVertices(const gvizGraph *g, size_t idx1, size_t idx2) {
  if (idx1 < 0 || idx2 < 0 || idx1 >= g->vertices.count ||
      idx2 >= g->vertices.count)
    return 0;
  return 1;
}

void printAt(int x, int y, char *line, FILE *stream) {
  fprintf(stream, "\x1b[%d;%dH%s", y, x + 1, line);
}

void clearScreen(FILE *stream) { fprintf(stream, "\x1b[2J\x1b[H"); }

double *barrycenter(size_t n, const double *positions, const gvizArray *indices,
                    double *out) {
  memset(out, 0, sizeof(double) * n);
  for (size_t i = 0; i < indices->count; i++) {
    size_t idx = *(size_t *)gvizArrayAtIndex(indices, i);
    gvizVecAxpy(n, 1.0, positions + idx, out);
  }
  gvizVecScale(n, 1.0 / (double)indices->count, out);

  return 0;
}
