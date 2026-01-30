#include "../include/graph.h"
#include "../include/graphvis.h"
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {

  printf("creating graph\n");
  Graph *g = createGraph(1, NODATA);
  for (int i = 0; i < 13; i++) {
    addVertex(g, NULL);
  }
  int res = 0;
  res = addEdge(g, 0, 1);
  res = addEdge(g, 0, 2);
  res = addEdge(g, 0, 3);
  res = addEdge(g, 1, 4);
  res = addEdge(g, 2, 5);
  res = addEdge(g, 2, 6);
  res = addEdge(g, 2, 7);
  res = addEdge(g, 3, 8);
  res = addEdge(g, 3, 9);
  res = addEdge(g, 5, 10);
  res = addEdge(g, 5, 11);
  res = addEdge(g, 7, 12);
  if (res != 0) {
    printf("nonzero res %d\n", res);
    exit(EXIT_FAILURE);
  }

  printDFSTree(g, NULL, stdout);
  Embedding embedding = generateTreeEmbedding(g);
  VisState state = {g, NULL, embedding};
  openStateInWindow(state);
  return 0;
}
