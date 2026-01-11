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
  Vector *frontier = createVec();
  Vector *expansionOrder = createVec();
  int visited[g->size];
  memset(visited, 0x00, sizeof(visited));
  pushToVec(frontier, from);
  Vector *currNeighbors = NULL;
  size_t curr;
  visited[0] = 1;
  while (!isEmpty(frontier)) {
    curr = popVec(frontier);
    pushToVec(expansionOrder, curr);
    currNeighbors = neighbors(g, curr);
    for (int idx = 0; idx < currNeighbors->count; idx++) {
      if (visited[currNeighbors->arr[idx]] == 0) {
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

void printDFSTree(Graph *g, Vector *dfs, char *strings[], FILE *stream) {
  clearScreen(stream);
  char str[32];
  Position positions[dfs->count];
  int parents[dfs->count];
  int childrenRemaining[dfs->count];
  int parentIdx = -1;
  Vertex *curr;
  memset(positions, 0, sizeof(positions));
  memset(childrenRemaining, 0, sizeof(childrenRemaining));

  for (int i = 0; i < dfs->count; i++) {
    parents[i] = -1;
  }

  parents[0] = -2;

  for (int i = 0; i < dfs->count; i++) {
    curr = g->vertices[dfs->arr[i]];
    for (int j = 0; j < curr->neighbors->count; j++) {
      if (parents[curr->neighbors->arr[j]] == -1) {
        parents[curr->neighbors->arr[j]] = dfs->arr[i];
        childrenRemaining[dfs->arr[i]]++;
      }
    }
  }

  for (int i = 0; i < dfs->count; i++) {
    if (!strings)
      snprintf(str, sizeof(str), "%zu", dfs->arr[i]);
    else
      strcpy(str, strings[dfs->arr[i]]);
    curr = g->vertices[dfs->arr[i]];

    if (i != 0) {
      parentIdx = parents[dfs->arr[i]];
      childrenRemaining[parentIdx]--;

      printAt(positions[parentIdx].x, positions[parentIdx].y + 1, "│", stream);
      printAt(positions[parentIdx].x, positions[parentIdx].y + 2, "└", stream);
      printAt(positions[parentIdx].x + 1, positions[parentIdx].y + 2, str,
              stream);
      positions[dfs->arr[i]].x = positions[parentIdx].x + 1;
      positions[dfs->arr[i]].y = positions[parentIdx].y + 2;

      positions[parentIdx].y += 2;
      parentIdx = parents[parentIdx];

      while (parentIdx != -2) {
        if (childrenRemaining[parentIdx] > 0) {
          printAt(positions[parentIdx].x, positions[parentIdx].y + 1, "│",
                  stream);

          printAt(positions[parentIdx].x, positions[parentIdx].y + 2, "│",
                  stream);
          positions[parentIdx].y += 2;
        }

        parentIdx = parents[parentIdx];
      }
    } else {
      printAt(1, 1, str, stream);
      positions[dfs->arr[i]].x = 1;
      positions[dfs->arr[i]].y = 1;
    }
  }
};
