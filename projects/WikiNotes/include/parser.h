#ifndef PARSER
#define PARSER

#include <stddef.h>
typedef struct Note {
  size_t line;
  size_t *references;
  size_t count;
  size_t capacity;
} Note;

size_t strToLine(char *ptr, size_t count);
[[nodiscard]] char *lineToStr(size_t n);

Note *parseLine(char *line, size_t idx);
Note **parseFile(char *path, size_t count);

#endif
