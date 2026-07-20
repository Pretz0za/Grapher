#include "ds/gvizTree.h"
#include "core/alloc.h"
#include "ds/gvizArray.h"
#include "ds/gvizGraph.h"
#include <stdlib.h>

int gvizGraphIsTree(const gvizGraph *graph, int *parents) {
  if (!graph->directed)
    return -2;

  size_t n = graph->vertices.count;
  int *ptr = parents;
  int *scratch = NULL;
  if (!ptr) {
    scratch = GVIZ_ALLOC(sizeof(int) * n);
    if (!scratch)
      return -1;
    ptr = scratch;
  }

  int result = -1;
  for (size_t i = 0; i < n; i++) {
    ptr[i] = -1;
  }

  size_t edgeCount = 0;

  gvizArray *curr;
  for (size_t i = 0; i < n; i++) {
    curr = gvizGraphGetVertexNeighbors(graph, i);
    for (size_t j = 0; j < curr->count; j++) {
      size_t child = *(size_t *)gvizArrayAtIndex(curr, j);
      if (ptr[child] != -1)
        goto done; // Vertex with in-degree > 1

      edgeCount++;
      ptr[child] = (int)i;
    }
  }

  int root = -1;
  for (size_t i = 0; i < n; i++) {
    if (ptr[i] == -1) {
      if (root == -1)
        root = (int)i;
      else
        goto done; // More than one vertex with in-degree = 0
    }
  }

  if (edgeCount != n - 1) {
    result = 0;
    goto done;
  }

  // In-degree <= 1 everywhere and a single root only rules out shared
  // parents; a disjoint directed cycle still satisfies both. Count the
  // vertices reachable from the root: exactly n for a tree.
  {
    size_t *stack = GVIZ_ALLOC(sizeof(size_t) * n);
    if (!stack)
      goto done;
    size_t top = 0;
    size_t reached = 0;
    stack[top++] = (size_t)root;
    while (top > 0) {
      size_t v = stack[--top];
      reached++;
      curr = gvizGraphGetVertexNeighbors(graph, v);
      for (size_t j = 0; j < curr->count; j++)
        stack[top++] = *(size_t *)gvizArrayAtIndex(curr, j);
    }
    GVIZ_DEALLOC(stack);
    result = reached == n;
  }

done:
  if (scratch)
    GVIZ_DEALLOC(scratch);
  return result;
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
