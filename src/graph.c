#include "../include/graph.h"
#include "../include/helpers.h"
#include "../include/stack.h"
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
  for (int i = 0; i < g->count; i++) {
    destroyVertex(g->vertices[i], g->freeData);
  }
  if (g->vertices) {
    free(g->vertices);
  }
  free(g);
}

Vector *DepthFirstSearch(Graph *g, size_t from) {
  // NOTE: We cannot use deleteFromVec() as it reorderes the elements
  //       popVec(), however, does not. Besides, it is faster.
  printf("intializing vectors. Graph size %lu\n", g->size);
  Vector *frontier = createVec();
  Vector *expansionOrder = createVec();
  int visited[g->size];
  memset(visited, 0x00, sizeof(visited));
  pushToVec(frontier, from);
  Vector *currNeighbors = NULL;
  size_t curr;
  printf("begining while loop\n");
  visited[0] = 1;
  while (!isEmpty(frontier)) {
    printf("popping\n");
    curr = popVec(frontier);
    printf("popped %d from frontier\n", (int)curr);
    pushToVec(expansionOrder, curr);
    printf("pushed to expansion vector\n");

    currNeighbors = neighbors(g, curr);
    printf("looping thorugh %lu neighbors\n", currNeighbors->count);
    for (int idx = 0; idx < currNeighbors->count; idx++) {
      if (visited[currNeighbors->arr[idx]] == 0) {
        printf("found unvisited neighbor %d: has index %lu\n", idx,
               currNeighbors->arr[idx]);
        visited[currNeighbors->arr[idx]] = 1;
        pushToVec(frontier, currNeighbors->arr[idx]);
      }
    }
  }
  destroyVec(frontier);
  return expansionOrder;
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

  return 0;
}
