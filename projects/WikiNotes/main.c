#include "../../include/graph.h"
#include "../../include/helpers.h"
#include "./include/helpers.h"
#include "./include/parser.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

DataCopyFn copyFn = copyNote;
DataFreeFn freeFn = freeNote;

Graph *makeNotesGraph(Note **notes, size_t size, size_t *filter,
                      size_t filterSize) {

  Graph *g = createGraph(filterSize ? filterSize : size, 1, copyFn, freeFn);

  for (int i = 0; i < (filterSize ? filterSize : size); i++) {
    addVertex(g, copyFn(notes[i]));
  }

  size_t refIdx;

  for (int i = 0; i < (filterSize ? filterSize : size); i++) {
    for (int j = 0; j < notes[i]->references->count; j++) {
      if (filterSize == 0 || (refIdx = findInArr(notes[i]->references->arr[j],
                                                 filter, filterSize)) != -1) {

        addEdge(g, i, refIdx);
      }
    }
  }

  return g;
}

void destroyNotes(Note **notes, size_t count) {
  for (size_t i = 0; i < count; i++) {
    destroyVec(notes[i]->references);
    free(notes[i]);
  }
  free(notes);
}

[[nodiscard]] size_t *makeFilter(char *notes[], size_t count) {
  size_t *output = malloc(sizeof(size_t) * count);
  for (size_t i = 0; i < count; i++) {
    output[i] = strToLine(notes[i], strlen(notes[i]));
  }
  return output;
}

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(1);
  char *subset[] = {"m",  "r",  "s",  "t",  "ab", "ad", "ai", "al",
                    "am", "ar", "bz", "ca", "cf", "ci", "cj", "ck",
                    "cl", "cm", "ct", "cu", "cv", "cy", "cz", "dd"};
  size_t subsetSize = 24;
  size_t *filter = makeFilter(subset, subsetSize);

  size_t lineCount = strToLine("fs", 2);
  Note **notes = parseFile(argv[1], lineCount, filter, subsetSize);

  Graph *g = makeNotesGraph(notes, lineCount, filter, subsetSize);
  Graph *reverse = createGraph(g->size, 1, copyFn, freeFn);
  copyReversedGraph(reverse, g);

  char *strings[subsetSize];

  Graph *DFSTree = DepthFirstSearch(g, 0);

  for (int i = 0; i < DFSTree->count; i++) {
    strings[i] = lineToStr(((Note *)getVertexData(DFSTree, i))->line);
    // printf("vertex idx: %d, str: %s, children: \n", i, strings[i]);
    // printVec(neighbors(DFSTree, i), stdout);
  }

  printf("\n\n DFS of graph completed. Reached %zu/%zu vertices\n",
         DFSTree->count, g->count);
  printDFSTree(DFSTree, strings, stdout);

  printf("\n");

  Graph *rDFSTree = DepthFirstSearch(reverse, 0);
  for (int i = 0; i < rDFSTree->count; i++) {
    strings[i] = lineToStr(((Note *)getVertexData(DFSTree, i))->line);
    // printf("vertex idx: %d, str: %s, children: \n", i, strings[i]);
    // printVec(neighbors(rDFSTree, i), stdout);
  }

  printf("\n\n DFS of reverse graph completed. Reached %zu/%zu vertices\n",
         DFSTree->count, reverse->count);
  printDFSTree(DFSTree, strings, stdout);

  printf("destroying...\n");
  destroyGraph(g);
  destroyGraph(reverse);
  destroyGraph(DFSTree);
  destroyGraph(rDFSTree);
  destroyNotes(notes, subsetSize ? subsetSize : lineCount);
}
