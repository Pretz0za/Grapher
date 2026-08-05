#include "Planar.hpp"
#include "SchnyderWood.hpp"

// The vendored Boyer-Myrvold planarity library stays plain C (off-limits to
// modify -- see this port's task instructions); graph.h itself already
// wraps its declarations in `extern "C" { ... }` when __cplusplus is
// defined, so including it directly here (no extra wrapping needed) gets us
// C linkage automatically.
#include "boyerMyrvold/appconst.h"
#include "boyerMyrvold/graph.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace gviz::layout {

namespace {

constexpr size_t kNoIndex = std::numeric_limits<size_t>::max();

// ---- rotation-system dart queries (shared implementation) -----------------

size_t PrevNeighborCCWImpl(const Graph &g, size_t u, size_t v) {
  size_t idx;
  [[maybe_unused]] bool found = g.NeighborPosition(u, v, idx);
  assert(found);
  size_t degree = g.Degree(u);
  return g.Neighbor(u, (idx == 0 ? degree : idx) - 1);
}

// ---- Boyer-Myrvold <-> Graph bridging --------------------------------------

// The CCW rotation Boyer-Myrvold installed on vertex v, read off its
// link[1] chain (arcs, i.e. entries >= N) and reversed -- mirrors the old
// gvizAdjacencyFromGP exactly (that function also reversed after collecting
// in link[1] order).
void AdjacencyFromBoyer(graphP theGraph, int v, std::vector<size_t> &out) {
  out.clear();
  if (!theGraph)
    return;
  int N = theGraph->N;
  if (v < 0 || v >= N)
    return;

  int J = theGraph->G[v].link[1];
  while (J >= N) {
    size_t curr = static_cast<size_t>(theGraph->G[J].v);
    if (curr < static_cast<size_t>(N))
      out.push_back(curr);
    J = theGraph->G[J].link[1];
  }

  std::reverse(out.begin(), out.end());
}

// Mirrors the old kuratowskiFromBoyer: walks each vertex's link[0] (DFS
// child/back-edge) arc ring and records every edge (u, v) with v < u.
Graph KuratowskiFromBoyer(graphP g) {
  Graph kg(false, static_cast<size_t>(g->N > 0 ? g->N : 0));
  for (int i = 0; i < g->N; i++)
    kg.AddVertex();

  for (int u = 0; u < g->N; u++) {
    int J = g->G[u].link[0];
    if (J < 2 * g->N)
      continue;
    int startArc = J;
    do {
      int v = g->G[J].v;
      if (v < u)
        kg.AddEdge(static_cast<size_t>(u), static_cast<size_t>(v), 1.0);
      J = g->G[J].link[0];
    } while (J != startArc);
  }

  return kg;
}

// Rewrites u's adjacency list to: Boyer-Myrvold's CCW order over u's
// subgraph neighbors, followed by any of u's remaining (out-of-subgraph)
// neighbors in their existing relative order. Mirrors mergeRotationIntoAdjacency.
void MergeRotationIntoAdjacency(Graph &g, const Subgraph &sg, graphP boyer, size_t u) {
  std::vector<size_t> order;
  AdjacencyFromBoyer(boyer, static_cast<int>(u), order);

  size_t nbDegree = g.Degree(u);
  for (size_t i = 0; i < nbDegree; i++) {
    size_t v = g.Neighbor(u, i);
    if (!sg.HasEdge(u, v) && std::find(order.begin(), order.end(), v) == order.end())
      order.push_back(v);
  }

  // Old C ignored ReorderNeighbors' return value too (order is built to be
  // exactly a permutation of u's current neighbors by construction: every
  // subgraph edge Boyer-Myrvold saw plus every remaining non-subgraph
  // neighbor, each exactly once).
  g.ReorderNeighbors(u, order);
}

bool CopyBoyerGraphIntoSubgraph(const Subgraph &sg, graphP boyer) {
  for (size_t u : sg) {
    for (size_t v : sg.Neighbors(u)) {
      if (v > u)
        continue;
      if (gp_AddEdge(boyer, static_cast<int>(u), 0, static_cast<int>(v), 0) != OK)
        return false;
    }
  }
  return true;
}

} // namespace

// ============================================================================
// ROTATION-SYSTEM DART QUERIES
// ============================================================================

size_t PrevNeighborCCW(const Graph &g, size_t u, size_t v) { return PrevNeighborCCWImpl(g, u, v); }

size_t NextNeighborCCW(const Graph &g, size_t u, size_t v) {
  size_t idx;
  [[maybe_unused]] bool found = g.NeighborPosition(u, v, idx);
  assert(found);
  size_t degree = g.Degree(u);
  return g.Neighbor(u, (idx + 1) % degree);
}

