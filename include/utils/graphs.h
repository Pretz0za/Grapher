#ifndef GVIZ_UTILS_GRAPHS_H
#define GVIZ_UTILS_GRAPHS_H

#include "ds/gvizGraph.h"

/*
 * Synthetic test-graph builders. Each returns an initialized undirected
 * gvizGraph by value; the caller owns it and must call gvizGraphRelease.
 * On allocation failure a zeroed graph (vertices.arr == NULL) is returned.
 */

/** Indices of the three outer corners of a Sierpinski triangle. */
typedef struct {
  size_t t, l, r;
} SierpinskiTriangle;

/** Indices of the four corners of a Sierpinski tetrahedron. */
typedef struct {
  size_t a, b, c, d;
} SierpinskiTetrahedron;

/**
 * Builds an undirected Sierpinski triangle graph of the given @p depth.
 *
 * Vertex count = 3 * (3^depth - 1) / 2 + 3, edge count = 3^(depth+1).
 *
 * @param st Optional: filled with the indices of the three outer corners.
 */
gvizGraph createSierpinski(int depth, SierpinskiTriangle *st);

/**
 * Builds an undirected Sierpinski tetrahedron graph of the given @p depth.
 *
 * @param st Optional: filled with the indices of the four outer corners.
 */
gvizGraph createSierpinskiTetrahedron(int depth, SierpinskiTetrahedron *st);

/** Builds the grid graph of a Sierpinski carpet at @p depth. */
gvizGraph build_sierpinski_carpet(size_t depth);

/** Builds a tetrahedral mesh graph refined to @p depth. */
gvizGraph build_tetrahedral_mesh(size_t depth);

/** Builds an L-by-W rectangular grid mesh (undirected). */
gvizGraph build_rect_mesh(size_t L, size_t W);

/** Builds an equilateral triangular mesh with @p depth rows. */
gvizGraph build_equilateral_tri_mesh(size_t depth);

/** Builds an L-by-W grid with periodic boundary identifications (knotted). */
gvizGraph build_knotted_rect_mesh(size_t L, size_t W);

/** Builds a Möbius strip mesh with @p rows by @p cols vertices. */
gvizGraph build_mobius_strip(size_t rows, size_t cols);

/** Builds a Klein bottle mesh with @p rows by @p cols vertices. */
gvizGraph build_klein_bottle(size_t rows, size_t cols);

/**
 * Builds a random connected undirected graph via a random spanning tree plus
 * extra random edges.
 *
 * A spanning tree is grown first: for i = 1..numVertices-1, vertex i is wired
 * to a uniformly random earlier vertex in [0, i), which guarantees the whole
 * graph ends up connected. Extra edges are then added between random distinct
 * vertex pairs until roughly @p edgeDensity of the non-tree edges (out of the
 * n*(n-1)/2 - (n-1) possible) are present.
 *
 * @param numVertices Number of vertices to create.
 * @param edgeDensity Fraction in [0, 1] of the non-tree edges to add on top
 *                    of the spanning tree. 0 = tree only, 1 = complete graph.
 *                    Clamped into [0, 1].
 * @param seed        Seed for the random number generator (reproducibility).
 */
gvizGraph build_random_connected_graph(size_t numVertices, double edgeDensity,
                                       unsigned int seed);

/** Returns 1 if every vertex of @p g is reachable from vertex 0. Rebuilds
 *  @p g->layout. */
int isConnected(gvizGraph *g);

#endif
