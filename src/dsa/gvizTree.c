#include "dsa/gvizTree.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"

int gvizGraphIsTree(const gvizGraph *graph, int *parents) {
  if (!graph->directed)
    return -2;

  int *ptr = parents;
  if (!ptr) {
    int x[graph->vertices.count];
    ptr = x;
  }

  for (int i = 0; i < graph->vertices.count; i++) {
    ptr[i] = -1;
  }

  size_t edgeCount = 0;

  gvizArray *curr;
  for (int i = 0; i < graph->vertices.count; i++) {
    curr = gvizGraphGetVertexNeighbors(graph, i);
    for (int j = 0; j < curr->count; j++) {
      if (ptr[*(size_t *)gvizArrayAtIndex(curr, j)] != -1)
        return -1; // Vertex with in-degree > 1

      edgeCount++;
      ptr[*(size_t *)gvizArrayAtIndex(curr, j)] = i;
    }
  }

  int root = -1;
  for (int i = 0; i < graph->vertices.count; i++) {
    if (ptr[i] == -1) {
      if (root == -1)
        root = i;
      else
        return -1; // More than one vertex with in-degree = 0
    }
  }

  return edgeCount == (graph->vertices.count - 1);
}

int gvizTreeIsLeaf(const gvizGraph *tree, size_t index) {
  return (gvizArrayIsEmpty(gvizGraphGetVertexNeighbors(tree, index)));
}

size_t gvizTreeCountLeaves(const gvizGraph *tree, size_t root) {
  if (gvizTreeIsLeaf(tree, root))
    return 1;

  size_t out = 0;
  gvizArray *children = gvizGraphGetVertexNeighbors(tree, root);

  for (size_t i = 0; i < children->count; i++) {
    out += gvizTreeCountLeaves(tree, *(size_t *)gvizArrayAtIndex(children, i));
  }

  return out;
}
