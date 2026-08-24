#include "Subgraph.hpp"

#include "Error.hpp"

namespace gviz {

namespace {

/** Binary search for the vertex owning flat bit index @p bit under a given
 *  vertexOffsets prefix-sum array; writes the within-vertex adjacency index
 *  to @p outIdx. */
size_t VertexFromBit(const std::vector<size_t> &offsets, size_t bit, size_t &outIdx) {
  size_t lo = 0;
  size_t hi = offsets.size() - 1;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (offsets[mid + 1] <= bit)
      lo = mid + 1;
    else
      hi = mid;
  }
  outIdx = bit - offsets[lo];
  return lo;
}

} // namespace

Subgraph Subgraph::CreateVertexInduced(const Graph &g) { return Subgraph(g); }

Subgraph Subgraph::CreateEmpty(const Graph &g) {
  if (!g.HasLayout())
    throw NoLayoutError();
  Subgraph sg(g);
  sg.edgeBits_ = std::make_unique<BitSet>(g.layout_->vertexOffsets[g.Size()]);
  return sg;
}

Subgraph Subgraph::CreateFull(const Graph &g) {
  Subgraph sg = CreateEmpty(g);
  sg.MakeFull();
  return sg;
}

Subgraph::Subgraph(const Subgraph &other)
    : g_(other.g_), vertexBits_(other.vertexBits_),
      edgeBits_(other.edgeBits_ ? std::make_unique<BitSet>(*other.edgeBits_) : nullptr) {}

uint64_t Subgraph::ParentMutationCount() const noexcept { return g_.MutationCount(); }
size_t Subgraph::ParentSize() const noexcept { return g_.Size(); }
bool Subgraph::ParentIsDirected() const noexcept { return g_.IsDirected(); }

bool Subgraph::HasVertex(size_t u) const noexcept {
  if (u >= vertexBits_.Size())
    return false;
  return vertexBits_.Test(u);
}

bool Subgraph::HasEdge(size_t u, size_t v) const noexcept {
  if (u >= vertexBits_.Size() || v >= vertexBits_.Size())
    return false;
  if (!vertexBits_.Test(u) || !vertexBits_.Test(v))
    return false;

  size_t idx;
  if (!g_.NeighborPosition(u, v, idx))
    return false;

  if (IsFull())
    return edgeBits_->Test(g_.layout_->vertexOffsets[u] + idx);
  return true;
}

size_t Subgraph::Degree(size_t u) const noexcept {
  if (u >= vertexBits_.Size() || !vertexBits_.Test(u))
    return 0;

  if (IsFull()) {
    size_t start = g_.layout_->vertexOffsets[u];
    size_t end = g_.layout_->vertexOffsets[u + 1];
    return edgeBits_->PopcountRange(start, end);
  }

  size_t degree = g_.Degree(u);
  size_t count = 0;
  for (size_t i = 0; i < degree; i++) {
    if (HasVertex(g_.Neighbor(u, i)))
      count++;
  }
  return count;
}

size_t Subgraph::EdgeCount() const noexcept {
  if (IsFull())
    return edgeBits_->Popcount();

  size_t count = 0;
  for (size_t u : *this) {
    size_t degree = g_.Degree(u);
    for (size_t i = 0; i < degree; i++) {
      if (HasVertex(g_.Neighbor(u, i)))
        count++;
    }
  }
  return count;
}

void Subgraph::HideVertex(size_t u) noexcept {
  vertexBits_.Clear(u);
  if (IsFull())
    ClearIncidentEdges(u);
}

void Subgraph::ShowEdge(size_t u, size_t v) noexcept {
  if (!IsFull())
    return;
  size_t idx;
  if (!g_.NeighborPosition(u, v, idx))
    return;
  edgeBits_->Set(g_.layout_->vertexOffsets[u] + idx);
}

void Subgraph::HideEdge(size_t u, size_t v) noexcept {
  if (!IsFull())
    return;
  size_t idx;
  if (!g_.NeighborPosition(u, v, idx))
    return;
  edgeBits_->Clear(g_.layout_->vertexOffsets[u] + idx);
}

