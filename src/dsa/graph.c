#include "dsa/graph.h"
#include "dsa/gvizArray.h"
#include "helpers.h"
#include <_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vertexInit(Vertex *v, void *data) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err = gvizArrayInit(&v->neighbors, sizeof(size_t));
  return err;
}

int vertexInitAtCapacity(Vertex *v, void *data, size_t initialCapacity) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err =
      gvizArrayInitAtCapacity(&v->neighbors, initialCapacity, sizeof(size_t));
  return err;
}

int vertexCopy(Vertex *dest, const Vertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = gvizArrayCopy(&dest->neighbors, &src->neighbors);
  return err;
}

int vertexClone(Vertex *dest, const Vertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = gvizArrayClone(&dest->neighbors, &src->neighbors);
  return err;
}

int graphInit(Graph *g, int directed) {
  int err;
  if (g == NULL)
    return -1;
  err = gvizArrayInit(&g->vertices, sizeof(Vertex));
  if (err < 0)
    return err;
  g->directed = directed;
  g->map = NULL;
  return 0;
}

int graphInitAtCapacity(Graph *g, int directed, size_t initialCapacity) {
  int err;
  if (g == NULL)
    return -1;
  err = gvizArrayInitAtCapacity(&g->vertices, sizeof(Vertex), initialCapacity);
  if (err < 0)
    return err;

  g->directed = directed;
  g->map = NULL;
  return 0;
}

int graphAddVertex(Graph *g, void *data, gvizArray *in, gvizArray *out) {
  Vertex v;
  int err;

  if (out) {
    err = vertexInitAtCapacity(&v, data, out->count);
    if (err < 0)
      return err;
    gvizArrayCopy(&v.neighbors, out);
  } else {
    err = vertexInit(&v, data);
    if (err < 0)
      return err;
  }

  gvizArrayPush(&g->vertices, &v);

  if (in) {
    size_t idx = g->vertices.count - 1;
    for (size_t i = 0; i < in->count; i++) {
      err = gvizArrayPush(
          gvizArrayAtIndex(&g->vertices, *(size_t *)gvizArrayAtIndex(in, i)),
          &idx);
      if (err < 0)
        return err;
    }
  }

  return 0;
}

int graphAddEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = gvizArrayPush(
      &((Vertex *)gvizArrayAtIndex(&g->vertices, from))->neighbors, &to);
  if (err == 0 && !g->directed) {
    err = gvizArrayPush(
        &((Vertex *)gvizArrayAtIndex(&g->vertices, to))->neighbors, &from);
  }
  return err;
}

int graphRemoveEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = gvizArrayFindOneAndDelete(
      &((Vertex *)gvizArrayAtIndex(&g->vertices, from))->neighbors, &to);
  if (err >= 0 && !g->directed)
    err = gvizArrayFindOneAndDelete(
        &((Vertex *)gvizArrayAtIndex(&g->vertices, to))->neighbors, &from);
  return err < 0 ? err : 0;
}

void *graphGetVertexData(Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return ((Vertex *)gvizArrayAtIndex(&g->vertices, idx))->data;
}

gvizArray *graphGetVertexNeighbors(const Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return &((Vertex *)gvizArrayAtIndex(&g->vertices, idx))->neighbors;
}

int graphEdgeExists(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  return gvizArrayFindOne(gvizArrayAtIndex(&g->vertices, from), &to) >= 0 ? 1
                                                                          : 0;
}

void vertexRelease(Vertex *v) { gvizArrayRelease(&v->neighbors); }

void graphRelease(Graph *g) {
  if (!gvizArrayIsEmpty(&g->vertices)) {
    for (int i = 0; i < g->vertices.count; i++) {
      vertexRelease(gvizArrayAtIndex(&g->vertices, i));
    }
  }
  gvizArrayRelease(&g->vertices);

  if (g->map)
    GRAPH_DEALLOC(g->map);
}

int graphDFSTree(Graph *g, Graph *out, size_t source) {
  int err = 0;

  // Initialize output graph
  err = graphInit(out, 1);
  if (err)
    return -1;
  int *map = GRAPH_ALLOC(sizeof(int) * g->vertices.count);
  if (!map) {
    return -1;
  }

  // Initialize frontier
  gvizArray frontier;
  err = gvizArrayInit(&frontier, sizeof(size_t));
  if (err)
    return -1;

  // Initialize DFS state
  gvizArray *currNeighbors;
  int visited[g->vertices.count];
  memset(visited, 0, sizeof(visited));
  size_t currNeighbor;
  size_t curr;

  // Add source vertex to output and frontier
  map[0] = source;

  err = graphAddVertex(out, graphGetVertexData(g, source), NULL, NULL);
  if (err)
    return -1;

  err = gvizArrayPush(&frontier, 0);
  if (err)
    return -1;

  visited[source] = 1;

  // DFS:
  while (!gvizArrayIsEmpty(&frontier)) {
    gvizArrayPop(&frontier, &curr);
    currNeighbors = graphGetVertexNeighbors(g, map[curr]);
    if (!currNeighbors)
      return -1;

    for (size_t i = 0; i < currNeighbors->count; i++) {
      currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);
      if (!visited[currNeighbor]) {
        visited[currNeighbor] = 1;
        err = graphAddVertex(out, graphGetVertexData(g, map[curr]), NULL, NULL);
        size_t idx = out->vertices.count - 1;
        if (err)
          return -1;
        err = gvizArrayPush(&frontier, &idx);
        if (err)
          return -1;
        map[idx] = g->map ? g->map[currNeighbor] : currNeighbor;
        err = graphAddEdge(out, curr, idx);
        if (err)
          return -1;
      }
    }
  }

  // GRAPH_REALLOCate map to have a possible smaller size.
  int *tmp = GRAPH_REALLOC(map, out->vertices.count * sizeof(int));
  if (!tmp) {
    return -1;
  }
  map = tmp;
  out->map = map;

  // Cleanup and return
  gvizArrayRelease(&frontier);
  return 0;
}

