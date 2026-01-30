#include "../include/graph.h"
#include "../include/helpers.h"
#include <_string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Graph *createGraph(int directed, DataType dataType) {
  Graph *g = malloc(sizeof(Graph));
  if (g == NULL)
    exit(EXIT_FAILURE); // malloc fail
  g->vertices = malloc(64 * sizeof(Vertex));
  if (g->vertices == NULL)
    exit(EXIT_FAILURE); // malloc fail

  g->directed = directed;
  g->size = 64;
  g->count = 0;
  g->dataType = dataType;
  g->map = NULL;
  return g;
}

int addVertex(Graph *g, void *data) {
  if (g->count >= g->size) {
    printf("REALLOCATING VERTICES GRAPH...\n");
    Vertex **newVertices =
        realloc(g->vertices, sizeof(Vertex *) * g->count * 2);
    if (newVertices == NULL) {
      exit(EXIT_FAILURE);
    }
    g->size = g->count * 2;
    g->vertices = newVertices;
  }
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

void destroyVertex(Vertex *v, DataType dataType) {
  if (!v)
    return;
  if (v->neighbors)
    destroyVec(v->neighbors);
  if (v->data)
    freeData(v->data, dataType);
  free(v);
}

void destroyGraph(Graph *g) {
  if (g->vertices) {
    for (int i = 0; i < g->count; i++) {
      destroyVertex(g->vertices[i], g->dataType);
    }
    free(g->vertices);
  }
  if (g->map)
    free(g->map);
  free(g);
}

Graph *DepthFirstSearch(Graph *g, size_t from) {
  // NOTE: We cannot use deleteFromVec() as it reorderes the elements
  //       popVec(), however, does not. Besides, it is faster.
  Graph *out = createGraph(1, NODATA);
  Vector *frontier = createVec();
  Vector *currNeighbors;
  int visited[g->count];
  int *map = malloc(sizeof(int) * g->count);
  if (!map) {
    exit(EXIT_FAILURE);
  }
  size_t currNeighbor;
  size_t curr;
  memset(visited, 0, sizeof(visited));

  addVertex(out, NULL);
  map[0] = from;
  pushToVec(frontier, 0);
  visited[from] = 1;
  while (!isEmpty(frontier)) {
    curr = popVec(frontier);
    currNeighbors = neighbors(g, map[curr]);
    for (size_t i = 0; i < currNeighbors->count; i++) {
      currNeighbor = currNeighbors->arr[i];
      if (!visited[currNeighbor]) {
        visited[currNeighbor] = 1;
        addVertex(out, NULL);
        pushToVec(frontier, out->count - 1);
        map[out->count - 1] = g->map ? g->map[currNeighbor] : currNeighbor;
        addEdge(out, curr, out->count - 1);
      }
    }
  }

  int *tmp = realloc(map, out->count * sizeof(int));
  if (!tmp) {
    exit(EXIT_FAILURE);
  }
  map = tmp;
  out->map = map;

  destroyVec(frontier);

  return out;
}

void clearVertices(Graph *g) {
  if (g->count == 0)
    return;
  for (int i = 0; i < g->count; i++) {
    destroyVertex(g->vertices[i], g->dataType);
  }
  g->count = 0;
}

int copyGraph(Graph *dest, Graph *src) {
  if (dest->dataType == src->dataType)
    return -1;
  if (dest->size < src->count)
    return 1;

  // if dest has too many vertices, destroy all extras
  while (dest->count > src->count) {
    destroyVertex(dest->vertices[--dest->count], dest->dataType);
  }

  // copy as many vertices into dest as we can
  for (int i = 0; i < dest->count; i++) {
    freeData(getVertexData(dest, i), dest->dataType);
    dest->vertices[i]->data = copyData(getVertexData(src, i), src->dataType);
    if (copyVec(dest->vertices[i]->neighbors, src->vertices[i]->neighbors) != 0)
      exit(EXIT_FAILURE); // realloc fail
  }

  // if we still didn't copy everything, create and copy the next vertex
  while (dest->count < src->count) {
    addVertex(dest, copyData(getVertexData(src, dest->count), src->dataType));
    if (copyVec(neighbors(dest, dest->count - 1),
                neighbors(src, dest->count - 1)) != 0)
      exit(EXIT_FAILURE); // realloc fail
    dest->count++;
  }

  dest->directed = src->directed;

  return 0;
}

int copyReversedGraph(Graph *dest, Graph *src) {
  if (!src->directed)
    return copyGraph(dest, src);
  if (dest->dataType != src->dataType)
    return -1;
  if (dest->size < src->count)
    return 1;

  // if dest has too many vertices, destroy all extras
  while (dest->count > src->count) {
    destroyVertex(dest->vertices[--dest->count], dest->dataType);
  }

  // if dest has too little vertices, copy the data of what we have
  for (int i = 0; dest->count < src->count && i < dest->count; i++) {
    freeData(getVertexData(dest, i), dest->dataType);
    dest->vertices[i]->data = copyData(getVertexData(src, i), src->dataType);
  }

  // if we still didn't copy everything, create and copy the next vertex
  while (dest->count < src->count) {
    addVertex(dest, copyData(getVertexData(src, dest->count), src->dataType));
  }

  dest->directed = src->directed;

  for (int i = 0; i < src->count; i++) {
    for (int j = 0; j < neighbors(src, i)->count; j++) {
      addEdge(dest, neighbors(src, i)->arr[j], i);
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

void *parseDataLine(const char *line, DataType dataType) {
  void *out;

  switch (dataType) {
  case NODATA:
    out = NULL;
    break;
  case CHARDATA:
    out = malloc(sizeof(char));
    *(char *)out = *line;
    break;
  case STRDATA:
    out = strdup(line);
    break;
  case IMGDATA:
    break;
  default:
    break;
  }

  return out;
}

Vector *parseAdjacencyListLine(const char *line) {
  Vector *out = createVec();
  size_t curr;
  char *endptr;
  char *cpy = strdup(line);
  char *currStr = strtok(cpy, " ");
  while (currStr != NULL) {
    curr = (size_t)strtol(currStr, &endptr, 10);
    if (*endptr == 0 && *currStr != 0) { // successful parse
      pushToVec(out, curr);
    }

    currStr = strtok(NULL, " ");
  }

  return out;
}

Graph *readGraphFile(const char *path) {
  FILE *pFile = fopen(path, "r");
  if (!pFile) {
    return NULL;
  }

  size_t size;
  char buffer[MAX_LINE_SIZE];
  char *endptr;
  int directed;
  int tree;
  int fail = 0;
  DataType dataType;

  if (fgets(buffer, sizeof(buffer), pFile) != NULL) {
    char *curr = buffer;
    char *prev = NULL;
    for (; curr; prev = curr++) {
      switch (*curr) {
      case 'T':
        tree = 1;
      case 'D':
        directed = 1;
        break;

      case '\n':
        if (prev) {
          dataType = (DataType)strtol(buffer, &endptr, 10);
          if (endptr == buffer) {
            dataType = NODATA;
          }
        } else
          dataType = NODATA;
        break;
      }
    }
  } else {
    // No lines in file. Invalid.
    fail = 1;
  }

  if (fgets(buffer, sizeof(buffer), pFile) != NULL) {
    size = (size_t)strtol(buffer, &endptr, 10);
    if (*endptr == 0 && *buffer != 0) {
      fail = 1;
    }
  } else {
    // No second line with vertex count in file. Invalid.
    fail = 1;
  }

  if (fail) {
    fclose(pFile);
    return NULL;
  }

  Graph *out = createGraph(directed, dataType);

  for (size_t i = 0; i < size; i++) {
    if (fgets(buffer, sizeof(buffer), pFile) != NULL) {
      addVertex(out, parseDataLine(buffer, dataType));
    } else {
      // Missing vertex data
      // TODO: maybe handle differently
      fclose(pFile);
      destroyGraph(out);
      return NULL;
    }
  }

  size_t curr;
  size_t i = 0;

  while (fgets(buffer, sizeof(buffer), pFile) != NULL) {
    char *cpy = strdup(buffer);
    char *currStr = strtok(cpy, " ");
    while (currStr != NULL) {
      curr = (size_t)strtol(currStr, &endptr, 10);
      if (*endptr == 0 && *currStr != 0) { // successful parse
        if (addEdge(out, i, curr) != 0) {  // invalid edge
          fclose(pFile);
          destroyGraph(out);
          return NULL;
        };
      }

      currStr = strtok(NULL, " ");
    }
    i++;
  }

  fclose(pFile);
  return out;
}

void *copyData(void *data, DataType dataType) {
  void *cpy;
  switch (dataType) {
  case NODATA:
    return NULL;
  case CHARDATA:
    cpy = malloc(sizeof(char));
    *(char *)cpy = *(char *)data;
    return cpy;
  case STRDATA:
    cpy = strdup(data);
    return cpy;
  case IMGDATA:
    return NULL;
  default:
    return NULL;
  }
}

void freeData(void *data, DataType dataType) {
  switch (dataType) {
  case NODATA:
    return;
  case CHARDATA:
    free(data);
  case STRDATA:
    free(data);
  case IMGDATA:
    return;
  default:
    return;
  }
}