HalfEdge HalfEdgeNext(const Graph &g, HalfEdge e) {
  size_t w = PrevNeighborCCWImpl(g, e.v, e.u);
  return HalfEdge{e.v, w};
}

// ============================================================================
// FACE WALKING
// ============================================================================

FaceWalk::FaceWalk(const Graph &g, const Subgraph &sg, HalfEdge start) : g_(g), start_(start) {
  if (!sg.HasEdge(start.u, start.v))
    throw LayoutError("gviz::layout::FaceWalk: start dart is not a subgraph edge");
}

FaceWalk::iterator FaceWalk::begin() const {
  iterator it;
  it.g_ = &g_;
  it.start_ = start_;
  it.current_ = start_;
  it.done_ = false;
  return it;
}

void FaceWalk::iterator::Advance() {
  current_ = HalfEdgeNext(*g_, current_);
  if (current_ == start_)
    done_ = true;
}

// ============================================================================
// FACE ENUMERATION AND TRIANGULATION
// ============================================================================

namespace {

// Prefix-sum offsets into a flat dart index space, one slot per (vertex,
// raw-adjacency-index) pair for every vertex IN the subgraph -- mirrors
// buildDartBorders. Note this counts a subgraph vertex's FULL raw degree,
// including darts to neighbors outside the subgraph (those darts are simply
// never visited/marked by the enumeration loop below); this matches the old
// C's behavior exactly rather than tightening it, since Planar's usual
// operating mode (a full subgraph over the whole graph) makes the
// distinction moot and this port isn't the place to change enumeration
// semantics for the one test that exercises a non-full subgraph.
std::vector<size_t> BuildDartBorders(const Graph &g, const Subgraph &sg, size_t &outDartCount) {
  size_t N = g.Size();
  std::vector<size_t> borders(N, 0);
  outDartCount = 0;
  for (size_t u = 0; u < N; u++) {
    borders[u] = outDartCount;
    if (sg.HasVertex(u))
      outDartCount += g.Degree(u);
  }
  return borders;
}

// Traces the face reached by repeatedly taking "the dart just before the
// reverse dart" (PrevNeighborCCW), pushing each dart's head vertex into
// @p face, until the starting dart (u, adjIdx) is revisited. Mirrors
// gvizPlanarTraceFace. Kept file-private (not part of Planar.hpp's public
// surface): its only caller in the old C was face enumeration itself, and
// its `visited`/`borders` state is a private detail of one enumeration pass,
// not something a caller could usefully drive standalone.
void TraceFace(const Graph &g, const std::vector<size_t> &borders, BitSet &visited, size_t u,
                size_t adjIdx, std::vector<size_t> &face) {
  struct Dart {
    size_t u;
    size_t idx;
  };

  Dart d{u, adjIdx};

  while (!visited.Test(borders[d.u] + d.idx)) {
    visited.Set(borders[d.u] + d.idx);

    size_t v = g.Neighbor(d.u, d.idx);
    face.push_back(v);

    size_t prevPos;
    [[maybe_unused]] bool found = g.NeighborPosition(v, d.u, prevPos);
    assert(found);
    size_t vDegree = g.Degree(v);
    d = Dart{v, (prevPos == 0 ? vDegree : prevPos) - 1};
  }
}

} // namespace

FaceEnumerator::FaceEnumerator(const Graph &g, const Subgraph &sg) {
  size_t dartCount;
  std::vector<size_t> borders = BuildDartBorders(g, sg, dartCount);
  BitSet visited(dartCount);

  for (size_t u : sg) {
    size_t degree = g.Degree(u);
    for (size_t i = 0; i < degree; i++) {
      size_t v = g.Neighbor(u, i);
      if (!sg.HasEdge(u, v))
        continue;
      if (visited.Test(borders[u] + i))
        continue;

      std::vector<size_t> face;
      TraceFace(g, borders, visited, u, i, face);

      if (face.size() >= 3)
        faces_.push_back(std::move(face));
    }
  }

  dartCount_ = dartCount;
}