void Subgraph::ClearIncidentEdges(size_t u) noexcept {
  size_t uStart = g_.layout_->vertexOffsets[u];
  size_t uEnd = g_.layout_->vertexOffsets[u + 1];
  edgeBits_->ClearRange(uStart, uEnd);

  for (size_t pos : *edgeBits_) {
    if (pos >= uStart && pos < uEnd)
      continue;
    size_t idx;
    size_t w = VertexFromBit(g_.layout_->vertexOffsets, pos, idx);
    if (w == u)
      continue;
    if (idx >= g_.Degree(w))
      continue;
    if (g_.Neighbor(w, idx) == u)
      edgeBits_->Clear(pos);
  }
}

void Subgraph::MakeEdgeSubset() {
  if (edgeBits_ || !g_.HasLayout())
    return;

  edgeBits_ = std::make_unique<BitSet>(g_.layout_->vertexOffsets[g_.Size()]);

  for (size_t u : *this) {
    size_t degree = g_.Degree(u);
    for (size_t idx = 0; idx < degree; idx++) {
      size_t v = g_.Neighbor(u, idx);
      if (HasVertex(v))
        edgeBits_->Set(g_.layout_->vertexOffsets[u] + idx);
    }
  }
}

void Subgraph::MakeFull() noexcept {
  if (!IsFull() || !g_.HasLayout())
    return;
  vertexBits_.SetAll();
  edgeBits_->SetAll();
}

void Subgraph::RebuildVertexInduced() {
  size_t needed = g_.Size();
  size_t capacity = vertexBits_.Size();
  if (needed <= capacity)
    return;

  size_t newCap = capacity ? capacity * 2 : 64;
  while (newCap < needed)
    newCap *= 2;

  vertexBits_.Resize(newCap);
}

void Subgraph::RebuildFull() {
  std::unique_ptr<BitSet> oldEdgeBits = std::move(edgeBits_);
  std::vector<size_t> oldOffsets;
  if (g_.HasLayout())
    oldOffsets = g_.layout_->vertexOffsets;

  g_.BuildLayout();

  size_t newNVerts = g_.Size();
  vertexBits_.Resize(newNVerts);

  size_t newEdgeBitCount = g_.layout_->vertexOffsets[newNVerts];
  auto migrated = std::make_unique<BitSet>(newEdgeBitCount);

  if (oldEdgeBits && !oldOffsets.empty()) {
    size_t oldEdgeBitCount = oldOffsets.back();
    for (size_t pos : oldEdgeBits->Range(0, oldEdgeBitCount)) {
      size_t idx;
      size_t u = VertexFromBit(oldOffsets, pos, idx);
      if (idx >= g_.Degree(u))
        continue;
      size_t v = g_.Neighbor(u, idx);
      size_t idxNew;
      if (!g_.NeighborPosition(u, v, idxNew))
        continue;
      migrated->Set(g_.layout_->vertexOffsets[u] + idxNew);
    }
  }

  edgeBits_ = std::move(migrated);
}

void Subgraph::Rebuild() {
  if (IsFull())
    RebuildFull();
  else
    RebuildVertexInduced();
}

Subgraph::NeighborIterator Subgraph::MakeNeighborIterator(size_t u) const noexcept {
  NeighborIterator it;
  if (u >= vertexBits_.Size() || !vertexBits_.Test(u))
    return it;

  it.sg_ = this;
  it.u_ = u;
  it.done_ = false;

  if (IsFull()) {
    size_t start = g_.layout_->vertexOffsets[u];
    size_t end = g_.layout_->vertexOffsets[u + 1];
    it.base_ = start;
    it.mode_ = NeighborIterator::Mode::Full;
    BitSet::RangeView range = edgeBits_->Range(start, end);
    it.edgeIt_ = range.begin();
    it.edgeEnd_ = range.end();
  } else {
    it.mode_ = NeighborIterator::Mode::Induced;
    it.adjIdx_ = 0;
    it.adjDegree_ = g_.Degree(u);
  }

  it.Step();
  return it;
}

bool Subgraph::NeighborIterator::Step() {
  if (mode_ == Mode::Full) {
    if (edgeIt_ == edgeEnd_) {
      done_ = true;
      return false;
    }
    size_t pos = *edgeIt_;
    ++edgeIt_;
    size_t idx = pos - base_;
    if (idx >= sg_->g_.Degree(u_)) {
      done_ = true;
      return false;
    }
    current_ = sg_->g_.Neighbor(u_, idx);
    return true;
  }

  while (adjIdx_ < adjDegree_) {
    size_t v = sg_->g_.Neighbor(u_, adjIdx_++);
    if (sg_->HasVertex(v)) {
      current_ = v;
      return true;
    }
  }
  done_ = true;
  return false;
}

} // namespace gviz
