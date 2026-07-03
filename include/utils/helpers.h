#ifndef __GRAPH_HELPERS_H__
#define __GRAPH_HELPERS_H__

#include "ds/gvizArray.h"
#include "ds/gvizGraph.h"
#include <stddef.h>
#include <stdio.h>

/** Swaps the values at @p a and @p b in place. */
void xorSwap(size_t *a, size_t *b);

/** Returns 1 if @p idx is a valid vertex index in @p g, 0 otherwise. */
int inBoundsVertex(const gvizGraph *g, size_t idx);

/** Returns 1 if both indices are valid vertex indices in @p g, 0 otherwise. */
int inBoundsVertices(const gvizGraph *g, size_t idx1, size_t idx2);

/**
 * Appends @p data to @p vertex's adjacency list.
 *
 * @return 0 on success, -1 on reallocation failure.
 */
int pushToNeighbors(gvizVertex *vertex, size_t data);

/**
 * Removes the first occurrence of @p data from @p vertex's adjacency list.
 *
 * @return 0 on success, -1 if not found.
 */
int removeFromNeighbors(gvizVertex *vertex, size_t data);

/** Moves the terminal cursor to (@p x, @p y) and prints @p line using ANSI escapes. */
void printAt(int x, int y, char *line, FILE *stream);

/** Clears the terminal screen using ANSI escapes. */
void clearScreen(FILE *stream);

/**
 * Computes the coordinate-wise mean of positions indexed by @p indices.
 * Writes the result into @p out (must hold @p dimension doubles).
 */
double *barrycenter(size_t dimension, const double *positions,
                    const gvizArray *indices, double *out);

#endif
