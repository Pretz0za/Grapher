#include "../include/graph.h"
#include "../include/helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Graph *createGraph(size_t vertexCount, int directed, DataCopyFn copyData,
                   DataFreeFn freeData) {
  Graph *g = malloc(sizeof(Graph));
  if (g == NULL)
    exit(EXIT_FAILURE); // malloc fail
  if (vertexCount != 0) {
    g->vertices = malloc(vertexCount * sizeof(Vertex));
    if (g->vertices == NULL)
      exit(EXIT_FAILURE); // malloc fail
  } else {
    g->vertices = NULL;
  }
  g->directed = directed;
  g->size = vertexCount;
  g->count = 0;
  g->copyData = copyData;
  g->freeData = freeData;
  return g;
}

int addVertex(Graph *g, void *data) {
  if (g->count >= g->size)
    return -1;
  Vertex *vertex = malloc(sizeof(Vertex));
  if (vertex == NULL)
    exit(EXIT_FAILURE); // malloc fail
  vertex->neighbors = createVec();
  vertex->data = data;
  g->vertices[g->count++] = vertex;

  return 0;
}

int addEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = pushToVec(g->vertices[from]->neighbors, to);
  if (err == 0 && !g->directed) {
    err = pushToVec(g->vertices[to]->neighbors, from);
  }
  if (err)
    exit(EXIT_FAILURE); // realloc fail
  return 0;
}

int removeEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = deleteFromVec(g->vertices[from]->neighbors, to);
  if (err == 0 && !g->directed)
    err = deleteFromVec(g->vertices[to]->neighbors, from);
  return err;
}

void *getVertexData(Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return g->vertices[idx]->data;
}

Vector *neighbors(Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return g->vertices[idx]->neighbors;
}

int adjacent(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  return findInVec(g->vertices[from]->neighbors, to) == NULL ? 0 : 1;
}

void destroyVertex(Vertex *v, DataFreeFn freeData) {
  if (!v)
    return;
  if (v->neighbors)
    destroyVec(v->neighbors);
  if (v->data)
    freeData(v->data);
  free(v);
}

void destroyGraph(Graph *g) {
  if (g->vertices) {
    for (int i = 0; i < g->count; i++) {
      destroyVertex(g->vertices[i], g->freeData);
    }
    free(g->vertices);
  }
  free(g);
}

Graph *DepthFirstSearch(Graph *g, size_t from) {
  // NOTE: We cannot use deleteFromVec() as it reorderes the elements
  //       popVec(), however, does not. Besides, it is faster.
  Graph *out = createGraph(g->count, 1, g->copyData, g->freeData);
  Vector *frontier = createVec();
  Vector *currNeighbors;
  int visited[g->count];
  size_t gToDFS[g->count]; // map the index of a Vertex in g->vertices to its
                           // index in out->vertices
  size_t currNeighbor;
  size_t curr;
  memset(visited, 0, sizeof(visited));

  gToDFS[from] = 0;
  pushToVec(frontier, from);
  visited[from] = 1;
  addVertex(out, g->copyData(g->vertices[from]->data));
  while (!isEmpty(frontier)) {
    curr = popVec(frontier);
    currNeighbors = neighbors(g, curr);
    for (size_t i = 0; i < currNeighbors->count; i++) {
      currNeighbor = currNeighbors->arr[i];
      if (!visited[currNeighbor]) {
        visited[currNeighbor] = 1;
        pushToVec(frontier, currNeighbor);
        addVertex(out, g->copyData(g->vertices[currNeighbor]->data));
        gToDFS[currNeighbor] = out->count - 1;
        addEdge(out, gToDFS[curr], gToDFS[currNeighbor]);
      }
    }
  }

  destroyVec(frontier);

  return out;
}

void clearVertices(Graph *g) {
  if (g->count == 0)
    return;
  for (int i = 0; i < g->count; i++) {
    destroyVertex(g->vertices[i], g->freeData);
  }
  g->count = 0;
}

int copyGraph(Graph *dest, Graph *src) {
  if (dest->freeData != src->freeData || dest->copyData != src->copyData)
    return -1;
  if (dest->size < src->count)
    return 1;

  // if dest has too many vertices, destroy all extras
  while (dest->count > src->count) {
    destroyVertex(dest->vertices[--dest->count], dest->freeData);
  }

  // copy as many vertices into dest as we can
  for (int i = 0; i < dest->count; i++) {
    dest->freeData(dest->vertices[i]->data);
    dest->vertices[i]->data = src->copyData(src->vertices[i]->data);
    if (copyVec(dest->vertices[i]->neighbors, src->vertices[i]->neighbors) != 0)
      exit(EXIT_FAILURE); // realloc fail
  }

  // if we still didn't copy everything, create and copy the next vertex
  while (dest->count < src->count) {
    addVertex(dest, src->copyData(src->vertices[dest->count]->data));
    if (copyVec(dest->vertices[dest->count - 1]->neighbors,
                src->vertices[dest->count - 1]->neighbors) != 0)
      exit(EXIT_FAILURE); // realloc fail
    dest->count++;
  }

  dest->directed = src->directed;

  return 0;
}

int copyReversedGraph(Graph *dest, Graph *src) {
  if (!src->directed)
    return copyGraph(dest, src);
  if (dest->freeData != src->freeData || dest->copyData != src->copyData)
    return -1;
  if (dest->size < src->count)
    return 1;

  // if dest has too many vertices, destroy all extras
  while (dest->count > src->count) {
    destroyVertex(dest->vertices[--dest->count], dest->freeData);
  }

  // if dest has too little vertices, copy the data of what we have
  for (int i = 0; dest->count < src->count && i < dest->count; i++) {
    dest->freeData(dest->vertices[i]->data);
    dest->vertices[i]->data = src->copyData(src->vertices[i]->data);
  }

  // if we still didn't copy everything, create and copy the next vertex
  while (dest->count < src->count) {
    addVertex(dest, src->copyData(src->vertices[dest->count]->data));
  }

  dest->directed = src->directed;

  Vertex *curr;
  for (int i = 0; i < src->count; i++) {
    curr = src->vertices[i];
    for (int j = 0; j < curr->neighbors->count; j++) {
      addEdge(dest, curr->neighbors->arr[j], i);
    }
  }

  return 0;
}

int printSubtree(Graph *tree, size_t root, Position rootPos, char *strings[],
                 FILE *stream) {
  Position newPos;
  Vector *currNeighbors = neighbors(tree, root);
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
