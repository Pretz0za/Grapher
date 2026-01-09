#include "../../include/graph.h"
#include "./include/helpers.h"
#include "./include/parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

Graph *makeNotesGraph(Note **notes, size_t size) {

  Graph *g = createGraph(size, 1);

  for (int i = 0; i < size; i++) {
    addVertex(g, notes[i]);
  }

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < notes[i]->count; j++) {
      addEdge(g, i, notes[i]->references[j]);
    }
  }

  return g;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(1);

  size_t lineCount = strToLine("fs", 2);
  Note **notes = parseFile(argv[1], lineCount);
  char *currNote;

  for (int i = 0; i < lineCount; i++) {

    currNote = lineToStr(i + 1);

    printf("Note [%s] has the following references in it:\n", currNote);

    for (int j = 0; j < notes[i]->count; j++) {
      currNote = lineToStr(notes[i]->references[j]);
      printf("%s, ", currNote);
    }

    printf("\n\n");
  }
}
