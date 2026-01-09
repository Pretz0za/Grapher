#include "../include/parser.h"
#include "../include/helpers.h"
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
  n->references = malloc(sizeof(size_t) * 4);
  n->line = idx;
  n->capacity = 4;
  n->count = 0;

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
        pushToRefs(n, currRef);
    }
  }

  return n;
}

Note **parseFile(char *path, size_t count) {
  Note **notes = malloc(sizeof(Note) * count);
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    return NULL;
  }
  char line[MAX_LINE_LEN];
  int idx = 0;

  while (fgets(line, sizeof(line), f) != NULL) {
    notes[idx] = parseLine(line, idx + 1);
    idx++;
  }

  return notes;
}
