#include "../include/parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int MAX_LINE_LEN = 2048;

int min(int a, int b) { return a < b ? a : b; }

char *lineToStr(size_t n) {
  if (n == 0)
    return NULL;

  size_t tmp = n;
  size_t len = 0;

  while (tmp) {
    tmp = (tmp - 1) / 26;
    len++;
  }

  char *out = malloc(len + 1);
  out[len] = '\0';

  while (n) {
    n--;
    out[--len] = 'a' + (n % 26);
    n /= 26;
  }

  return out;
}

size_t strToLine(char *ptr, size_t count) {
  if (count == 0)
    return 0;
  size_t out = 0;
  int mult = 1;
  int currVal;
  for (char *curr = ptr + count - 1; curr >= ptr; curr--) {
    if (*curr < 'a' || *curr > 'z')
      return 0;
    out += (*curr - 'a' + 1) * mult;
    mult *= 26;
  }
  return out;
}

Note *parseLine(char *line, size_t idx) {

  Note *n = malloc(sizeof(Note));
  n->references = createVec();
  n->line = idx;

  char *open;
  char *close = line;
  size_t currRef = 0;
  while ((open = strchr(close, '[')) != NULL) {
    close = strchr(open, ']');
    if (close == NULL)
      break;
    if (close - open < 5) {
      currRef = strToLine(open + 1, close - open - 1);
      if (currRef != 0)
        pushToVec(n->references, currRef);
    }
  }

  return n;
}

Note **parseFile(char *path, size_t count, size_t *filter, size_t filterSize) {
  Note **notes = NULL;
  if (filterSize)
    notes = malloc(sizeof(Note) * filterSize);
  else
    notes = malloc(sizeof(Note) * count);
  if (notes == NULL)
    exit(EXIT_FAILURE);

  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }

  char line[MAX_LINE_LEN];
  int idx = 0;

  int subsetIdx = 0;

  while (subsetIdx < filterSize && fgets(line, sizeof(line), f) != NULL) {
    if (filterSize == 0 || idx + 1 == filter[subsetIdx]) {
      // printf("parsing line %d\n", idx + 1);
      notes[subsetIdx] = parseLine(line, idx + 1);
      // printf("parsed! found %zu references.\n",
      // notes[idx]->references->count); printVec(notes[idx]->references,
      // stdout);
      subsetIdx++;
    }
    idx++;
  }

  return notes;
}
