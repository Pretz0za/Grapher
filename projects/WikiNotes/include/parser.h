#ifndef PARSER
#define PARSER

#include <stddef.h>
typedef struct Note {
  size_t line;
  size_t *references;
  size_t count;
  size_t capacity;
} Note;

typedef struct NotesSection {
  Note **notes;
  size_t count;
} NotesSection;

size_t strToLine(char *ptr, size_t count);
[[nodiscard]] char *lineToStr(size_t line);

Note *parseLine(char *line, size_t idx);
NotesSection parseFile(char *path, size_t count);

#endif
