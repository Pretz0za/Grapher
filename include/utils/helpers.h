#ifndef __GRAPH_HELPERS_H__
#define __GRAPH_HELPERS_H__

#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include <stddef.h>
#include <stdio.h>

void xorSwap(size_t *a, size_t *b);

int inBoundsVertex(const gvizGraph *g, size_t idx);
int inBoundsVertices(const gvizGraph *g, size_t idx1, size_t idx2);

int pushToNeighbors(gvizVertex *vertex, size_t data);
int removeFromNeighbors(gvizVertex *vertex, size_t data);

void printAt(int x, int y, char *line, FILE *stream);
void clearScreen(FILE *stream);

double *barrycenter(size_t dimension, const double *positions,
                    const gvizArray *indices, double *out);

#endif
