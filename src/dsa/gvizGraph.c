#include "dsa/gvizGraph.h"
#include "dsa/gvizArray.h"
#include "helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int gvizVertexInit(gvizVertex *v, void *data) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err = gvizArrayInit(&v->neighbors, sizeof(size_t));
  return err;
}

int gvizVertexInitAtCapacity(gvizVertex *v, void *data,
                             size_t initialCapacity) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err =
      gvizArrayInitAtCapacity(&v->neighbors, sizeof(size_t), initialCapacity);
  return err;
}

int gvizVertexCopy(gvizVertex *dest, const gvizVertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = gvizArrayCopy(&dest->neighbors, &src->neighbors);
  return err;
}

int gvizVertexClone(gvizVertex *dest, const gvizVertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = gvizArrayClone(&dest->neighbors, &src->neighbors);
  return err;
}

int gvizGraphInit(gvizGraph *g, int directed) {
  int err;
  if (g == NULL)
    return -1;
  err = gvizArrayInit(&g->vertices, sizeof(gvizVertex));
  if (err < 0)
    return err;
  g->directed = directed;
  g->map = NULL;
  return 0;
}

int gvizGraphInitAtCapacity(gvizGraph *g, int directed,
                            size_t initialCapacity) {
  int err;
  if (g == NULL)
    return -1;
  err = gvizArrayInitAtCapacity(&g->vertices, sizeof(gvizVertex),
                                initialCapacity);
  if (err < 0)
    return err;

  g->directed = directed;
  g->map = NULL;
  return 0;
}

