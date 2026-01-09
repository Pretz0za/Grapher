#ifndef PARSER
#define PARSER

#include <stddef.h>
typedef struct Note {
  size_t line;
  size_t *references;
  size_t count;
  size_t capacity;
} Note;

Note *parseLine(char *line, size_t idx);
Note **parseFile(char *path, size_t count);

#endif
