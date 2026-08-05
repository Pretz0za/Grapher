#include "EmbeddedGraph.hpp"

#include "Vec.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace gviz::layout {

namespace {

/**
 * The out- (and, for directed graphs, in-) adjacency CSRs over raw ids
 * [0, rawCount) for @p sg as it stands right now. Built entirely into fresh
 * local vectors and handed back by value, so a throwing allocation partway
 * through never touches the caller's already-published CSR members --
 * mirrors the old C buildSyncedAdjacency's "build into temporaries, publish
 * only on success" shape, now for free via RVO/move instead of manual
 * malloc-then-rollback bookkeeping.
 */
struct SyncedAdjacency {
  std::vector<size_t> outOffsets;
  std::vector<size_t> outNeighbors;
  std::vector<size_t> inOffsets;
  std::vector<size_t> inNeighbors;
};

SyncedAdjacency BuildSyncedAdjacency(const Subgraph &sg, size_t rawCount,
                                      bool directed) {
  SyncedAdjacency adj;
  adj.outOffsets.assign(rawCount + 1, size_t{0});
  if (directed)
    adj.inOffsets.assign(rawCount + 1, size_t{0});

  for (size_t u : sg) {
    for (size_t v : sg.Neighbors(u)) {
      adj.outOffsets[u + 1]++;
      if (directed)
        adj.inOffsets[v + 1]++;
    }
  }
  for (size_t i = 0; i < rawCount; i++) {
    adj.outOffsets[i + 1] += adj.outOffsets[i];
    if (directed)
      adj.inOffsets[i + 1] += adj.inOffsets[i];
  }

  adj.outNeighbors.resize(adj.outOffsets[rawCount]);
  std::vector<size_t> inCursor;
  if (directed) {
    adj.inNeighbors.resize(adj.inOffsets[rawCount]);
    inCursor.assign(adj.inOffsets.begin(), adj.inOffsets.begin() + rawCount);
  }

  size_t outCursor = 0;
  for (size_t u : sg) {
    for (size_t v : sg.Neighbors(u)) {
      adj.outNeighbors[outCursor++] = v;
      if (directed)
        adj.inNeighbors[inCursor[v]++] = u;
    }
  }

  return adj;
}

} // namespace

EmbeddedGraph::EmbeddedGraph(Subgraph subgraph, size_t dimension)
    : subgraph_(std::move(subgraph)), dim_(dimension),
      positions_(subgraph_.ParentSize() * dimension, 0.0),
      syncedGraphSize_(subgraph_.ParentSize()) {
  drawMask_.visibleVertices = BitSet(subgraph_.ParentSize());
  ShowAllSubgraphVerticesInMask();
}

void EmbeddedGraph::ShowAllSubgraphVerticesInMask() noexcept {
  for (size_t u : subgraph_)
    drawMask_.visibleVertices.Set(u);
}

// GROWTH & SYNC: ---------------------------------------------------------------

bool EmbeddedGraph::Sync() {
  uint64_t currentMutation = subgraph_.ParentMutationCount();
  if (syncedMutationCount_ == currentMutation)
    return false;

  size_t oldCap = subgraph_.VertexCapacity();
  subgraph_.Rebuild();
  size_t newCap = subgraph_.VertexCapacity();
  size_t newRaw = subgraph_.ParentSize();

  if (newCap > oldCap) {
    positions_.resize(newCap * dim_, 0.0);
    drawMask_.visibleVertices.Resize(newCap);
  }

  // Admit every vertex added since the last commit (position slots are
  // already zeroed by the growth above). Vertices that existed when this
  // object was constructed keep whatever membership the caller chose.
  for (size_t v = syncedGraphSize_; v < newRaw; v++) {
    subgraph_.ShowVertex(v);
    drawMask_.visibleVertices.Set(v);
  }

  SyncedAdjacency adj =
      BuildSyncedAdjacency(subgraph_, newRaw, subgraph_.ParentIsDirected());

  outNeighborOffsets_ = std::move(adj.outOffsets);
  outNeighbors_ = std::move(adj.outNeighbors);
  inNeighborOffsets_ = std::move(adj.inOffsets);
  inNeighbors_ = std::move(adj.inNeighbors);
  syncedGraphSize_ = newRaw;
  syncedMutationCount_ = currentMutation;
  DrawMaskNotifyChanged();

  return true;
}

