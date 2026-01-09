#ifndef GRAPH
#define GRAPH

#include <stddef.h>

typedef struct Vertex {
  size_t *neighbors;
  size_t count;
  size_t capacity;
  void *data;
} Vertex;

typedef struct {
  Vertex **vertices;
  size_t count;
  size_t size;
  int directed;
} Graph;

[[nodiscard]] Graph *createGraph(size_t vertexCount, int directed);
int addVertex(Graph *g, void *data);
int addEdge(Graph *g, size_t from, size_t to);
int removeEdge(Graph *g, size_t from, size_t to);
void *getVertexData(Graph *g, size_t idx);
size_t *neighbors(Graph *g, size_t idx);
int adjacent(Graph *g, size_t from, size_t to);

void destroyVertex(Vertex *v);
void destroyGraph(Graph *g);

#endif
