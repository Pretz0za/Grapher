#include "../../include/graph.h"
#include "./include/helpers.h"
#include "./include/parser.h"
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

  size_t lineCount =
      strToLine("fs", 2); // Index of last note in wikipedia article
  NotesSection notes = parseFile(argv[1], lineCount);
}