void graphClear(Graph *g) {
  if (g->vertices.count == 0)
    return;
  for (int i = 0; i < g->vertices.count; i++) {
    vertexRelease(gvizArrayAtIndex(&g->vertices, i));
  }
  g->vertices.count = 0;
}

int graphCopy(Graph *dest, const Graph *src) {
  if (!dest || !src)
    return -1;

  graphClear(dest);

  // NOTE: We can't just gvizArrayCopy() since that is a shallow copy
  // The vertices in dest and src will share the memory for their adjacency
  // lists, causing possible double frees if both graphs are released.

  if (dest->vertices.capacity < src->vertices.count) {
    gvizArrayRelease(&dest->vertices);
    gvizArrayInitAtCapacity(&dest->vertices, sizeof(Vertex),
                            src->vertices.count);
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    int err = vertexClone(gvizArrayAtIndex(&dest->vertices, i),
                          gvizArrayAtIndex(&src->vertices, i));
    if (err < 0)
      return err;
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int graphClone(Graph *dest, const Graph *src) {
  if (!dest || !src)
    return -1;
  graphInitAtCapacity(dest, src->directed, src->vertices.count);

  for (size_t i = 0; i < src->vertices.count; i++) {
    int err = vertexClone(gvizArrayAtIndex(&dest->vertices, i),
                          gvizArrayAtIndex(&src->vertices, i));
    if (err < 0)
      return err;
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int graphCopyReversed(Graph *dest, const Graph *src) {
  if (!src->directed)
    return graphCopy(dest, src);

  graphClear(dest);

  if (dest->vertices.capacity < src->vertices.count) {
    gvizArrayRelease(&dest->vertices);
    gvizArrayInitAtCapacity(&dest->vertices, sizeof(Vertex),
                            src->vertices.count);
  }

  gvizArray *curr;
  gvizArray reversedLists[src->vertices.count];
  for (size_t i = 0; i < src->vertices.count; i++) {
    gvizArrayInit(&reversedLists[i], sizeof(size_t));
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    curr = graphGetVertexNeighbors(src, i);
    for (int j = 0; curr->count; j++) {
      gvizArrayPush(reversedLists + *(size_t *)gvizArrayAtIndex(curr, j), &i);
    }
  }

  Vertex *source;
  Vertex *target;
  for (size_t i = 0; i < src->vertices.count; i++) {
    target = gvizArrayAtIndex(&dest->vertices, i);
    source = gvizArrayAtIndex(&src->vertices, i);
    target->data = source->data;
    gvizArrayMove(&target->neighbors, &reversedLists[i]);
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int graphCloneReversed(Graph *dest, const Graph *src) {
  if (!src->directed)
    return graphCopy(dest, src);

  graphInitAtCapacity(dest, src->directed, src->vertices.count);

  gvizArray *curr;
  gvizArray reversedLists[src->vertices.count];
  for (size_t i = 0; i < src->vertices.count; i++) {
    gvizArrayInit(&reversedLists[i], sizeof(size_t));
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    curr = graphGetVertexNeighbors(src, i);
    for (int j = 0; curr->count; j++) {
      gvizArrayPush(reversedLists + *(size_t *)gvizArrayAtIndex(curr, j), &i);
    }
  }

  Vertex *source;
  Vertex *target;
  for (size_t i = 0; i < src->vertices.count; i++) {
    target = gvizArrayAtIndex(&dest->vertices, i);
    source = gvizArrayAtIndex(&src->vertices, i);
    target->data = source->data;
    gvizArrayMove(&target->neighbors, &reversedLists[i]);
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int printSubtree(Graph *tree, size_t root, Position rootPos, char *strings[],
                 FILE *stream) {
  Position newPos;
  gvizArray *currNeighbors = graphGetVertexNeighbors(tree, root);
  char str[32];
  if (!strings)
    snprintf(str, sizeof(str), "%zu", root);
  else
    strcpy(str, strings[root]);

  printAt(rootPos.x, rootPos.y, str, stream);
  int dy = 0;
  for (int i = 0; i < currNeighbors->count; i++) {
    printAt(rootPos.x, rootPos.y + 1, "│", stream);
    printAt(rootPos.x, rootPos.y + 2, "└", stream);
    printAt(rootPos.x + 1, rootPos.y + 2, "──", stream);
    newPos.x = rootPos.x + 3;
    newPos.y = rootPos.y + 2;
    rootPos.y += 2;
    dy += 2;

    int res = printSubtree(tree, *(size_t *)gvizArrayAtIndex(currNeighbors, i),
                           newPos, strings, stream);

    if (i + 1 < currNeighbors->count) {
      for (int j = 1; j < res; j += 2) {
        printAt(rootPos.x, rootPos.y + j, "│", stream);
        printAt(rootPos.x, rootPos.y + j + 1, "│", stream);
      }
    }

    rootPos.y += res;
    dy += res;
  }
  return dy;
}

void printDFSTree(Graph *tree, char *strings[], FILE *stream) {
  clearScreen(stream);
  Position start = {1, 1};
  printSubtree(tree, 0, start, strings, stream);
  printAt(100, 100, "\n\n", stdout);
};
