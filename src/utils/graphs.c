#include "utils/graphs.h"
#include "core/alloc.h"
#include "dsa/gvizGraph.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int sierpinskiRecurse(gvizGraph *g, int depth, size_t t, size_t l,
                             size_t r) {
  if (depth == 0) {
    // Base case: just wire the three corners into a triangle.
    if (gvizGraphAddEdge(g, t, l) < 0)
      return -1;
    if (gvizGraphAddEdge(g, l, r) < 0)
      return -1;
    if (gvizGraphAddEdge(g, r, t) < 0)
      return -1;
    return 0;
  }

  /*
   * Split into three sub-triangles by introducing midpoint vertices:
   *
   *          t
   *         / \
   *        mt--tr       mt = midpoint of (t,l)
   *       / \ / \       tr = midpoint of (t,r)
   *      l--ml---r      ml = midpoint of (l,r)
   *
   * The three sub-triangles are:
   *   top:   (t,  mt, tr)
   *   left:  (mt, l,  ml)
   *   right: (tr, ml, r )
   *
   * mt, tr, ml are new vertices we add now. They will be passed as corners
   * to exactly two sub-triangles each, so edges on shared sides won't be
   * double-added (each sub-triangle only draws its own three edges at depth 0).
   */

  size_t mt = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;

  size_t tr = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;

  size_t ml = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;

  // Top sub-triangle
  if (sierpinskiRecurse(g, depth - 1, t, mt, tr) < 0)
    return -1;
  // Left sub-triangle
  if (sierpinskiRecurse(g, depth - 1, mt, l, ml) < 0)
    return -1;
  // Right sub-triangle
  if (sierpinskiRecurse(g, depth - 1, tr, ml, r) < 0)
    return -1;

  return 0;
}

static int sierpinskiTetrahedronRecurse(gvizGraph *g, int depth, size_t a,
                                        size_t b, size_t c, size_t d) {
  if (depth == 0) {
    // Base case: wire four corners into a complete graph K4 (tetrahedron)
    if (gvizGraphAddEdge(g, a, b) < 0)
      return -1;
    if (gvizGraphAddEdge(g, a, c) < 0)
      return -1;
    if (gvizGraphAddEdge(g, a, d) < 0)
      return -1;
    if (gvizGraphAddEdge(g, b, c) < 0)
      return -1;
    if (gvizGraphAddEdge(g, b, d) < 0)
      return -1;
    if (gvizGraphAddEdge(g, c, d) < 0)
      return -1;
    return 0;
  }

  /*
   * A tetrahedron has 4 corners and 6 edges.
   * We introduce one midpoint per edge — 6 new vertices:
   *
   *   ab = midpoint of (a, b)
   *   ac = midpoint of (a, c)
   *   ad = midpoint of (a, d)
   *   bc = midpoint of (b, c)
   *   bd = midpoint of (b, d)
   *   cd = midpoint of (c, d)
   *
   * This splits the tetrahedron into 4 sub-tetrahedra at the corners
   * and 1 octahedron in the middle. The Sierpinski tetrahedron discards
   * the central octahedron and recurses into the 4 corner sub-tetrahedra:
   *
   *   corner a: (a,  ab, ac, ad)
   *   corner b: (ab, b,  bc, bd)
   *   corner c: (ac, bc, c,  cd)
   *   corner d: (ad, bd, cd, d )
   *
   * Each midpoint vertex is shared by exactly 2 sub-tetrahedra,
   * so edges on shared faces won't be double-added.
   */

  size_t ab = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;
  size_t ac = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;
  size_t ad = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;
  size_t bc = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;
  size_t bd = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;
  size_t cd = g->vertices.count;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return -1;

  if (sierpinskiTetrahedronRecurse(g, depth - 1, a, ab, ac, ad) < 0)
    return -1;
  if (sierpinskiTetrahedronRecurse(g, depth - 1, ab, b, bc, bd) < 0)
    return -1;
  if (sierpinskiTetrahedronRecurse(g, depth - 1, ac, bc, c, cd) < 0)
    return -1;
  if (sierpinskiTetrahedronRecurse(g, depth - 1, ad, bd, cd, d) < 0)
    return -1;

  return 0;
}

gvizGraph createSierpinskiTetrahedron(int depth, SierpinskiTetrahedron *st) {

  size_t capacity = 4;
  for (int i = 0; i < depth; i++)
    capacity = 4 * capacity - 6;

  gvizGraph out;
  if (gvizGraphInitAtCapacity(&out, 0, capacity) < 0)
    exit(1);

  size_t a = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);
  size_t b = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);
  size_t c = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);
  size_t d = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);

  if (sierpinskiTetrahedronRecurse(&out, depth, a, b, c, d) < 0) {
    gvizGraphRelease(&out);
    exit(1);
  }

  if (st) {
    st->g = &out;
    st->a = a;
    st->b = b;
    st->c = c;
    st->d = d;
  }

  printf("created Sierpinski tetrahedron with depth %d (%zu vertices)\n", depth,
         out.vertices.count);
  return out;
}

