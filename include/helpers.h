#ifndef __GRAPH_HELPERS_H__
#define __GRAPH_HELPERS_H__

#include "./graph.h"
#include <stddef.h>
#include <stdio.h>

void xorSwap(size_t *a, size_t *b);

int inBoundsVertex(Graph *g, size_t idx);
int inBoundsVertices(Graph *g, size_t idx1, size_t idx2);

int pushToNeighbors(Vertex *vertex, size_t data);
int removeFromNeighbors(Vertex *vertex, size_t data);

void printAt(int x, int y, char *line, FILE *stream);
void clearScreen(FILE *stream);

#endif
