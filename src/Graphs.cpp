#include "Graphs.hpp"

#include "BitSet.hpp"
#include "ConnectedComponents.hpp"
#include "Subgraph.hpp"

#include <cassert>
#include <cstdlib>
#include <limits>
#include <vector>

namespace gviz::graphs {

namespace {

void SierpinskiRecurse(Graph &g, int depth, size_t t, size_t l, size_t r) {
  if (depth == 0) {
    // Base case: just wire the three corners into a triangle.
    g.AddEdge(t, l, 1.0);
    g.AddEdge(l, r, 1.0);
    g.AddEdge(r, t, 1.0);
    return;
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
   * double-added (each sub-triangle only draws its own three edges at
   * depth 0).
   */
  size_t mt = g.AddVertex();
  size_t tr = g.AddVertex();
  size_t ml = g.AddVertex();

  SierpinskiRecurse(g, depth - 1, t, mt, tr);
  SierpinskiRecurse(g, depth - 1, mt, l, ml);
  SierpinskiRecurse(g, depth - 1, tr, ml, r);
}

void SierpinskiTetrahedronRecurse(Graph &g, int depth, size_t a, size_t b, size_t c, size_t d) {
  if (depth == 0) {
    // Base case: wire four corners into a complete graph K4 (tetrahedron).
    g.AddEdge(a, b, 1.0);
    g.AddEdge(a, c, 1.0);
    g.AddEdge(a, d, 1.0);
    g.AddEdge(b, c, 1.0);
    g.AddEdge(b, d, 1.0);
    g.AddEdge(c, d, 1.0);
    return;
  }

  /*
   * A tetrahedron has 4 corners and 6 edges. We introduce one midpoint per
   * edge -- 6 new vertices:
   *
   *   ab = midpoint of (a, b)     bc = midpoint of (b, c)
   *   ac = midpoint of (a, c)     bd = midpoint of (b, d)
   *   ad = midpoint of (a, d)     cd = midpoint of (c, d)
   *
   * This splits the tetrahedron into 4 sub-tetrahedra at the corners and 1
   * octahedron in the middle. The Sierpinski tetrahedron discards the
   * central octahedron and recurses into the 4 corner sub-tetrahedra:
   *
   *   corner a: (a,  ab, ac, ad)      corner c: (ac, bc, c,  cd)
   *   corner b: (ab, b,  bc, bd)      corner d: (ad, bd, cd, d )
   *
   * Each midpoint vertex is shared by exactly 2 sub-tetrahedra, so edges on
   * shared faces won't be double-added.
   */
  size_t ab = g.AddVertex();
  size_t ac = g.AddVertex();
  size_t ad = g.AddVertex();
  size_t bc = g.AddVertex();
  size_t bd = g.AddVertex();
  size_t cd = g.AddVertex();

  SierpinskiTetrahedronRecurse(g, depth - 1, a, ab, ac, ad);
  SierpinskiTetrahedronRecurse(g, depth - 1, ab, b, bc, bd);
  SierpinskiTetrahedronRecurse(g, depth - 1, ac, bc, c, cd);
  SierpinskiTetrahedronRecurse(g, depth - 1, ad, bd, cd, d);
}

// -------------------------
// Tetrahedral mesh builder
// -------------------------
//
// Graph of the 1-skeleton of a subdivided tetrahedron:
//  - Vertices: barycentric integer points (a,b,c,d), a+b+c+d = depth
//  - Edges: (+1,-1,0,0) moves (and permutations), constrained to
//    nonnegative coords.
//
// These helpers are pure math (no gviz types) and are private to this
// translation unit -- unlike the old C globals bary4_to_index/
// index_to_bary4/tetra_num_vertices, which leaked file-external linkage to
// no actual external caller (grep confirms nothing outside graphs.c ever
// referenced them).

struct Bary4 {
  size_t a, b, c, d; // a + b + c + d = depth
};

// Total number of barycentric integer points for given depth = C(depth+3, 3).
size_t TetraNumVertices(size_t depth) {
  return (depth + 3) * (depth + 2) * (depth + 1) / 6;
}

// Map (a,b,c,d) -> linear index in [0, numVerts).
//
// Enumeration order:
//   for (a = 0..depth)
//     for (b = 0..depth-a)
//       for (c = 0..depth-a-b)
//         d = depth - a - b - c;
//         emit (a,b,c,d);
size_t Bary4ToIndex(size_t depth, size_t a, size_t b, size_t c, size_t d) {
  assert(a + b + c + d == depth);

  size_t idx = 0;

  // Count all points with a' < a.
  for (size_t aa = 0; aa < a; aa++) {
    size_t remaining1 = depth - aa; // b + c + d = remaining1
    size_t r = remaining1;
    idx += (r + 2) * (r + 1) / 2; // C(r+2,2)
  }

  // For this a, count all with b' < b.
  size_t remaining2 = depth - a;
  for (size_t bb = 0; bb < b; bb++) {
    size_t remBc = remaining2 - bb; // c + d = remBc
    idx += remBc + 1;               // #solutions to c + d = remBc
  }

  // For this (a,b), c is just an offset inside that block.
  idx += c;
  return idx;
}

// Inverse mapping: index -> (a,b,c,d) for given depth.
Bary4 IndexToBary4(size_t depth, size_t idx) {
  assert(idx < TetraNumVertices(depth));

  Bary4 out{0, 0, 0, 0};

  // Find a such that idx falls in that a-slab.
  for (size_t a = 0; a <= depth; a++) {
    size_t remaining1 = depth - a; // b + c + d = remaining1
    size_t blockA = (remaining1 + 2) * (remaining1 + 1) / 2; // C(r+2,2)
    if (idx < blockA) {
      out.a = a;
      break;
    }
    idx -= blockA;
  }

  size_t remaining2 = depth - out.a;

  // Find b for this a.
  for (size_t b = 0; b <= remaining2; b++) {
    size_t remBc = remaining2 - b; // c + d = remBc
    size_t blockB = remBc + 1;     // #solutions to c + d = remBc
    if (idx < blockB) {
      out.b = b;
      break;
    }
    idx -= blockB;
  }

  size_t remCd = remaining2 - out.b;
  out.c = idx;          // c is exactly the remaining idx inside this (a,b) block
  out.d = remCd - out.c; // d is forced by the sum constraint

  assert(out.a + out.b + out.c + out.d == depth);
  return out;
}

} // namespace

Graph CreateSierpinski(int depth, SierpinskiTriangle *outCorners) {
  // Pre-compute vertex count so we can reserve at the right capacity.
  // V(0)=3, V(n) = 3*V(n-1) - 3  =>  V(n) = (3^(n+1) + 3) / 2
  size_t capacity = 3;
  for (int i = 0; i < depth; i++)
    capacity = 3 * capacity - 3;

  Graph g(/*directed=*/false, capacity);

  // Add the three fixed outer corners first.
  size_t t = g.AddVertex();
  size_t l = g.AddVertex();
  size_t r = g.AddVertex();

  SierpinskiRecurse(g, depth, t, l, r);

  if (outCorners)
    *outCorners = SierpinskiTriangle{t, l, r};

  return g;
}

Graph CreateSierpinskiTetrahedron(int depth, SierpinskiTetrahedron *outCorners) {
  size_t capacity = 4;
  for (int i = 0; i < depth; i++)
    capacity = 4 * capacity - 6;

  Graph g(/*directed=*/false, capacity);

  size_t a = g.AddVertex();
  size_t b = g.AddVertex();
  size_t c = g.AddVertex();
  size_t d = g.AddVertex();

  SierpinskiTetrahedronRecurse(g, depth, a, b, c, d);

  if (outCorners)
    *outCorners = SierpinskiTetrahedron{a, b, c, d};

  return g;
}

Graph BuildSierpinskiCarpet(size_t depth) {
  size_t dim = 1;
  for (size_t k = 0; k < depth; k++)
    dim *= 3; // dim = 3^depth

  // First pass: determine which grid points are in the carpet.
  size_t total = dim * dim;
  BitSet inCarpet(total);
  std::vector<size_t> nodeId(total, std::numeric_limits<size_t>::max()); // sentinel

  size_t count = 0;
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      bool keep = true;
      size_t ti = i, tj = j;
      for (size_t k = 0; k < depth; k++) {
        if (ti % 3 == 1 && tj % 3 == 1) {
          keep = false;
          break;
        }
        ti /= 3;
        tj /= 3;
      }
      if (keep) {
        inCarpet.Set(i * dim + j);
        nodeId[i * dim + j] = count++;
      }
    }
  }