gvizGraph createSierpinski(int depth, SierpinskiTriangle *st) {
  // Pre-compute vertex count so we can init at the right capacity.
  // V(0)=3, V(n) = 3*V(n-1) - 3  =>  V(n) = (3^(n+1) + 3) / 2
  size_t capacity = 3;
  for (int i = 0; i < depth; i++)
    capacity = 3 * capacity - 3;

  gvizGraph out;
  if (gvizGraphInitAtCapacity(&out, /*directed=*/0, capacity) < 0)
    exit(1);

  // Add the three fixed outer corners first.
  size_t t = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);

  size_t l = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);

  size_t r = out.vertices.count;
  if (gvizGraphAddVertex(&out, NULL, NULL, NULL) < 0)
    exit(1);

  if (sierpinskiRecurse(&out, depth, t, l, r) < 0) {
    gvizGraphRelease(&out);
    exit(1);
  }

  if (st) {
    st->g = &out;
    st->t = t;
    st->l = l;
    st->r = r;
  }

  printf("created Sierpinski graph with depth %d (%zu vertices)\n", depth,
         out.vertices.count);
  return out;
}

gvizGraph build_sierpinski_carpet(size_t depth) {
  size_t dim = 1;
  for (size_t k = 0; k < depth; k++)
    dim *= 3; // dim = 3^depth

  // first pass: determine which grid points are in the carpet
  size_t total = dim * dim;
  GVIZ_BIT_ARRAY in_carpet =
      GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(total));
  memset(in_carpet, 0, sizeof(GVIZ_BIT_UNIT) * (GVIZ_ARRAY_UNITS(total)));

  size_t *node_id = GVIZ_ALLOC(total * sizeof(size_t));
  memset(node_id, 0xFF, total * sizeof(size_t)); // sentinel

  size_t count = 0;
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      bool keep = true;
      size_t ti = i, tj = j;
      for (size_t k = 0; k < depth; k++) {
        if ((ti % 3 == 1) && (tj % 3 == 1)) {
          keep = false;
          break;
        }
        ti /= 3;
        tj /= 3;
      }
      if (keep) {
        gvizSetBit(in_carpet, i * dim + j);
        node_id[i * dim + j] = count++;
      }
    }
  }

  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, count);
  for (size_t i = 0; i < count; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  // connect grid neighbors that are both in the carpet
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      if (!gvizTestBit(in_carpet, i * dim + j))
        continue;
      size_t idx = node_id[i * dim + j];
      // right
      if (j + 1 < dim && gvizTestBit(in_carpet, i * dim + j + 1))
        gvizGraphAddEdge(&g, idx, node_id[i * dim + (j + 1)]);
      // down
      if (i + 1 < dim && gvizTestBit(in_carpet, (i + 1) * dim + j))
        gvizGraphAddEdge(&g, idx, node_id[(i + 1) * dim + j]);
      // diag right (triangulate)
      if (i + 1 < dim && j + 1 < dim &&
          gvizTestBit(in_carpet, (i + 1) * dim + (j + 1)))
        gvizGraphAddEdge(&g, idx, node_id[(i + 1) * dim + (j + 1)]);
      // diag left
      if (i + 1 < dim && j > 0 &&
          gvizTestBit(in_carpet, (i + 1) * dim + (j - 1)))
        gvizGraphAddEdge(&g, idx, node_id[(i + 1) * dim + (j - 1)]);
    }
  }

  GVIZ_DEALLOC(in_carpet);
  GVIZ_DEALLOC(node_id);

  printf("created sierpinski carpet with depth %zu (%zu vertices).\n", depth,
         g.vertices.count);

  return g;
}

gvizGraph build_rect_mesh(size_t L, size_t W) {
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, L * W);

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++)
      gvizGraphAddVertex(&g, NULL, NULL, NULL);

  size_t idx, idx_right, idx_down;

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++) {
      idx = i * W + j;

      // right neighbor
      if (j + 1 < W) {
        idx_right = i * W + (j + 1);
        gvizGraphAddEdge(&g, idx, idx_right);
      }

      // down neighbor
      if (i + 1 < L) {
        idx_down = (i + 1) * W + j;
        gvizGraphAddEdge(&g, idx, idx_down);
      }
    }

  return g;
}