size_t EmbeddedGraph::OutDegree(size_t v) const noexcept {
  if (outNeighborOffsets_.empty() || v >= syncedGraphSize_)
    return 0;
  return outNeighborOffsets_[v + 1] - outNeighborOffsets_[v];
}

std::span<const size_t> EmbeddedGraph::OutNeighbors(size_t v) const noexcept {
  if (outNeighborOffsets_.empty() || v >= syncedGraphSize_)
    return {};
  return std::span<const size_t>(outNeighbors_.data() + outNeighborOffsets_[v],
                                  outNeighborOffsets_[v + 1] - outNeighborOffsets_[v]);
}

size_t EmbeddedGraph::InDegree(size_t v) const noexcept {
  if (inNeighborOffsets_.empty() || v >= syncedGraphSize_)
    return 0;
  return inNeighborOffsets_[v + 1] - inNeighborOffsets_[v];
}

std::span<const size_t> EmbeddedGraph::InNeighbors(size_t v) const noexcept {
  if (inNeighborOffsets_.empty() || v >= syncedGraphSize_)
    return {};
  return std::span<const size_t>(inNeighbors_.data() + inNeighborOffsets_[v],
                                  inNeighborOffsets_[v + 1] - inNeighborOffsets_[v]);
}

// DRAW MASK: ---------------------------------------------------------------

void EmbeddedGraph::SetDrawMaskEdgePolicy(DrawEdgePolicy edgePolicy) {
  drawMask_.edgePolicy = edgePolicy;
  drawMask_.revision++;
}

void EmbeddedGraph::DrawMaskShowVertex(size_t u) noexcept {
  drawMask_.visibleVertices.Set(u);
}

void EmbeddedGraph::DrawMaskHideVertex(size_t u) noexcept {
  drawMask_.visibleVertices.Clear(u);
}

void EmbeddedGraph::DrawMaskClearVertices() noexcept {
  drawMask_.visibleVertices.ClearAll();
}

void EmbeddedGraph::ResetDrawMask() {
  drawMask_.edgePolicy = DrawEdgePolicy::All;
  ShowAllSubgraphVerticesInMask();
  drawMask_.revision++;
}

bool EmbeddedGraph::IsVertexVisible(size_t u) const noexcept {
  if (!subgraph_.HasVertex(u))
    return false;
  return drawMask_.visibleVertices.Test(u);
}

bool EmbeddedGraph::IsEdgeVisible(size_t u, size_t v) const noexcept {
  switch (drawMask_.edgePolicy) {
  case DrawEdgePolicy::None:
    return false;
  case DrawEdgePolicy::IfBothVisible:
    return IsVertexVisible(u) && IsVertexVisible(v);
  case DrawEdgePolicy::All:
  default:
    return subgraph_.HasEdge(u, v);
  }
}

// ACTIONS: -------------------------------------------------------------------

namespace {
// Single const-correct implementation shared by every registry lookup
// (Action, StatSeries): both structs carry a `const char *name` first
// member. Callers that hold a non-const container and need a mutable
// pointer const_cast the *result* (always safe: they already have
// unrestricted access to the object the pointer points into), rather than
// duplicating this loop or const_cast-ing the container argument itself.
template <typename T>
const T *FindByName(const std::vector<T> &items, const char *name) {
  if (!name)
    return nullptr;
  for (const auto &item : items) {
    if (std::strcmp(item.name, name) == 0)
      return &item;
  }
  return nullptr;
}
} // namespace

bool EmbeddedGraph::AddAction(const char *name, ActionHandler handler,
                               void *userData) {
  if (!name || !handler)
    return false;

  if (auto *existing = const_cast<Action *>(FindByName(actions_, name))) {
    existing->handler = handler;
    existing->userData = userData;
    return true;
  }

  actions_.push_back(Action{name, handler, userData});
  return true;
}

bool EmbeddedGraph::RemoveAction(const char *name) {
  auto *found = const_cast<Action *>(FindByName(actions_, name));
  if (!found)
    return false;
  *found = actions_.back();
  actions_.pop_back();
  return true;
}

const Action *EmbeddedGraph::FindAction(const char *name) const noexcept {
  return FindByName(actions_, name);
}