  Graph g(/*directed=*/false, count);
  for (size_t i = 0; i < count; i++)
    g.AddVertex();

  // Connect grid neighbors that are both in the carpet.
  for (size_t i = 0; i < dim; i++) {
    for (size_t j = 0; j < dim; j++) {
      if (!inCarpet.Test(i * dim + j))
        continue;
      size_t idx = nodeId[i * dim + j];
      // right
      if (j + 1 < dim && inCarpet.Test(i * dim + j + 1))
        g.AddEdge(idx, nodeId[i * dim + (j + 1)], 1.0);
      // down
      if (i + 1 < dim && inCarpet.Test((i + 1) * dim + j))
        g.AddEdge(idx, nodeId[(i + 1) * dim + j], 1.0);
      // diag right (triangulate)
      if (i + 1 < dim && j + 1 < dim && inCarpet.Test((i + 1) * dim + (j + 1)))
        g.AddEdge(idx, nodeId[(i + 1) * dim + (j + 1)], 1.0);
      // diag left
      if (i + 1 < dim && j > 0 && inCarpet.Test((i + 1) * dim + (j - 1)))
        g.AddEdge(idx, nodeId[(i + 1) * dim + (j - 1)], 1.0);
    }
  }

  return g;
}

Graph BuildRectMesh(size_t length, size_t width) {
  Graph g(/*directed=*/false, length * width);
  for (size_t i = 0; i < length; i++)
    for (size_t j = 0; j < width; j++)
      g.AddVertex();

  for (size_t i = 0; i < length; i++) {
    for (size_t j = 0; j < width; j++) {
      size_t idx = i * width + j;

      // right neighbor
      if (j + 1 < width)
        g.AddEdge(idx, i * width + (j + 1), 1.0);

      // down neighbor
      if (i + 1 < length)
        g.AddEdge(idx, (i + 1) * width + j, 1.0);
    }
  }

  return g;
}