typedef struct {
  size_t a, b, c, d; // a + b + c + d = depth
} Bary4;

// Total number of barycentric integer points for given depth
// = C(depth + 3, 3)
static inline size_t tetra_num_vertices(size_t depth) {
  return (depth + 3) * (depth + 2) * (depth + 1) / 6;
}

// Map (a,b,c,d) -> linear index in [0, numVerts)
//
// Enumeration order:
//   for (a = 0..depth)
//     for (b = 0..depth-a)
//       for (c = 0..depth-a-b)
//         d = depth - a - b - c;
//         emit (a,b,c,d);
size_t bary4_to_index(size_t depth, size_t a, size_t b, size_t c, size_t d) {
  // Ensure it's a valid barycentric tuple
  assert(a + b + c + d == depth);

  size_t idx = 0;

  // Count all points with a' < a
  for (size_t aa = 0; aa < a; aa++) {
    size_t remaining1 = depth - aa; // b + c + d = remaining1
    // #solutions to b + c + d = remaining1 is C(remaining1 + 2, 2)
    size_t r = remaining1;
    idx += (r + 2) * (r + 1) / 2; // C(r+2,2)
  }

  // For this a, count all with b' < b
  size_t remaining2 = depth - a;
  for (size_t bb = 0; bb < b; bb++) {
    size_t rem_bc = remaining2 - bb; // c + d = rem_bc
    // #solutions to c + d = rem_bc is rem_bc + 1
    idx += rem_bc + 1;
  }

  // For this (a,b), c is just an offset inside that block
  idx += c;

  return idx;
}

// Inverse mapping: index -> (a,b,c,d) for given depth
Bary4 index_to_bary4(size_t depth, size_t idx) {
  size_t numVerts = tetra_num_vertices(depth);
  assert(idx < numVerts);

  Bary4 out = {0, 0, 0, 0};

  // Find a such that idx falls in that a-slab
  for (size_t a = 0; a <= depth; a++) {
    size_t remaining1 = depth - a; // b + c + d = remaining1
    size_t blockA = (remaining1 + 2) * (remaining1 + 1) / 2; // C(r+2,2)

    if (idx < blockA) {
      out.a = a;
      break;
    } else {
      idx -= blockA;
    }
  }

  size_t remaining2 = depth - out.a;

  // Find b for this a
  for (size_t b = 0; b <= remaining2; b++) {
    size_t rem_bc = remaining2 - b; // c + d = rem_bc
    size_t blockB = rem_bc + 1;     // #solutions to c + d = rem_bc

    if (idx < blockB) {
      out.b = b;
      break;
    } else {
      idx -= blockB;
    }
  }

  size_t rem_cd = remaining2 - out.b;

  // c is exactly the remaining idx inside this (a,b) block
  out.c = idx;
  // d is forced by the sum constraint
  out.d = rem_cd - out.c;

  assert(out.a + out.b + out.c + out.d == depth);
  return out;
}

// -------------------------
// Tetrahedral mesh builder
// -------------------------

// Graph of the 1-skeleton of a subdivided tetrahedron:
// - Vertices: barycentric integer points (a,b,c,d), a+b+c+d = depth
// - Edges: (+1,-1,0,0) moves (and permutations), constrained to nonnegative
// coords
gvizGraph build_tetrahedral_mesh(size_t depth) {
  size_t numVerts = tetra_num_vertices(depth);

  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, numVerts);

  for (size_t i = 0; i < numVerts; i++) {
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  }

  for (size_t idx = 0; idx < numVerts; idx++) {
    Bary4 v = index_to_bary4(depth, idx);

// Try a single (+1,-1,0,0) pattern
#define TRY_MOVE(da, db, dc, dd)                                               \
  do {                                                                         \
    long na = (long)v.a + (da);                                                \
    long nb = (long)v.b + (db);                                                \
    long nc = (long)v.c + (dc);                                                \
    long nd = (long)v.d + (dd);                                                \
    if (na >= 0 && nb >= 0 && nc >= 0 && nd >= 0 &&                            \
        na + nb + nc + nd == (long)depth) {                                    \
      size_t to = bary4_to_index(depth, (size_t)na, (size_t)nb, (size_t)nc,    \
                                 (size_t)nd);                                  \
      gvizGraphAddEdge(&g, idx, to);                                           \
    }                                                                          \
  } while (0)

    // 6 distinct oriented moves (+1/-1 between coordinates)
    TRY_MOVE(+1, -1, 0, 0);
    TRY_MOVE(+1, 0, -1, 0);
    TRY_MOVE(+1, 0, 0, -1);
    TRY_MOVE(0, +1, -1, 0);
    TRY_MOVE(0, +1, 0, -1);
    TRY_MOVE(0, 0, +1, -1);

#undef TRY_MOVE
  }

  printf("Created tetrahedral mesh with depth %zu (%zu vertices).\n", depth,
         numVerts);

  return g;
}

