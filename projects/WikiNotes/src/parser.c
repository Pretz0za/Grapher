#include "../include/parser.h"
#include "../include/helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int MAX_LINE_LEN = 2048;

int min(int a, int b) { return a < b ? a : b; }

char *lineToStr(size_t line) {
  char *out =
      malloc((line / 26 + 2) * sizeof(char)); // +1 for \0, +1 for rounding = +2
  size_t idx = 0;
  for (int curr = line; curr > 0; curr -= 26) {
    out[idx++] = 'a' + min(curr, 26) - 1;
  }
  out[idx] = 0;
  return out;
}

size_t strToLine(char *ptr, size_t count) {
  if (count == 0)
    return 0;
  size_t out = 0;
  int mult = 1;
  int currVal;
  for (char *curr = ptr + count - 1; curr >= ptr; curr--) {
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

  char *word;
  word = strtok(line, " ");
  size_t len;
  while (word) {
    len = strlen(word);
    if (*word == '[' && *(word + len - 1) == ']') {
      pushToRefs(n, strToLine(word + 1, len - 2));
    }
    word = strtok(NULL, " ");
  }
  return n;
}

NotesSection parseFile(char *path, size_t count) {
  NotesSection out;
  Note **notes = malloc(sizeof(Note) * count);
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    out.notes = NULL;
    out.count = 0;
    return out;
  }
  char line[MAX_LINE_LEN];
  int idx = 0;

  while (fgets(line, sizeof(line), f) != NULL) {
    notes[idx] = parseLine(line, idx + 1);
    idx++;
  }

  out.notes = notes;
  out.count = idx;
  return out;
}
