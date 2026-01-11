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

void printAt(int x, int y, char *line, FILE *stream) {
  fprintf(stream, "\x1b[%d;%dH%s", y, x + 1, line);
}

void clearScreen(FILE *stream) { fprintf(stream, "\x1b[2J\x1b[H"); }
