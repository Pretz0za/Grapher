#include "../include/graph.h"
#include "../include/graphvis.h"
#include <_stdlib.h>
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXDEPTH 6

void randomTree(Graph *tree, size_t root, int depth, int k, int exact) {
  if (depth >= MAXDEPTH) {
    return;
  }
  if (exact) {
    for (int i = 0; i < k; i++) {
      graphAddVertex(tree, NULL);
      graphAddEdge(tree, root, tree->count - 1);
      randomTree(tree, tree->count - 1, depth + 1, k, exact);
    }
  } else {
    int random = arc4random_uniform(k + 1);
    printf("random number %d\n", random);
    for (int i = 0; i < random; i++) {
      graphAddVertex(tree, NULL);
      graphAddEdge(tree, root, tree->count - 1);
      randomTree(tree, tree->count - 1, depth + 1, k, exact);
    }
  }
  return;
}

int main() {
  int k = 5;
  Graph g;
  graphInit(&g, 1, NODATA);
  graphAddVertex(&g, NULL);
  randomTree(&g, 0, 0, k, 0);
  size_t curr = g.count;
  while (curr < pow(k, MAXDEPTH - 2)) {
    graphRelease(&g);
    graphInit(&g, 1, NODATA);
    graphAddVertex(&g, NULL);
    randomTree(&g, 0, 0, k, 0);
    curr = g.count;
  }
  // Graph *dfs = DepthFirstSearch(g, 1);
  // ViewStack stack;
  // stack.capacity = 1;
  // stack.count = 1;
  // stack.end = malloc(sizeof(Graph *));
  // stack.end = dfs;
  //
  Embedding embedding = generateTreeEmbedding(&g);
  VisState state = {&g, NULL, embedding};
  openStateInWindow(state);
  graphRelease(&g);
  free(embedding.components);
  return 0;
}
