#ifndef GVIZ_GRAPHS_HPP
#define GVIZ_GRAPHS_HPP

#include "Graph.hpp"

#include <cstddef>

namespace gviz::graphs {

/*
 * Synthetic test-graph builders, ported from utils/graphs.h. Each returns
 * an initialized undirected Graph by value; there is no more "return a
 * zeroed graph on allocation failure" convention -- std::bad_alloc just
 * propagates out of Graph's underlying std::vector allocations like
 * anywhere else in this port, so callers that want to catch it can, and
 * the (overwhelming) majority that don't need not check anything.
 */

/** Indices of the three outer corners of a Sierpinski triangle. */
struct SierpinskiTriangle {
  size_t t, l, r;
};

/** Indices of the four corners of a Sierpinski tetrahedron. */
struct SierpinskiTetrahedron {
  size_t a, b, c, d;
};

/**
 * Builds an undirected Sierpinski triangle graph of the given @p depth.
 *
 * Vertex count = 3 * (3^depth - 1) / 2 + 3, edge count = 3^(depth+1).
 *
 * @param outCorners Optional: filled with the indices of the three outer
 *                    corners.
 */
Graph CreateSierpinski(int depth, SierpinskiTriangle *outCorners = nullptr);

/**
 * Builds an undirected Sierpinski tetrahedron graph of the given @p depth.
 *
 * @param outCorners Optional: filled with the indices of the four outer
 *                    corners.
 */
Graph CreateSierpinskiTetrahedron(int depth, SierpinskiTetrahedron *outCorners = nullptr);

/** Builds the grid graph of a Sierpinski carpet at @p depth. */
Graph BuildSierpinskiCarpet(size_t depth);

/** Builds a tetrahedral mesh graph refined to @p depth. */
Graph BuildTetrahedralMesh(size_t depth);

/** Builds a @p length-by-@p width rectangular grid mesh (undirected). */
Graph BuildRectMesh(size_t length, size_t width);

/** Builds an equilateral triangular mesh with @p depth rows. */
Graph BuildEquilateralTriMesh(size_t depth);

/** Builds a @p length-by-@p width grid with periodic boundary
 *  identifications (knotted). */
Graph BuildKnottedRectMesh(size_t length, size_t width);

/** Builds a Mobius strip mesh with @p rows by @p cols vertices. */
Graph BuildMobiusStrip(size_t rows, size_t cols);

/** Builds a Klein bottle mesh with @p rows by @p cols vertices. */
Graph BuildKleinBottle(size_t rows, size_t cols);

/**
 * Builds a random connected undirected graph via a random spanning tree
 * plus extra random edges.
 *
 * A spanning tree is grown first: for i = 1..numVertices-1, vertex i is
 * wired to a uniformly random earlier vertex in [0, i), which guarantees
 * the whole graph ends up connected. Extra edges are then added between
 * random distinct vertex pairs until roughly @p edgeDensity of the
 * non-tree edges (out of the n*(n-1)/2 - (n-1) possible) are present.
 *
 * @param numVertices Number of vertices to create.
 * @param edgeDensity Fraction in [0, 1] of the non-tree edges to add on top
 *                     of the spanning tree. 0 = tree only, 1 = complete
 *                     graph. Clamped into [0, 1].
 * @param seed        Seed for the random number generator (reproducibility).
 */
Graph BuildRandomConnectedGraph(size_t numVertices, double edgeDensity, unsigned int seed);

/**
 * Returns whether every vertex of @p g is reachable from every other vertex
 * (true trivially for 0 or 1 vertices).
 *
 * Port note: the old C isConnected(gvizGraph*) rebuilt g->layout and ran a
 * BFS into a full subgraph just to compare its vertex count against the
 * whole graph's -- which required a non-const gvizGraph* purely to satisfy
 * gvizGraphBuildLayout, and paid for a layout (O(V+E)) and an edge bitset
 * neither of which the question "is this connected" actually needs. This
 * port instead marks every vertex present in a vertex-induced Subgraph (no
 * layout, no edge bitset -- see Subgraph.hpp) and asks
 * search::ConnectedComponents whether that comes out to at most one
 * component. Graph::AddVertex/BuildLayout etc. all leave @p g's public
 * surface const-correct enough that this genuinely never needs to mutate
 * @p g, unlike the old signature.
 */
bool IsConnected(const Graph &g);

} // namespace gviz::graphs

#endif
