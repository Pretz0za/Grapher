#ifndef PARSER
#define PARSER

#include "../../../include/stack.h"
#include <stddef.h>

typedef struct Note {
  size_t line;
  Vector *references;
} Note;

size_t strToLine(char *ptr, size_t count);
[[nodiscard]] char *lineToStr(size_t n);

[[nodiscard]] Note *parseLine(char *line, size_t idx);
[[nodiscard]] Note **parseFile(char *path, size_t count, size_t *filter,
                               size_t filterSize);

#endif