Graph BuildTetrahedralMesh(size_t depth) {
  size_t numVerts = TetraNumVertices(depth);

  Graph g(/*directed=*/false, numVerts);
  for (size_t i = 0; i < numVerts; i++)
    g.AddVertex();

  for (size_t idx = 0; idx < numVerts; idx++) {
    Bary4 v = IndexToBary4(depth, idx);

    // Try a single (+1,-1,0,0) pattern (and its permutations below).
    auto tryMove = [&](long da, long db, long dc, long dd) {
      long na = static_cast<long>(v.a) + da;
      long nb = static_cast<long>(v.b) + db;
      long nc = static_cast<long>(v.c) + dc;
      long nd = static_cast<long>(v.d) + dd;
      if (na >= 0 && nb >= 0 && nc >= 0 && nd >= 0 &&
          na + nb + nc + nd == static_cast<long>(depth)) {
        size_t to = Bary4ToIndex(depth, static_cast<size_t>(na), static_cast<size_t>(nb),
                                  static_cast<size_t>(nc), static_cast<size_t>(nd));
        g.AddEdge(idx, to, 1.0);
      }
    };

    // 6 distinct oriented moves (+1/-1 between coordinates).
    tryMove(+1, -1, 0, 0);
    tryMove(+1, 0, -1, 0);
    tryMove(+1, 0, 0, -1);
    tryMove(0, +1, -1, 0);
    tryMove(0, +1, 0, -1);
    tryMove(0, 0, +1, -1);
  }

  return g;
}

Graph BuildEquilateralTriMesh(size_t depth) {
  // depth = number of rows, total nodes = (depth+1)*(depth+2)/2.
  size_t n = (depth + 1) * (depth + 2) / 2;
  Graph g(/*directed=*/false, n);
  for (size_t i = 0; i < n; i++)
    g.AddVertex();

  // node (i, j) where i = row (0..depth), j = col (0..i) maps to index
  // i*(i+1)/2 + j.
  auto idx = [](size_t i, size_t j) -> size_t { return i * (i + 1) / 2 + j; };

  for (size_t i = 0; i <= depth; i++) {
    for (size_t j = 0; j <= i; j++) {
      if (j + 1 <= i)
        g.AddEdge(idx(i, j), idx(i, j + 1), 1.0); // right on same row
      if (i + 1 <= depth) {
        g.AddEdge(idx(i, j), idx(i + 1, j), 1.0);     // down-left
        g.AddEdge(idx(i, j), idx(i + 1, j + 1), 1.0); // down-right
      }
    }
  }

  return g;
}

