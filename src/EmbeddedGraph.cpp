#include "EmbeddedGraph.hpp"

#include "Vec.hpp"

#include <cstdio>
#include <cstring>
#include <ctime>

namespace gviz::layout {

EmbeddedGraph::EmbeddedGraph(size_t vertexCount, size_t dimension)
    : vertexCount_(vertexCount), dim_(dimension),
      positions_(vertexCount * dimension, 0.0) {
  drawMask_.visibleVertices = BitSet(vertexCount);
  drawMask_.visibleVertices.SetAll();
}

// DRAW MASK: ---------------------------------------------------------------

void EmbeddedGraph::SetDrawMaskEdgePolicy(DrawEdgePolicy edgePolicy) {
  drawMask_.edgePolicy = edgePolicy;
  drawMask_.revision++;
}

void EmbeddedGraph::DrawMaskShowVertex(size_t i) noexcept {
  drawMask_.visibleVertices.Set(i);
}

void EmbeddedGraph::DrawMaskHideVertex(size_t i) noexcept {
  drawMask_.visibleVertices.Clear(i);
}

void EmbeddedGraph::DrawMaskClearVertices() noexcept {
  drawMask_.visibleVertices.ClearAll();
}

void EmbeddedGraph::ResetDrawMask() {
  drawMask_.edgePolicy = DrawEdgePolicy::All;
  drawMask_.visibleVertices.SetAll();
  drawMask_.revision++;
}

bool EmbeddedGraph::IsVertexVisible(size_t i) const noexcept {
  return drawMask_.visibleVertices.Test(i);
}

bool EmbeddedGraph::IsEdgeVisible(size_t iu, size_t iv, bool edgeExists) const noexcept {
  switch (drawMask_.edgePolicy) {
  case DrawEdgePolicy::None:
    return false;
  case DrawEdgePolicy::IfBothVisible:
    return IsVertexVisible(iu) && IsVertexVisible(iv);
  case DrawEdgePolicy::All:
  default:
    return edgeExists;
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

// POSITIONS: ---------------------------------------------------------------

void EmbeddedGraph::SetVPosition(size_t i, const double *position) noexcept {
  VecCopy(dim_, position, GetVPosition(i));
}

void EmbeddedGraph::AddVPosition(size_t i, const double *position) noexcept {
  VecAxpy(dim_, 1.0, position, GetVPosition(i));
}

void EmbeddedGraph::RandomizePositions(double boxExtent, unsigned int seed) {
  if (seed == 0)
    seed = static_cast<unsigned int>(time(nullptr));

  std::vector<double> pos(dim_);
  for (size_t i = 0; i < vertexCount_; i++) {
    for (size_t d = 0; d < dim_; d++) {
      double unit = static_cast<double>(rand_r(&seed)) /
                    (static_cast<double>(RAND_MAX) + 1.0);
      pos[d] = boxExtent * (2.0 * unit - 1.0);
    }
    SetVPosition(i, pos.data());
  }
}

// SAVE/LOAD: -----------------------------------------------------------------

bool EmbeddedGraph::SaveEmbedding(const char *name, const char *filename) const {
  FILE *f = fopen(filename, "w");
  if (!f)
    return false;

  fprintf(f, "%s\n", name);
  fprintf(f, "%zu %zu\n", vertexCount_, dim_);
  for (size_t i = 0; i < vertexCount_; i++) {
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
  if (vertexCount != vertexCount_ || dim != dim_) {
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
