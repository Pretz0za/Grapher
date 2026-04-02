#include "dsa/gvizGraph.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizDeque.h"
#include "helpers.h"
#include <stdint.h>
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
  int err;

  // initialize LIFO stack
  gvizDeque stack;
  err = gvizDequeInit(&stack, sizeof(size_t));
  if (err < 0)
    return -1;
  err = gvizDequePush(&stack, &source);
  if (err < 0)
    return -1;

  // initialize out to g map
  gvizArray map;
  err = gvizArrayInit(&map, sizeof(size_t));
  if (err < 0)
    return -1;
  err = gvizArrayPush(&map, &source);
  if (err < 0)
    return -1;

  // initialize g to out map
  size_t invMap[g->vertices.count];
  memset(invMap, 0, sizeof(invMap));

  // initialize seen array
  GVIZ_BIT_UNIT seen[GVIZ_ARRAY_UNITS(g->vertices.count)];
  memset(seen, 0, sizeof(seen));
  gvizSetBit(seen, source);

  // initialzie output tree
  err = gvizGraphInit(out, 1);
  if (err < 0)
    return -1;
  err = gvizGraphAddVertex(out, NULL, NULL, NULL);
  if (err < 0)
    return -1;

  // DFS
  gvizArray *currNeighbors;
  size_t image, curr, currNeighbor;
  while (!gvizDequeIsEmpty(&stack)) {
    gvizDequePopRight(&stack, &curr);
    currNeighbors = gvizGraphGetVertexNeighbors(g, curr);

    for (size_t i = 0; i < currNeighbors->count; i++) {
      if (gvizTestBit(seen, i))
        continue;
      gvizSetBit(seen, i);
      currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);

      err = gvizGraphAddVertex(out, NULL, NULL, NULL);
      if (err < 0)
        return -1;

      invMap[currNeighbor] = out->vertices.count - 1;

      err = gvizGraphAddEdge(out, invMap[curr], out->vertices.count - 1);
      if (err < 0)
        return -1;

      image = g->map ? g->map[currNeighbor] : currNeighbor;
      err = gvizArrayPush(&map, &image);
      if (err < 0)
        return -1;

      err = gvizDequePush(&stack, &currNeighbor);
      if (err < 0)
        return -1;
    }
  }
  out->map = map.arr;

  gvizDequeRelease(&stack);
  return 0;
};

int gvizGraphBFSTree(gvizGraph *g, gvizGraph *out, size_t source,
                     size_t maxDepth, int inv) {
  int err;

  // queue elements carry both vertex id and BFS depth
  typedef struct {
    size_t v;
    size_t d;
  } NodeDepth;

  // init queue
  gvizDeque queue;
  err = gvizDequeInit(&queue, sizeof(NodeDepth));
  if (err < 0)
    return -1;

  // seen bitset over global vertex IDs
  GVIZ_BIT_UNIT seen[GVIZ_ARRAY_UNITS(g->vertices.count)];
  memset(seen, 0, sizeof(seen));
  gvizSetBit(seen, source);

  // global-id -> index in 'out'
  gvizArray invMap;
  gvizArrayInitAtCapacity(&invMap, sizeof(size_t), g->vertices.count);
  memset(invMap.arr, 0, sizeof(size_t) * invMap.capacity);

  // init out graph: one vertex for source
  err = gvizGraphInit(out, 1);
  if (err < 0)
    return -1;

  err = gvizGraphAddVertex(out, NULL, NULL, NULL);
  if (err < 0)
    return -1;

  // build map array: one entry per vertex in 'out'
  gvizArray map;
  err = gvizArrayInit(&map, sizeof(size_t));
  if (err < 0)
    return -1;

  size_t image = g->map ? g->map[source] : source;
  err = gvizArrayPush(&map, &image);
  if (err < 0)
    return -1;

  // enqueue start node with depth 0
  NodeDepth start = {.v = source, .d = 0};
  err = gvizDequePush(&queue, &start);
  if (err < 0)
    return -1;

  // BFS
  while (!gvizDequeIsEmpty(&queue)) {
    NodeDepth nd;
    gvizDequePopLeft(&queue, &nd);

    size_t curr = nd.v;
    size_t currDepth = nd.d;

    // if we've reached maxDepth, don't expand this node further
    if (maxDepth && currDepth >= maxDepth)
      continue;

    gvizArray *currNeighbors = gvizGraphGetVertexNeighbors(g, curr);

    for (size_t i = 0; i < currNeighbors->count; i++) {
      size_t currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);

      // seen keyed by vertex ID
      if (gvizTestBit(seen, currNeighbor))
        continue;
      gvizSetBit(seen, currNeighbor);

      // create vertex in out for this neighbor
      // Stores the depth in the data ptr info!
      // So the data ptr doesnt actually point to a valid address!
      err = gvizGraphAddVertex(out, (void *)(currDepth + 1), NULL, NULL);
      if (err < 0)
        return -1;

      size_t outIdx = out->vertices.count - 1;
      *(size_t *)(gvizArrayAtIndex(&invMap, currNeighbor)) = outIdx;

      // edge from curr's image to neighbor's image
      size_t currOutIdx = *(size_t *)(gvizArrayAtIndex(&invMap, curr));
      err = gvizGraphAddEdge(out, currOutIdx, outIdx);
      if (err < 0)
        return -1;

      // push external or internal id into map
      image = g->map ? g->map[currNeighbor] : currNeighbor;
      err = gvizArrayPush(&map, &image);
      if (err < 0)
        return -1;

      // enqueue neighbor with depth+1
      NodeDepth next = {.v = currNeighbor, .d = currDepth + 1};
      err = gvizDequePush(&queue, &next);
      if (err < 0)
        return -1;
    }
  }

  // out->map should now be map or invMap
  if (inv)
    out->map = invMap.arr;
  else
    out->map = map.arr;

  // for (size_t i = 0; i < GVIZ_ARRAY_UNITS(g->vertices.count); i++) {
  //   printf("%d, ", seen[i] == 0xFF);
  // }

  gvizDequeRelease(&queue);
  return 0;
}

int gvizGraphKNearestNeighbors(gvizGraph *g, size_t *out, size_t k,
                               size_t source, GVIZ_BIT_ARRAY filter) {
  if (k == 0)
    return 0;
  int err;

  // init queue
  gvizDeque queue;
  err = gvizDequeInit(&queue, sizeof(size_t));
  if (err < 0)
    return -1;

  // seen bitset over global vertex IDs
  GVIZ_BIT_UNIT seen[GVIZ_ARRAY_UNITS(g->vertices.count)];
  memset(seen, 0, sizeof(seen));
  gvizSetBit(seen, source);

  // enqueue start node with depth 0
  err = gvizDequePush(&queue, &source);
  if (err < 0)
    return -1;

  // BFS
  size_t curr, count = 0;
  while (!gvizDequeIsEmpty(&queue)) {

    gvizDequePopLeft(&queue, &curr);

    gvizArray *currNeighbors = gvizGraphGetVertexNeighbors(g, curr);

    for (size_t i = 0; i < currNeighbors->count; i++) {
      size_t currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);

      // seen keyed by vertex ID
      if (gvizTestBit(seen, currNeighbor))
        continue;
      gvizSetBit(seen, currNeighbor);

      // If satisfies filter, increment count and add to out
      if (!filter || gvizTestBit(filter, currNeighbor)) {
        out[count++] = currNeighbor;
        if (count >= k)
          break;
      }

      // enqueue neighbor
      err = gvizDequePush(&queue, &currNeighbor);
      if (err < 0)
        return -1;
    }
  }

  gvizDequeRelease(&queue);
  return count;
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