Graph BuildKnottedRectMesh(size_t length, size_t width) {
  Graph g = BuildRectMesh(length, width);

  // Knot the corners together.
  size_t last = g.Size() - 1;
  g.AddEdge(0, last, 1.0);
  g.AddEdge(0, width - 1, 1.0);
  g.AddEdge(last, width - 1, 1.0);
  g.AddEdge(0, last, 1.0);
  g.AddEdge(width - 1, g.Size() - width, 1.0);
  g.AddEdge(last, g.Size() - width, 1.0);
  g.AddEdge(0, g.Size() - width, 1.0);

  return g;
}

// A Mobius strip graph is a grid graph with a twist: one end is glued to
// the other with a flip.
//   rows = number of vertices along the length of the strip
//   cols = number of vertices across the width
Graph BuildMobiusStrip(size_t rows, size_t cols) {
  size_t n = rows * cols;
  Graph g(/*directed=*/false, n);
  for (size_t i = 0; i < n; i++)
    g.AddVertex();

  // idx(i, j) = i * cols + j
  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      size_t curr = i * cols + j;

      // right neighbor (along width)
      if (j + 1 < cols)
        g.AddEdge(curr, i * cols + (j + 1), 1.0);

      // down neighbor (along length)
      if (i + 1 < rows) {
        g.AddEdge(curr, (i + 1) * cols + j, 1.0);
      } else {
        // last row glues to first row with a flip: column j connects to
        // column (cols - 1 - j).
        size_t flipped = cols - 1 - j;
        g.AddEdge(curr, flipped, 1.0);
      }
    }
  }

  return g;
}

Graph BuildKleinBottle(size_t rows, size_t cols) {
  size_t n = rows * cols;
  Graph g(/*directed=*/false, n);
  for (size_t i = 0; i < n; i++)
    g.AddVertex();

  for (size_t i = 0; i < rows; i++) {
    for (size_t j = 0; j < cols; j++) {
      size_t curr = i * cols + j;

      // right neighbor -- last col glues to first col, no flip (cylinder).
      if (j + 1 < cols)
        g.AddEdge(curr, i * cols + (j + 1), 1.0);
      else
        g.AddEdge(curr, i * cols, 1.0);

      // down neighbor -- last row glues to first row, with flip.
      if (i + 1 < rows)
        g.AddEdge(curr, (i + 1) * cols + j, 1.0);
      else
        g.AddEdge(curr, cols - 1 - j, 1.0);
    }
  }

  return g;
}

Graph BuildRandomConnectedGraph(size_t numVertices, double edgeDensity, unsigned int seed,
                                 bool directed) {
  if (edgeDensity < 0.0)
    edgeDensity = 0.0;
  if (edgeDensity > 1.0)
    edgeDensity = 1.0;

  Graph g(directed, numVertices);
  for (size_t i = 0; i < numVertices; i++)
    g.AddVertex();

  if (numVertices < 2)
    return g;

  for (size_t i = 1; i < numVertices; i++) {
    size_t j = static_cast<size_t>(rand_r(&seed) % i);
    // Undirected: orientation is irrelevant. Directed: point parent (the
    // earlier, already-attached vertex) -> child (the new vertex) so the
    // whole spanning tree is a proper out-tree rooted at vertex 0, matching
    // gviz::search::IsTree's parent-scan (in-degree <= 1, one parentless
    // root).
    if (directed)
      g.AddEdge(j, i, 1.0);
    else
      g.AddEdge(i, j, 1.0);
  }

  size_t treeEdges = numVertices - 1;
  size_t maxEdges = numVertices * (numVertices - 1) / 2;
  size_t maxExtra = maxEdges - treeEdges;
  size_t targetExtra = static_cast<size_t>(edgeDensity * static_cast<double>(maxExtra) + 0.5);

  size_t added = 0;
  size_t attempts = 0;
  size_t maxAttempts = targetExtra * 20 + 100;
  while (added < targetExtra && attempts < maxAttempts) {
    attempts++;
    size_t a = static_cast<size_t>(rand_r(&seed) % numVertices);
    size_t b = static_cast<size_t>(rand_r(&seed) % numVertices);
    if (a == b || g.EdgeExists(a, b))
      continue;
    g.AddEdge(a, b, 1.0);
    added++;
  }

  return g;
}

bool IsConnected(const Graph &g) {
  Subgraph sg = Subgraph::CreateVertexInduced(g);
  for (size_t i = 0; i < g.Size(); i++)
    sg.ShowVertex(i);
  return search::ConnectedComponents(sg).count <= 1;
}

} // namespace gviz::graphs