void Triangulate(Graph &g, Subgraph &sg, FaceEnumerator &faces) {
  std::vector<std::vector<size_t>> &faceList = faces.Faces();

  // Every access below goes through faceList[i] by INDEX, re-read fresh
  // each time, rather than caching a pointer/reference to a face across the
  // `faceList.push_back()` a few lines down. The old C
  // (gvizPlanarEmbedderTriangulate) cached `gvizArray *face =
  // gvizArrayAtIndex(&context->faces, i)` once per outer loop iteration and
  // kept dereferencing it across `goto t;` even after
  // `gvizArrayPush(&context->faces, &newFace)` -- a push that can reallocate
  // context->faces' backing storage, leaving `face` dangling. Re-reading by
  // index every time sidesteps that class of bug entirely.
  for (size_t i = 0; i < faceList.size(); i++) {
    while (faceList[i].size() != 3) {
      bool split = false;

      for (size_t x = 0; x < 4 && !split; x++) {
        for (size_t y = x + 1; y < 4; y++) {
          size_t u = faceList[i][x];
          size_t v = faceList[i][y];

          if (u == v || g.EdgeExists(u, v))
            continue;

          size_t faceSize = faceList[i].size();
          size_t faceV1 = faceList[i][(x + 1) % faceSize];
          size_t idx1Pos;
          [[maybe_unused]] bool f1 = g.NeighborPosition(u, faceV1, idx1Pos);
          assert(f1);
          size_t idx1 = (idx1Pos + 1) % g.Degree(u);

          size_t faceV2 = faceList[i][(y - 1) % faceSize];
          size_t idx2Pos;
          [[maybe_unused]] bool f2 = g.NeighborPosition(v, faceV2, idx2Pos);
          assert(f2);
          size_t idx2 = idx2Pos;

          // Collect the vertices strictly between x and y (the "hidden"
          // boundary run this split cuts off into its own smaller face)
          // BEFORE erasing them -- reading faceList[i][z] in a plain
          // ascending loop, unlike the old C's delete-while-reading-at-z
          // loop, which (for y - x >= 3, i.e. more than one vertex between
          // them) skipped a vertex because each gvizArrayDeleteAtIndex
          // shifted later elements down while z kept incrementing against
          // the ORIGINAL span. See Planar.hpp's Triangulate doc for the
          // user-visible consequence.
          std::vector<size_t> newFace;
          newFace.push_back(u);
          for (size_t z = x + 1; z < y; z++)
            newFace.push_back(faceList[i][z]);
          faceList[i].erase(faceList[i].begin() + static_cast<ptrdiff_t>(x + 1),
                             faceList[i].begin() + static_cast<ptrdiff_t>(y));
          newFace.push_back(v);

          faceList.push_back(std::move(newFace)); // may reallocate faceList

          g.InsertNeighborAt(u, v, 1.0, idx1);
          g.InsertNeighborAt(v, u, 1.0, idx2);

          faces.AddDarts(2);

          split = true;
          break;
        }
      }

      if (!split)
        break; // no eligible chord among this face's first four vertices;
                // leave it as-is, matching the old code's silent fall-through.
    }
  }

  // The insertions above shifted adjacency indices, so the old edge bits no
  // longer line up with any layout. Triangulation operates on a full
  // subgraph (faces come from the full rotation system), so re-derive it as
  // full over the augmented graph instead of migrating stale bit positions.
  g.BuildLayout();
  sg.MakeFull();
}

// ============================================================================
// PLANAR ROTATION APPLICATION
// ============================================================================

void ApplyPlanarRotation(Graph &g, Subgraph &subgraph, bool captureWitness) {
  size_t N = g.Size();

  graphP boyer = gp_New();
  if (!boyer)
    throw std::bad_alloc();

  if (gp_InitGraph(boyer, static_cast<int>(N)) != OK) {
    gp_Free(&boyer);
    throw LayoutError("gviz::layout::ApplyPlanarRotation: failed to initialize "
                       "Boyer-Myrvold working graph");
  }

  if (!CopyBoyerGraphIntoSubgraph(subgraph, boyer)) {
    gp_Free(&boyer);
    throw LayoutError(
        "gviz::layout::ApplyPlanarRotation: failed to copy subgraph edges into "
        "Boyer-Myrvold working graph");
  }

  int res = gp_Embed(boyer, 0 /* no special embed flags, matches the old call */);
  if (res == NONPLANAR) {
    if (captureWitness) {
      Graph witness = KuratowskiFromBoyer(boyer);
      gp_Free(&boyer);
      throw PlanarNotPlanarError(std::move(witness));
    }
    gp_Free(&boyer);
    throw NotPlanarError();
  }

  if (res != OK) {
    gp_Free(&boyer);
    throw LayoutError("gviz::layout::ApplyPlanarRotation: Boyer-Myrvold embedding failed");
  }

  if (gp_SortVertices(boyer) != OK) {
    gp_Free(&boyer);
    throw LayoutError("gviz::layout::ApplyPlanarRotation: Boyer-Myrvold vertex sort failed");
  }

  for (size_t u : subgraph)
    MergeRotationIntoAdjacency(g, subgraph, boyer, u);

  gp_Free(&boyer);

  g.BuildLayout();
  subgraph.Rebuild();
}

// ============================================================================
// THE PLANAR EMBEDDER
// ============================================================================

Subgraph Planar::MakeRotated(Graph &g, Subgraph &&subgraph) {
  ApplyPlanarRotation(g, subgraph); // mutates subgraph in place; throws on failure
  return std::move(subgraph);
}