int gvizGraphAddVertex(gvizGraph *g, void *data, gvizArray *in,
                       gvizArray *out) {
  gvizVertex v;
  int err;

  if (out != NULL) {
    err = gvizVertexInitAtCapacity(&v, data, out->count);
    if (err < 0)
      return err;
    gvizArrayCopy(&v.neighbors, out);
  } else {
    err = gvizVertexInit(&v, data);
    if (err < 0)
      return err;
  }

  gvizArrayPush(&g->vertices, &v);

  if (in != NULL) {
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

int gvizGraphAddEdge(gvizGraph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = gvizArrayPush(
      &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->neighbors, &to);
  if (err == 0 && !g->directed) {
    err = gvizArrayPush(
        &((gvizVertex *)gvizArrayAtIndex(&g->vertices, to))->neighbors, &from);
  }
  return err;
}

int gvizGraphRemoveEdge(gvizGraph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = gvizArrayFindOneAndDelete(
      &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->neighbors, &to);
  if (err >= 0 && !g->directed)
    err = gvizArrayFindOneAndDelete(
        &((gvizVertex *)gvizArrayAtIndex(&g->vertices, to))->neighbors, &from);
  return err < 0 ? err : 0;
}

void gvizGraphSetVertexData(gvizGraph *g, size_t idx, void *data) {
  ((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->data = data;
  return;
}

void *gvizGraphGetVertexData(gvizGraph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return ((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->data;
}

gvizArray *gvizGraphGetVertexNeighbors(const gvizGraph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return &((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->neighbors;
}

int gvizGraphEdgeExists(gvizGraph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  return gvizArrayFindOne(
             &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->neighbors,
             &to) >= 0
             ? 1
             : 0;
}

void gvizVertexRelease(gvizVertex *v) { gvizArrayRelease(&v->neighbors); }

void gvizGraphRelease(gvizGraph *g) {
  if (!gvizArrayIsEmpty(&g->vertices)) {
    for (int i = 0; i < g->vertices.count; i++) {
      gvizVertexRelease(gvizArrayAtIndex(&g->vertices, i));
    }
  }
  gvizArrayRelease(&g->vertices);

  if (g->map)
    GVIZ_DEALLOC(g->map);
}

int gvizGraphDFSTree(gvizGraph *g, gvizGraph *out, size_t source) {
  int err = 0;

  // Initialize output graph
  err = gvizGraphInit(out, 1);
  if (err)
    return -1;
  int *map = GVIZ_ALLOC(sizeof(int) * g->vertices.count);
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

  err = gvizGraphAddVertex(out, gvizGraphGetVertexData(g, source), NULL, NULL);
  if (err)
    return -1;

  err = gvizArrayPush(&frontier, 0);
  if (err)
    return -1;

  visited[source] = 1;

  // DFS:
  while (!gvizArrayIsEmpty(&frontier)) {
    gvizArrayPop(&frontier, &curr);
    currNeighbors = gvizGraphGetVertexNeighbors(g, map[curr]);
    if (!currNeighbors)
      return -1;

    for (size_t i = 0; i < currNeighbors->count; i++) {
      currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);
      if (!visited[currNeighbor]) {
        visited[currNeighbor] = 1;
        err = gvizGraphAddVertex(out, gvizGraphGetVertexData(g, map[curr]),
                                 NULL, NULL);
        size_t idx = out->vertices.count - 1;
        if (err)
          return -1;
        err = gvizArrayPush(&frontier, &idx);
        if (err)
          return -1;
        map[idx] = g->map ? g->map[currNeighbor] : currNeighbor;
        err = gvizGraphAddEdge(out, curr, idx);
        if (err)
          return -1;
      }
    }
  }

  // GVIZ_REALLOCate map to have a possible smaller size.
  int *tmp = GVIZ_REALLOC(map, out->vertices.count * sizeof(int));
  if (!tmp) {
    return -1;
  }
  map = tmp;
  out->map = map;

  // Cleanup and return
  gvizArrayRelease(&frontier);
  return 0;
}

void gvizGraphClear(gvizGraph *g) {
  if (g->vertices.count == 0)
    return;
  for (int i = 0; i < g->vertices.count; i++) {
    gvizVertexRelease(gvizArrayAtIndex(&g->vertices, i));
  }
  g->vertices.count = 0;
}

int gvizGraphCopy(gvizGraph *dest, const gvizGraph *src) {
  if (!dest || !src)
    return -1;

  gvizGraphClear(dest);

  // NOTE: We can't just gvizArrayCopy() since that is a shallow copy
  // The vertices in dest and src will share the memory for their adjacency
  // lists, causing possible double frees if both graphs are released.

  if (dest->vertices.capacity < src->vertices.count) {
    gvizArrayRelease(&dest->vertices);
    gvizArrayInitAtCapacity(&dest->vertices, sizeof(gvizVertex),
                            src->vertices.count);
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    int err = gvizVertexClone(gvizArrayAtIndex(&dest->vertices, i),
                              gvizArrayAtIndex(&src->vertices, i));
    if (err < 0)
      return err;
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int gvizGraphClone(gvizGraph *dest, const gvizGraph *src) {
  if (!dest || !src)
    return -1;
  gvizGraphInitAtCapacity(dest, src->directed, src->vertices.count);

  for (size_t i = 0; i < src->vertices.count; i++) {
    int err = gvizVertexClone(gvizArrayAtIndex(&dest->vertices, i),
                              gvizArrayAtIndex(&src->vertices, i));
    if (err < 0)
      return err;
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int gvizGraphCopyReversed(gvizGraph *dest, const gvizGraph *src) {
  if (!src->directed)
    return gvizGraphCopy(dest, src);

  gvizGraphClear(dest);

  if (dest->vertices.capacity < src->vertices.count) {
    gvizArrayRelease(&dest->vertices);
    gvizArrayInitAtCapacity(&dest->vertices, sizeof(gvizVertex),
                            src->vertices.count);
  }

  gvizArray *curr;
  gvizArray reversedLists[src->vertices.count];
  for (size_t i = 0; i < src->vertices.count; i++) {
    gvizArrayInit(&reversedLists[i], sizeof(size_t));
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    curr = gvizGraphGetVertexNeighbors(src, i);
    for (int j = 0; j < curr->count; j++) {
      gvizArrayPush(reversedLists + *(size_t *)gvizArrayAtIndex(curr, j), &i);
    }
  }

  gvizVertex *source;
  gvizVertex *target;
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

int gvizGraphCloneReversed(gvizGraph *dest, const gvizGraph *src) {
  if (!src->directed)
    return gvizGraphCopy(dest, src);

  gvizGraphInitAtCapacity(dest, src->directed, src->vertices.count);

  gvizArray *curr;
  gvizArray reversedLists[src->vertices.count];
  for (size_t i = 0; i < src->vertices.count; i++) {
    gvizArrayInit(&reversedLists[i], sizeof(size_t));
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    curr = gvizGraphGetVertexNeighbors(src, i);
    for (int j = 0; curr->count; j++) {
      gvizArrayPush(reversedLists + *(size_t *)gvizArrayAtIndex(curr, j), &i);
    }
  }

  gvizVertex *source;
  gvizVertex *target;
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

int printSubtree(gvizGraph *tree, size_t root, Position rootPos,
                 char *strings[], FILE *stream) {
  Position newPos;
  gvizArray *currNeighbors = gvizGraphGetVertexNeighbors(tree, root);
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

void gvizPrintDFSTree(gvizGraph *tree, char *strings[], FILE *stream) {
  clearScreen(stream);
  Position start = {1, 1};
  printSubtree(tree, 0, start, strings, stream);
  printAt(100, 100, "\n\n", stdout);
};
