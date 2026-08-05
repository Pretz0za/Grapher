#include "SchnyderWood.hpp"

#include "BitSet.hpp"
#include "Error.hpp"

#include <cassert>
#include <optional>

namespace gviz::layout {

namespace {

// Third vertex of the triangular face traversed by dart u->v. Uses the same
// rotation as face enumeration: dart u->v -> next dart v->w where w is the
// neighbor just before u in v's CCW adjacency list. Mirrors swFaceThird.
size_t FaceThird(const Graph &g, size_t u, size_t v) {
  size_t idx;
  [[maybe_unused]] bool found = g.NeighborPosition(v, u, idx);
  assert(found);
  size_t degree = g.Degree(v);
  size_t prev = (idx == 0 ? degree : idx) - 1;
  return g.Neighbor(v, prev);
}

// Index of the neighbor immediately after u in v's adjacency list (wrapping
// to 0 past the end). Mirrors nextNeighborIdx, minus its debug printf()s
// (see SchnyderWood.hpp's IMPLEMENTATION STATUS note).
size_t NextNeighborIdx(const Graph &g, size_t v, size_t u) {
  size_t idx;
  [[maybe_unused]] bool found = g.NeighborPosition(v, u, idx);
  assert(found);
  size_t degree = g.Degree(v);
  size_t next = idx + 1;
  return next == degree ? 0 : next;
}

// Given a boundary cycle `face` (vertices in order) and the bitset of
// vertices already claimed by the current partition (`faceVertices`), finds
// a vertex strictly inside the face -- i.e. a neighbor, reached by stepping
// one dart past each boundary edge, that isn't itself already on the
// boundary/claimed. Mirrors findVertexInsideFace (minus its debug
// printf()s); std::optional replaces the old -1 sentinel.
std::optional<size_t> FindVertexInsideFace(const Graph &g, const std::vector<size_t> &face,
                                            const BitSet &faceVertices) {
  size_t n = face.size();
  for (size_t i = 0; i < n; i++) {
    size_t next = (i + 1) % n;
    size_t v = face[i];
    size_t u = face[next];
    size_t potentialInsideIdx = NextNeighborIdx(g, v, u);
    size_t potentialInside = g.Neighbor(v, potentialInsideIdx);
    if (!faceVertices.Test(potentialInside))
      return potentialInside;
  }
  return std::nullopt;
}

// Counts the vertices reachable from `v` without crossing `pathVertices`
// (a BFS bounded by that vertex set) -- i.e. the size of the region
// enclosed by the current partition on v's side. Mirrors verticesInRegion,
// using a plain index-cursor std::vector as the BFS queue instead of a
// gvizDeque (push-at-back/pop-at-front is all this ever needs) and dropping
// the old code's `invMap` scratch array, which was allocated and zeroed but
// never actually read from or written to anywhere in the original function
// -- dead code, not ported.
size_t VerticesInRegion(const Graph &g, size_t v, const BitSet &pathVertices) {
  std::vector<size_t> queue;
  size_t head = 0;

  BitSet seen(g.Size());
  seen.Set(v);
  size_t count = 1;
  queue.push_back(v);

  while (head < queue.size()) {
    size_t curr = queue[head++];
    size_t degree = g.Degree(curr);
    for (size_t i = 0; i < degree; i++) {
      size_t nb = g.Neighbor(curr, i);
      if (seen.Test(nb) || pathVertices.Test(nb))
        continue;
      seen.Set(nb);
      count++;
      queue.push_back(nb);
    }
  }

  return count;
}

// The cycle surrounding the region with the same color as path1: path2
// forward, then path1 reversed, then the partition vertex. Mirrors
// getRegionBoundary.
std::vector<size_t> GetRegionBoundary(size_t partitionV, const std::vector<size_t> &path1,
                                       const std::vector<size_t> &path2) {
  std::vector<size_t> out;
  out.reserve(path1.size() + path2.size() + 1);
  out.insert(out.end(), path2.begin(), path2.end());
  for (size_t i = path1.size(); i-- > 0;)
    out.push_back(path1[i]);
  out.push_back(partitionV);
  return out;
}

} // namespace

SchnyderWood::SchnyderWood(const Graph &g) : g_(g), n_(g.Size()) {
  size_t N = n_;

  if (N < 3)
    throw LayoutError("gviz::layout::SchnyderWood requires at least 3 vertices");

  for (auto &p : parent_)
    p.assign(N, kNone);

  // Identify the outer-face triangle (s0, s1, s2):
  //   s0 = vertex 0
  //   s1 = first neighbor of s0 in its adjacency list
  //   s2 = third vertex of the face traced by dart s0->s1
  // Dart s0->s1 traces one of the two faces incident to edge s0-s1; that
  // face becomes the "outer" triangle -- s2 is the root of T2, while s0 and
  // s1 are the roots of T0 and T1 respectively. The opposite face (traced
  // by dart s1->s0) contains the first interior vertex to be processed.
  if (g.Degree(0) == 0)
    throw LayoutError("gviz::layout::SchnyderWood: vertex 0 has no neighbors");

  size_t s0 = 0;
  size_t s1 = g.Neighbor(0, 0);
  size_t s2 = FaceThird(g, s0, s1);

  root_[0] = s0;
  root_[1] = s1;
  root_[2] = s2;

  // Trivial case: only the outer triangle, no interior vertices.
  if (N == 3) {
    parent_[0][s2] = s0;
    parent_[1][s2] = s1;
    return;
  }

  std::vector<size_t> bnext(N, kNone), bprev(N, kNone);
  std::vector<char> done(N, 0);

  // Boundary path starts as s0 <-> s1 (the base edge).
  bnext[s0] = s1;
  bprev[s1] = s0;
  done[s0] = 1;
  done[s1] = 1;

  // Process N-3 interior vertices (all vertices except s0, s1, s2) in
  // canonical order using a boundary-edge scan.
  //
  // At each step:
  //   1. Walk the boundary left-to-right and find a boundary edge (L, R)
  //      whose interior face vertex w = FaceThird(g, R, L) is unprocessed
  //      and is not s2.
  //   2. Extend the attachment arc leftward while bprev[Lp] is a neighbor
  //      of w, and rightward while bnext[Rp] is a neighbor of w, giving the
  //      full consecutive arc [Lp ... Rp] of w's boundary neighbors.
  //   3. Assign:
  //        parent[0][w] = Lp   (T0 edge: w->Lp, toward s0)
  //        parent[1][w] = Rp   (T1 edge: w->Rp, toward s1)
  //        parent[2][ci] = w   for every "hidden" boundary vertex ci
  //                             strictly between Lp and Rp (T2 edge: ci->w)
  //   4. Remove hidden vertices from the boundary; insert w between Lp/Rp.
  size_t remaining = N - 3;

  while (remaining > 0) {
    size_t w = kNone;
    size_t foundL = kNone;

    size_t L = s0;
    while (bnext[L] != kNone) {
      size_t R = bnext[L];
      size_t cand = FaceThird(g, R, L);
      if (cand != s2 && !done[cand]) {
        w = cand;
        foundL = L;
        break;
      }
      L = R;
    }

    // For a valid triangulated planar graph this must always succeed. The
    // old C asserted this (a no-op in a release build) and then defensively
    // `break`-ed out, leaving `sw` half-built and silently wrong; throwing
    // instead means a caller can never observe an invalid SchnyderWood.
    if (w == kNone)
      throw LayoutError(
          "gviz::layout::SchnyderWood: canonical-ordering scan found no candidate "
          "vertex (graph is not a validly triangulated planar rotation system)");

    size_t foundR = bnext[foundL];

    // Extend the attachment arc leftward.
    size_t Lp = foundL;
    while (bprev[Lp] != kNone && g.EdgeExists(w, bprev[Lp]))
      Lp = bprev[Lp];

    // Extend the attachment arc rightward.
    size_t Rp = foundR;
    while (bnext[Rp] != kNone && g.EdgeExists(w, bnext[Rp]))
      Rp = bnext[Rp];

    parent_[0][w] = Lp;
    parent_[1][w] = Rp;

    // "Hidden" boundary vertices strictly between Lp and Rp become T2
    // children of w; remove them from the boundary linked list.
    size_t c = bnext[Lp];
    while (c != Rp) {
      parent_[2][c] = w;
      size_t nc = bnext[c];
      bnext[c] = kNone;
      bprev[c] = kNone;
      c = nc;
    }

    // Insert w into the boundary between Lp and Rp.
    bnext[Lp] = w;
    bprev[w] = Lp;
    bnext[w] = Rp;
    bprev[Rp] = w;

    done[w] = 1;
    remaining--;
  }

  // Final step: add s2 (the T2 root). Any boundary vertex still between s0
  // and s1 that wasn't covered by an earlier step becomes a T2 child of s2.
  // s2's own tree parents are the two non-base outer-triangle edges.
  size_t cv = bnext[s0];
  while (cv != s1 && cv != kNone) {
    parent_[2][cv] = s2;
    cv = bnext[cv];
  }
  parent_[0][s2] = s0;
  parent_[1][s2] = s1;
}

void SchnyderWood::Embed(EmbeddedGraph &embedding) const {
  if (embedding.Dim() != 2)
    throw DimensionError("gviz::layout::SchnyderWood::Embed requires a 2D embedding");

  std::array<std::vector<size_t>, 3> paths;

  for (size_t i = 0; i < n_; i++) {
    bool isRoot = (i == root_[0] || i == root_[1] || i == root_[2]);
    if (isRoot)
      continue;

    for (auto &p : paths)
      p.clear();

    BitSet pathVertices(n_);
    pathVertices.Set(i);

    for (size_t r = 0; r < 3; r++) {
      size_t curr = parent_[r][i];
      while (true) {
        pathVertices.Set(curr);
        paths[r].push_back(curr);
        if (curr == root_[r])
          break;
        curr = parent_[r][curr];
      }
    }

    double coordinates[3];
    for (size_t r = 0; r < 3; r++) {
      // TODO(carried over from the old C, unresolved -- see this class'
      // IMPLEMENTATION STATUS doc): finding the previous neighbor here is
      // not sufficient in general; this region-counting step is the known-
      // incomplete part of the algorithm.
      std::vector<size_t> boundary = GetRegionBoundary(i, paths[r], paths[(r + 1) % 3]);
      std::optional<size_t> v = FindVertexInsideFace(g_, boundary, pathVertices);

      size_t count = 0;
      if (v && !pathVertices.Test(*v))
        count = VerticesInRegion(g_, *v, pathVertices);
      count += paths[r].size();
      coordinates[r] = static_cast<double>(count);
    }

    // Only the first two of the three barycentric-style coordinates are
    // copied in, since the embedding is two-dimensional.
    embedding.SetVPosition(i, coordinates);
  }
}

} // namespace gviz::layout