gvizGraph build_equilateral_tri_mesh(size_t depth) {
  // depth = number of rows, total nodes = (depth+1)*(depth+2)/2
  size_t n = (depth + 1) * (depth + 2) / 2;
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, n);
  for (size_t i = 0; i < n; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

// node (i, j) where i = row (0..depth), j = col (0..i)
// maps to index: i*(i+1)/2 + j
#define IDX(i, j) ((i) * ((i) + 1) / 2 + (j))

  for (size_t i = 0; i <= depth; i++) {
    for (size_t j = 0; j <= i; j++) {
      if (j + 1 <= i)
        gvizGraphAddEdge(&g, IDX(i, j), IDX(i, j + 1)); // right on same row
      if (i + 1 <= depth) {
        gvizGraphAddEdge(&g, IDX(i, j), IDX(i + 1, j));     // down-left
        gvizGraphAddEdge(&g, IDX(i, j), IDX(i + 1, j + 1)); // down-right
      }
    }
  }

#undef IDX

  printf("Created trimesh with depth %zu (%zu vertices).\n", depth,
         g.vertices.count);

  return g;
}

int isConnected(gvizGraph *g) {
  gvizGraph bfs;
  gvizGraphBFSTree(g, &bfs, 0, 0, 0);
  int res = bfs.vertices.count == g->vertices.count;
  gvizGraphRelease(&bfs);
  return res;
}

gvizGraph build_knotted_rect_mesh(size_t L, size_t W) {
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, L * W);

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++)
      gvizGraphAddVertex(&g, NULL, NULL, NULL);

  size_t idx, idx_right, idx_down;

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++) {
      idx = i * W + j;

      // right neighbor
      if (j + 1 < W) {
        idx_right = i * W + (j + 1);
        gvizGraphAddEdge(&g, idx, idx_right);
      }

      // down neighbor
      if (i + 1 < L) {
        idx_down = (i + 1) * W + j;
        gvizGraphAddEdge(&g, idx, idx_down);
      }
    }

  // knot the corners together
  gvizGraphAddEdge(&g, 0, g.vertices.count - 1);
  gvizGraphAddEdge(&g, 0, W - 1);
  gvizGraphAddEdge(&g, g.vertices.count - 1, W - 1);
  gvizGraphAddEdge(&g, 0, g.vertices.count - 1);
  gvizGraphAddEdge(&g, W - 1, g.vertices.count - W);
  gvizGraphAddEdge(&g, g.vertices.count - 1, g.vertices.count - W);
  gvizGraphAddEdge(&g, 0, g.vertices.count - W);

  return g;
}

// A Mobius strip graph is a grid graph with a twist:
// one end is glued to the other with a flip.
// rows = number of vertices along the length of the strip
// cols = number of vertices across the width
gvizGraph build_mobius_strip(size_t rows, size_t cols) {
  size_t n = rows * cols;
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, n);
  for (size_t i = 0; i < n; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  // idx(i, j) = i * cols + j
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      size_t curr = i * cols + j;

      // right neighbor (along width)
      if (j + 1 < cols)
        gvizGraphAddEdge(&g, curr, i * cols + (j + 1));

      // down neighbor (along length)
      if (i + 1 < rows) {
        gvizGraphAddEdge(&g, curr, (i + 1) * cols + j);
      } else {
        // last row glues to first row with a flip:
        // column j connects to column (cols - 1 - j)
        size_t flipped = (cols - 1 - j);
        gvizGraphAddEdge(&g, curr, 0 * cols + flipped);
      }
    }
  }
  return g;
}

gvizGraph build_klein_bottle(size_t rows, size_t cols) {
  size_t n = rows * cols;
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, n);
  for (size_t i = 0; i < n; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      size_t curr = i * cols + j;

      // right neighbor — last col glues to first col, no flip (cylinder)
      if (j + 1 < cols)
        gvizGraphAddEdge(&g, curr, i * cols + (j + 1));
      else
        gvizGraphAddEdge(&g, curr, i * cols + 0);

      // down neighbor — last row glues to first row, with flip
      if (i + 1 < rows)
        gvizGraphAddEdge(&g, curr, (i + 1) * cols + j);
      else
        gvizGraphAddEdge(&g, curr, 0 * cols + (cols - 1 - j));
    }
  }
  return g;
}