const Action *EmbeddedGraph::ActionAt(size_t idx) const noexcept {
  return idx < actions_.size() ? &actions_[idx] : nullptr;
}

bool EmbeddedGraph::InvokeAction(const char *name,
                                  const ActionPayload *payload) {
  const Action *action = FindAction(name);
  if (!action)
    return false;

  ActionPayload zeroed{};
  action->handler(*this, action->userData, payload ? *payload : zeroed);
  return true;
}

// STATS: -----------------------------------------------------------------------

StatSeries *EmbeddedGraph::AddStatSeries(const char *name, StatChartKind kind) {
  if (!name)
    return nullptr;

  if (auto *existing = const_cast<StatSeries *>(FindByName(stats_, name))) {
    existing->kind = kind;
    return existing;
  }

  stats_.push_back(StatSeries{name, kind, {}, 0});
  return &stats_.back();
}

bool EmbeddedGraph::StatAppend(const char *name, double value) {
  if (!name)
    return false;

  auto *series = const_cast<StatSeries *>(FindByName(stats_, name));
  if (!series)
    series = AddStatSeries(name, StatChartKind::Line);

  series->samples.push_back(value);
  series->revision++;
  return true;
}

void EmbeddedGraph::StatClear(const char *name) {
  auto *series = const_cast<StatSeries *>(FindByName(stats_, name));
  if (!series)
    return;
  series->samples.clear();
  series->revision++;
}

const StatSeries *EmbeddedGraph::StatSeriesAt(size_t idx) const noexcept {
  return idx < stats_.size() ? &stats_[idx] : nullptr;
}

const StatSeries *EmbeddedGraph::FindStatSeries(const char *name) const noexcept {
  return FindByName(stats_, name);
}

// HIGHLIGHT: -------------------------------------------------------------------

void EmbeddedGraph::SetHighlight(Subgraph highlight) {
  highlight_.emplace(std::move(highlight));
}

// POSITIONS: ---------------------------------------------------------------

void EmbeddedGraph::SetVPosition(size_t idx, const double *position) noexcept {
  VecCopy(dim_, position, GetVPosition(idx));
}

void EmbeddedGraph::AddVPosition(size_t idx, const double *position) noexcept {
  VecAxpy(dim_, 1.0, position, GetVPosition(idx));
}

void EmbeddedGraph::RandomizePositions(double boxExtent, unsigned int seed) {
  if (seed == 0)
    seed = static_cast<unsigned int>(time(nullptr));

  std::vector<double> pos(dim_);
  for (size_t u : subgraph_) {
    for (size_t d = 0; d < dim_; d++) {
      double unit = static_cast<double>(rand_r(&seed)) /
                    (static_cast<double>(RAND_MAX) + 1.0);
      pos[d] = boxExtent * (2.0 * unit - 1.0);
    }
    SetVPosition(u, pos.data());
  }
}

// SAVE/LOAD: -----------------------------------------------------------------

bool EmbeddedGraph::SaveEmbedding(const char *name, const char *filename) const {
  FILE *f = fopen(filename, "w");
  if (!f)
    return false;

  fprintf(f, "%s\n", name);
  fprintf(f, "%zu %zu\n", syncedGraphSize_, dim_);
  for (size_t i = 0; i < syncedGraphSize_; i++) {
    const double *pos = GetVPosition(i);
    for (size_t j = 0; j < dim_; j++)
      fprintf(f, "%f ", pos[j]);
    fprintf(f, "\n");
  }

  fclose(f);
  return true;
}

bool EmbeddedGraph::LoadEmbedding(const char *filename) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return false;

  char name[256];
  if (!fgets(name, sizeof(name), f)) {
    fclose(f);
    return false;
  }

  size_t vertexCount, dim;
  if (fscanf(f, "%zu %zu", &vertexCount, &dim) != 2) {
    fclose(f);
    return false;
  }
  if (vertexCount != syncedGraphSize_ || dim != dim_) {
    fclose(f);
    return false;
  }

  for (size_t i = 0; i < vertexCount; i++) {
    double *pos = GetVPosition(i);
    for (size_t j = 0; j < dim; j++) {
      if (fscanf(f, "%lf", &pos[j]) != 1) {
        fclose(f);
        return false;
      }
    }
  }

  fclose(f);
  return true;
}

} // namespace gviz::layout
