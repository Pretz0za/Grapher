#include "dsa/gvizGraph.h"

typedef struct {
  gvizGraph *g;
  size_t t, l, r; // indices into g->vertices for the three corners
} SierpinskiTriangle;

/**
 * Recursively builds a Sierpinski triangle of a given depth into an existing
 * graph. Rather than allocating new corner vertices, the caller passes in the
 * indices of the three corners (already added to g), so adjacent sub-triangles
 * naturally share vertices.
 *
 * @param g     The graph being built into (must be initialized, undirected).
 * @param depth Recursion depth. 0 = a single triangle (3 edges, no new verts).
 * @param t     Index of the top corner vertex in g.
 * @param l     Index of the left corner vertex in g.
 * @param r     Index of the right corner vertex in g.
 * @return 0 on success, -1 on allocation failure.
 */
static int sierpinskiRecurse(gvizGraph *g, int depth, size_t t, size_t l,
                             size_t r);

/**
 * Initializes @p out as an undirected graph and fills it with the Sierpinski
 * triangle of the given depth.
 *
 * Vertex count  = 3 * (3^depth - 1) / 2  + 3   (closed form)
 * Edge count    = 3 * 3^depth
 *
 * @param out   Pointer to an uninitialized gvizGraph.
 * @param depth 0 = single triangle, 1 = three triangles, etc.
 * @param st    Optional: filled with the indices of the three outer corners
 *              (t, l, r) if non-NULL.
 * @return 0 on success, -1 on allocation failure.
 */

typedef struct {
  gvizGraph *g;
  size_t a, b, c, d; // indices of the four corners of the tetrahedron
} SierpinskiTetrahedron;

/** Like sierpinskiRecurse, but for a tetrahedron with corners @p a–@p d. */
static int sierpinskiTetrahedronRecurse(gvizGraph *g, int depth, size_t a,
                                        size_t b, size_t c, size_t d);

/** Builds an undirected Sierpinski tetrahedron graph of the given @p depth. */
gvizGraph createSierpinskiTetrahedron(int depth, SierpinskiTetrahedron *st);

/** Builds an undirected Sierpinski triangle graph of the given @p depth. */
gvizGraph createSierpinski(int depth, SierpinskiTriangle *st);

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

int isConnected(gvizGraph *g);
