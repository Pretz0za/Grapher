#ifndef HELPERS
#define HELPERS

#include "./graph.h"
#include <stddef.h>

void xorSwap(size_t *a, size_t *b);

int inBoundsVertex(Graph *g, size_t idx);
int inBoundsVertices(Graph *g, size_t idx1, size_t idx2);

int pushToNeighbors(Vertex *vertex, size_t data);
int removeFromNeighbors(Vertex *vertex, size_t data);

#endif
