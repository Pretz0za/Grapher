#include "../../include/graph.h"
#include "./include/helpers.h"
#include "./include/parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void *copyNote(const void *data) {
  Note *src = (Note *)data;
  Note *dest = malloc(sizeof(Note));
  dest->references = createVec();
  copyVec(dest->references, src->references);
  dest->line = src->line;

  return dest;
}

void freeNote(void *data) {
  Note *tmp = (Note *)data;
  destroyVec(tmp->references);
  free(tmp);
}

Graph *makeNotesGraph(Note **notes, size_t size, size_t *filter,
                      size_t filterSize) {

  DataCopyFn copyFn = copyNote;
  DataFreeFn freeFn = freeNote;
  Graph *g = createGraph(size, 1, copyFn, freeFn);

  for (int i = 0; i < size; i++) {
    addVertex(g, notes[i]);
  }

  for (int i = 0; i < size; i++) {
    for (int j = 0; j < notes[i]->references->count; j++) {
      if (filterSize == 0 ||
          findInArr(notes[i]->references->arr[j], filter, filterSize))
        addEdge(g, i, notes[i]->references->arr[j]);
    }
  }

  return g;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(1);
  char *subset[] = {"m",  "r",  "s",  "t",  "ab", "ad", "ai", "al",
                    "am", "ar", "bz", "ca", "cf", "ci", "cj", "ck",
                    "cl", "cm", "ct", "cu", "cv", "cy", "cz", "dd"};
  size_t subsetSize = 21;
  size_t lineSubset[subsetSize];
  size_t len = 1;
  for (int i = 0; i < subsetSize; i++) {
    if (i == 4)
      len = 2;
    lineSubset[i] = strToLine(subset[i], len);
  }

  size_t lineCount = strToLine("fs", 2);
  Note **notes = parseFile(argv[1], lineCount, lineSubset, subsetSize);
  char *currNote;

  for (int i = 0; i < subsetSize; i++) {

    currNote = lineToStr(i + 1);

    printf("Note [%s] has the following references in it:\n", currNote);

    for (int j = 0; j < notes[i]->references->count; j++) {
      currNote = lineToStr(notes[i]->references->arr[j]);
      printf("%s, ", currNote);
    }

    printf("\n\n");
  }

  Graph *g = makeNotesGraph(notes, lineCount);

  // TODO: Cool stuff

  printf("Running DFS\n");
  Vector *expansionOrder = DepthFirstSearch(g, 0);
  for (int i = 0; i < expansionOrder->count; i++) {
    printf("%c", *(char *)(g->vertices[expansionOrder->arr[i]]->data));
  }
  printf("\n");
  destroyGraph(g);
}
