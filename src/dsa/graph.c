#include "dsa/graph.h"
#include "dsa/uLongArray.h"
#include "helpers.h"
#include <_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int vertexInit(Vertex *v, void *data) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err = ulArrayInit(&v->neighbors);
  return err;
}

int vertexInitAtCapacity(Vertex *v, void *data, size_t initialCapacity) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err = ulArrayInitAtCapacity(&v->neighbors, initialCapacity);
  return err;
}

int vertexCopy(Vertex *dest, const Vertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = ulArrayCopy(&dest->neighbors, &src->neighbors);
  return err;
}

int vertexClone(Vertex *dest, const Vertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = ulArrayClone(&dest->neighbors, &src->neighbors);
  return err;
}

int graphInit(Graph *g, int directed) {
  if (g == NULL)
    return -1;
  g->vertices = GRAPH_ALLOC(64 * sizeof(Vertex));
  if (g->vertices == NULL)
    return -1;

  g->directed = directed;
  g->size = 64;
  g->count = 0;
  g->map = NULL;
  return 0;
}

int graphInitAtCapacity(Graph *g, int directed, size_t initialCapacity) {
  if (g == NULL)
    return -1;
  g->vertices = GRAPH_ALLOC(initialCapacity * sizeof(Vertex));
  if (g->vertices == NULL)
    return -1;

  g->directed = directed;
  g->size = initialCapacity;
  g->count = 0;
  g->map = NULL;
  return 0;
}

