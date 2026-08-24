#include "Planar.hpp"
#include "SchnyderWood.hpp"

// graph.h wraps its declarations in extern "C" when __cplusplus is defined.
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
// link[1] arc chain (entries >= N) and reversed.
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

// Walks each vertex's link[0] (DFS child/back-edge) arc ring and records
// every edge (u, v) with v < u.
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
// neighbors in their existing relative order.
void MergeRotationIntoAdjacency(Graph &g, const Subgraph &sg, graphP boyer, size_t u) {
  std::vector<size_t> order;
  AdjacencyFromBoyer(boyer, static_cast<int>(u), order);

  size_t nbDegree = g.Degree(u);
  for (size_t i = 0; i < nbDegree; i++) {
    size_t v = g.Neighbor(u, i);
    if (!sg.HasEdge(u, v) && std::find(order.begin(), order.end(), v) == order.end())
      order.push_back(v);
  }

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
// raw-adjacency-index) pair for every vertex in the subgraph. Counts each
// subgraph vertex's full raw degree, including darts to neighbors outside
// the subgraph -- those darts are simply never visited by the enumeration
// loop below.
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
// @p face, until the starting dart (u, adjIdx) is revisited.
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

  // Every access below goes through faceList[i] by index, re-read fresh
  // each time: faceList.push_back() below can reallocate, which would
  // invalidate a cached pointer/reference into faceList.
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

          // Collect the vertices strictly between x and y before erasing
          // them, so erasing (which shifts later elements) can't skip one.
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
        break; // no eligible chord among this face's first four vertices
    }
  }

  // The insertions above shifted adjacency indices, invalidating the old
  // edge bits; re-derive the subgraph as full over the augmented graph.
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

  int res = gp_Embed(boyer, 0 /* no special embed flags */);
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

// Rotates a throwaway full subgraph over all of @p g in place and returns
// its vertex count for the base EmbeddedGraph constructor.
size_t Planar::ValidateAndRotate(Graph &g) {
  g.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(g);
  ApplyPlanarRotation(g, sg);
  return g.Size();
}

Planar::Planar(Graph &g) : EmbeddedGraph(ValidateAndRotate(g), 2), graph_(g) {
  SetPlanarEmbedded(true);
}

void Planar::Embed() {
  graph_.BuildLayout();
  Subgraph sg = Subgraph::CreateFull(graph_);

  FaceEnumerator faces(graph_, sg);
  Triangulate(graph_, sg, faces);

  SchnyderWood sw(graph_);
  sw.Embed(*this);
}

// ============================================================================
// PLANAR QUERIES ON ANY EMBEDDED GRAPH
// ============================================================================

Subgraph FaceSubgraph(const Graph &g, const std::vector<size_t> &face) {
  g.EnsureLayout();

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

  g.EnsureLayout();
  Subgraph sg = Subgraph::CreateFull(g);
  FaceEnumerator faces(g, sg);

  double minX = INFINITY, minY = INFINITY, maxX = -INFINITY, maxY = -INFINITY;
  for (size_t u : sg) {
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
