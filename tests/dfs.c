#include "../include/graph.h"
#include <stdio.h>

int main() {
  Graph *g = createGraph(5, 0);
  char data[] = {'A', 'B', 'C', 'D', 'E'};
  for (int i = 0; i < 5; i++) {
    printf("Creating vertex %c\n", data[i]);
    addVertex(g, data + i);
  }
  printf("Adding Edges\n");
  addEdge(g, 0, 1);
  addEdge(g, 0, 2);
  addEdge(g, 1, 3);
  addEdge(g, 1, 4);
  addEdge(g, 2, 3);
  printf("Running DFS\n");
  Vector *expansionOrder = DepthFirstSearch(g, 0);
  for (int i = 0; i < expansionOrder->count; i++) {
    printf("%c", *(char *)(g->vertices[expansionOrder->arr[i]]->data));
  }
  printf("\n");
}