int graphAddVertex(Graph *g, void *data, ULongArray *in, ULongArray *out) {
  Vertex v;
  int err;

  if (out) {
    err = vertexInitAtCapacity(&v, data, out->count);
    if (err < 0)
      return err;
    ulArrayCopy(&v.neighbors, out);
  } else {
    err = vertexInit(&v, data);
    if (err < 0)
      return err;
  }

  if (g->count >= g->size) {
    Vertex *newVertices =
        GRAPH_REALLOC(g->vertices, sizeof(Vertex) * g->count * 2);
    if (newVertices == NULL)
      return -1;
    g->size = g->count * 2;
    g->vertices = newVertices;
  }

  memcpy(g->vertices + g->count++, &v, sizeof(Vertex));

  if (in) {
    for (size_t i = 0; i < in->count; i++) {
      err =
          ulArrayPush(&g->vertices[((size_t *)in)[i]].neighbors, g->count - 1);
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
  err = ulArrayPush(&g->vertices[from].neighbors, to);
  if (err == 0 && !g->directed) {
    err = ulArrayPush(&g->vertices[to].neighbors, from);
  }
  return err;
}

int graphRemoveEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = ulArrayFindOneAndDelete(&g->vertices[from].neighbors, to);
  if (err >= 0 && !g->directed)
    err = ulArrayFindOneAndDelete(&g->vertices[to].neighbors, from);
  return err < 0 ? err : 0;
}

void *graphGetVertexData(Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return g->vertices[idx].data;
}

ULongArray *graphGetVertexNeighbors(const Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return &g->vertices[idx].neighbors;
}

int graphEdgeExists(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  return ulArrayFindOne(&g->vertices[from].neighbors, to) >= 0 ? 1 : 0;
}

void vertexRelease(Vertex *v) { ulArrayRelease(&v->neighbors); }

void graphRelease(Graph *g) {
  if (g->vertices) {
    for (int i = 0; i < g->count; i++) {
      vertexRelease(&g->vertices[i]);
    }
    GRAPH_DEALLOC(g->vertices);
  }
  if (g->map)
    GRAPH_DEALLOC(g->map);
}

int graphDFSTree(Graph *g, Graph *out, size_t source) {
  int err = 0;

  // Initialize output graph
  err = graphInit(out, 1);
  if (err)
    return -1;
  int *map = GRAPH_ALLOC(sizeof(int) * g->count);
  if (!map) {
    return -1;
  }

  // Initialize frontier
  ULongArray frontier;
  err = ulArrayInit(&frontier);
  if (err)
    return -1;

  // Initialize DFS state
  ULongArray *currNeighbors;
  int visited[g->count];
  memset(visited, 0, sizeof(visited));
  size_t currNeighbor;
  size_t curr;

  // Add source vertex to output and frontier
  map[0] = source;

  err = graphAddVertex(out, graphGetVertexData(g, source), NULL, NULL);
  if (err)
    return -1;

  err = ulArrayPush(&frontier, 0);
  if (err)
    return -1;

  visited[source] = 1;

  // DFS:
  while (!ulArrayIsEmpty(&frontier)) {
    ulArrayPop(&frontier, &curr);
    currNeighbors = graphGetVertexNeighbors(g, map[curr]);
    if (!currNeighbors)
      return -1;

    for (size_t i = 0; i < currNeighbors->count; i++) {
      currNeighbor = currNeighbors->arr[i];
      if (!visited[currNeighbor]) {
        visited[currNeighbor] = 1;
        err = graphAddVertex(out, graphGetVertexData(g, map[curr]), NULL, NULL);
        if (err)
          return -1;
        err = ulArrayPush(&frontier, out->count - 1);
        if (err)
          return -1;
        map[out->count - 1] = g->map ? g->map[currNeighbor] : currNeighbor;
        err = graphAddEdge(out, curr, out->count - 1);
        if (err)
          return -1;
      }
    }
  }

  // GRAPH_REALLOCate map to have a possible smaller size.
  int *tmp = GRAPH_REALLOC(map, out->count * sizeof(int));
  if (!tmp) {
    return -1;
  }
  map = tmp;
  out->map = map;

  // Cleanup and return
  ulArrayRelease(&frontier);
  return 0;
}

void graphClear(Graph *g) {
  if (g->count == 0)
    return;
  for (int i = 0; i < g->count; i++) {
    vertexRelease(&g->vertices[i]);
  }
  g->count = 0;
}

int graphCopy(Graph *dest, const Graph *src) {
  if (!dest || !src)
    return -1;

  graphClear(dest);

  if (dest->size < src->count) {
    GRAPH_DEALLOC(dest->vertices);
    dest->vertices = GRAPH_ALLOC(sizeof(Vertex *) * src->count);
    dest->size = src->count;
    if (!dest->vertices)
      return -1;
  }

  for (size_t i = 0; i < src->count; i++) {
    int err = vertexClone(&dest->vertices[i], &src->vertices[i]);
    if (err < 0)
      return err;
  }

  dest->count = src->count;
  dest->directed = src->directed;

  return 0;
}

int graphClone(Graph *dest, const Graph *src) {
  if (!dest || !src)
    return -1;
  graphInitAtCapacity(dest, src->directed, src->count);

  for (size_t i = 0; i < src->count; i++) {
    int err = vertexClone(&dest->vertices[i], &src->vertices[i]);
    if (err < 0)
      return err;
  }

  dest->count = src->count;
  dest->directed = src->directed;

  return 0;
}

int graphCopyReversed(Graph *dest, const Graph *src) {
  if (!src->directed)
    return graphCopy(dest, src);

  graphClear(dest);

  if (dest->size < src->count) {
    GRAPH_DEALLOC(dest->vertices);
    dest->vertices = GRAPH_ALLOC(sizeof(Vertex *) * src->count);
    dest->size = src->count;
    if (!dest->vertices)
      return -1;
  }

  ULongArray *curr;
  ULongArray reversedLists[src->count];
  for (size_t i = 0; i < src->count; i++) {
    ulArrayInit(&reversedLists[i]);
  }

  for (size_t i = 0; i < src->count; i++) {
    curr = graphGetVertexNeighbors(src, i);
    for (int j = 0; curr->count; j++) {
      ulArrayPush(reversedLists + curr->arr[j], i);
    }
  }

  Vertex *target = dest->vertices;
  dest->count = 0;
  for (size_t i = 0; i < src->count; i++) {
    target[i].data = src->vertices[i].data;
    ulArrayMove(&target[i].neighbors, &reversedLists[i]);
  }

  dest->count = src->count;
  dest->directed = src->directed;

  return 0;
}

int graphCloneReversed(Graph *dest, const Graph *src) {
  if (!src->directed)
    return graphClone(dest, src);

  ULongArray *curr;
  ULongArray reversedLists[src->count];
  for (size_t i = 0; i < src->count; i++) {
    ulArrayInit(&reversedLists[i]);
  }

  for (size_t i = 0; i < src->count; i++) {
    curr = graphGetVertexNeighbors(src, i);
    for (int j = 0; curr->count; j++) {
      ulArrayPush(reversedLists + curr->arr[j], i);
    }
  }

  graphInitAtCapacity(dest, src->directed, src->count);

  Vertex *target = dest->vertices;
  dest->count = 0;
  for (size_t i = 0; i < src->count; i++) {
    target[i].data = src->vertices[i].data;
    ulArrayMove(&target[i].neighbors, &reversedLists[i]);
  }

  dest->count = src->count;
  dest->directed = src->directed;

  return 0;
}

int printSubtree(Graph *tree, size_t root, Position rootPos, char *strings[],
                 FILE *stream) {
  Position newPos;
  ULongArray *currNeighbors = graphGetVertexNeighbors(tree, root);
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

    int res =
        printSubtree(tree, currNeighbors->arr[i], newPos, strings, stream);

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
