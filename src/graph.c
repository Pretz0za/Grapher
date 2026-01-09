#include "../include/graph.h"
#include "../include/helpers.h"
#include <stdio.h>
#include <stdlib.h>

Graph *createGraph(size_t vertexCount, int directed, int weighted) {
  Graph *g = malloc(sizeof(Graph));
  g->vertices = malloc(vertexCount * sizeof(Vertex));
  g->directed = directed;
  g->weighted = weighted;
  return g;
}

Vertex *createVertex(void *data) {
  Vertex *vertex = malloc(sizeof(Vertex));
  vertex->neighbors = malloc(4 * sizeof(size_t));
  vertex->data = data;
  vertex->capacity = 4;
  vertex->count = 0;
  return vertex;
}

int addEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = pushToNeighbors(g->vertices[from], to);
  if (err == 0 && !g->directed) {
    err = pushToNeighbors(g->vertices[to], from);
  }
  return err;
}

int removeEdge(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  err = removeFromNeighbors(g->vertices[from], to);
  if (err == 0 && !g->directed)
    err = removeFromNeighbors(g->vertices[to], from);
  return err;
}

void *getVertexData(Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return g->vertices[idx]->data;
}

size_t *neighbors(Graph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return g->vertices[idx]->neighbors;
}

int adjacent(Graph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  for (int idx = 0; idx < g->vertices[from]->count; idx++) {
    if (g->vertices[from]->neighbors[idx] == to)
      return 1;
  }
  return 0;
}