Planar::Planar(Graph &g, Subgraph subgraph)
    : EmbeddedGraph(MakeRotated(g, std::move(subgraph)), 2), graph_(g) {
  // Reached only on success (MakeRotated/ApplyPlanarRotation throws before
  // the base EmbeddedGraph -- and therefore this constructor body -- ever
  // runs on failure), so this is unconditionally correct here. This ordering
  // (rotate first, construct the base once already-rotated) is also why this
  // port never needs the old C's "partially-initialized embedding" window:
  // gvizPlanarEmbedderInit constructed the embedding FIRST and mutated its
  // subgraph in place afterward, which worked there only because C has no
  // exceptions to unwind through.
  SetPlanarEmbedded(true);
}

void Planar::Embed() {
  FaceEnumerator faces(graph_, Structure());
  Triangulate(graph_, Structure(), faces);

  SchnyderWood sw(graph_);
  sw.Embed(*this);
}

// ============================================================================
// PLANAR QUERIES ON ANY EMBEDDED GRAPH
// ============================================================================

Subgraph FaceSubgraph(const Graph &g, const std::vector<size_t> &face) {
  g.EnsureLayout(); // build-if-absent, not force-rebuild (matches faceToSubgraph)

  Subgraph out = Subgraph::CreateEmpty(g);
  for (size_t u : face)
    out.ShowVertex(u);

  size_t n = face.size();
  for (size_t i = 0; i < n; i++)
    out.ShowEdge(face[i], face[(i + 1) % n]);

  return out;
}

namespace {

bool PointInPolygon(const EmbeddedGraph &embedding, const std::vector<size_t> &face, double x,
                     double y) {
  size_t n = face.size();
  if (n < 3)
    return false;

  bool inside = false;
  for (size_t i = 0, j = n - 1; i < n; j = i++) {
    const double *pi = embedding.GetVPosition(face[i]);
    const double *pj = embedding.GetVPosition(face[j]);

    bool intersect = ((pi[1] > y) != (pj[1] > y)) &&
                      (x < (pj[0] - pi[0]) * (y - pi[1]) / (pj[1] - pi[1] + 0.0) + pi[0]);
    if (intersect)
      inside = !inside;
  }
  return inside;
}

double PolygonSignedArea(const EmbeddedGraph &embedding, const std::vector<size_t> &face) {
  double area = 0.0;
  size_t n = face.size();
  for (size_t i = 0; i < n; i++) {
    const double *pu = embedding.GetVPosition(face[i]);
    const double *pv = embedding.GetVPosition(face[(i + 1) % n]);
    area += pu[0] * pv[1] - pv[0] * pu[1];
  }
  return area * 0.5;
}

} // namespace

std::optional<Subgraph> FaceSubgraphAt(const Graph &g, const EmbeddedGraph &embedding,
                                        double worldX, double worldY) {
  if (!embedding.IsPlanarEmbedded())
    return std::nullopt;

  FaceEnumerator faces(g, embedding.Structure());

  double minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
  for (size_t u : embedding.Structure()) {
    const double *p = embedding.GetVPosition(u);
    minX = std::min(minX, p[0]);
    minY = std::min(minY, p[1]);
    maxX = std::max(maxX, p[0]);
    maxY = std::max(maxY, p[1]);
  }

  bool pickLargest = (worldX < minX || worldX > maxX || worldY < minY || worldY > maxY);

  size_t bestIdx = kNoIndex;
  double bestMetric = pickLargest ? -1.0 : INFINITY;

  const auto &faceList = faces.Faces();
  for (size_t i = 0; i < faceList.size(); i++) {
    const auto &face = faceList[i];
    if (face.size() < 3)
      continue;
    if (!pickLargest && !PointInPolygon(embedding, face, worldX, worldY))
      continue;

    double area = std::fabs(PolygonSignedArea(embedding, face));
    if (pickLargest) {
      if (area > bestMetric) {
        bestMetric = area;
        bestIdx = i;
      }
    } else if (area < bestMetric) {
      bestMetric = area;
      bestIdx = i;
    }
  }

  if (bestIdx == kNoIndex)
    return std::nullopt;

  return FaceSubgraph(g, faceList[bestIdx]);
}

std::vector<size_t> LargestFaceBoundary(const Graph &g, const Subgraph &sg) {
  FaceEnumerator faces(g, sg);

  size_t bestIdx = kNoIndex;
  size_t best = 0;
  const auto &faceList = faces.Faces();
  for (size_t i = 0; i < faceList.size(); i++) {
    if (faceList[i].size() < 3)
      continue;
    if (faceList[i].size() > best) {
      best = faceList[i].size();
      bestIdx = i;
    }
  }

  if (bestIdx == kNoIndex)
    return {};

  return faceList[bestIdx];
}

} // namespace gviz::layout
