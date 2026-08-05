#include "Graph.hpp"

#include <algorithm>

namespace gviz {

Graph::Graph(bool directed, size_t initialCapacity) : directed_(directed) {
  vertices_.reserve(initialCapacity);
}

Graph::Graph(const Graph &other)
    : vertices_(other.vertices_), directed_(other.directed_),
      mutationCount_(other.mutationCount_), layout_(nullptr) {}

Graph &Graph::operator=(const Graph &other) {
  if (this != &other) {
    vertices_ = other.vertices_;
    directed_ = other.directed_;
    mutationCount_ = other.mutationCount_;
    layout_.reset();
  }
  return *this;
}

size_t Graph::EdgeCount() const noexcept {
  return layout_ ? layout_->edgeCount : 0;
}

void Graph::BuildLayout() const {
  size_t n = vertices_.size();
  if (!layout_)
    layout_ = std::make_unique<Layout>();

  layout_->vertexOffsets.assign(n + 1, 0);
  size_t off = 0;
  for (size_t i = 0; i < n; i++) {
    layout_->vertexOffsets[i] = off;
    off += vertices_[i].edges.size();
  }
  layout_->vertexOffsets[n] = off;
  layout_->edgeCount = directed_ ? off : off / 2;
  layout_->builtAtMutation = mutationCount_;
}

void Graph::EnsureLayout() const {
  if (layout_ && layout_->builtAtMutation == mutationCount_)
    return;
  BuildLayout();
}

Graph Graph::Reversed() const {
  if (!directed_)
    return *this;

  Graph result(directed_, vertices_.size());
  result.vertices_.resize(vertices_.size());
  for (size_t i = 0; i < vertices_.size(); i++)
    result.vertices_[i].data = vertices_[i].data;

  for (size_t u = 0; u < vertices_.size(); u++) {
    for (const Edge &e : vertices_[u].edges)
      result.vertices_[e.idx].edges.push_back(Edge{u, e.weight});
  }

  return result;
}

size_t Graph::AddVertex(void *data) {
  vertices_.push_back(Vertex{data, {}});
  mutationCount_++;
  return vertices_.size() - 1;
}

void Graph::Clear() {
  if (vertices_.empty())
    return;
  vertices_.clear();
  mutationCount_++;
  layout_.reset();
}

void Graph::AddEdge(size_t from, size_t to, double weight) {
  vertices_[from].edges.push_back(Edge{to, weight});
  if (!directed_)
    vertices_[to].edges.push_back(Edge{from, weight});
  mutationCount_++;
}

bool Graph::RemoveEdge(size_t from, size_t to) {
  auto &fromEdges = vertices_[from].edges;
  auto it = std::find_if(fromEdges.begin(), fromEdges.end(),
                          [to](const Edge &e) { return e.idx == to; });
  if (it == fromEdges.end())
    return false;
  fromEdges.erase(it);

  if (!directed_) {
    auto &toEdges = vertices_[to].edges;
    auto it2 = std::find_if(toEdges.begin(), toEdges.end(),
                             [from](const Edge &e) { return e.idx == from; });
    if (it2 == toEdges.end())
      return false;
    toEdges.erase(it2);
  }

  mutationCount_++;
  return true;
}

void *Graph::GetVertexData(size_t idx) const {
  if (idx >= vertices_.size())
    return nullptr;
  return vertices_[idx].data;
}

void Graph::SetVertexData(size_t idx, void *data) noexcept {
  vertices_[idx].data = data;
}

bool Graph::NeighborPosition(size_t from, size_t to, size_t &outPos) const noexcept {
  const auto &edges = vertices_[from].edges;
  for (size_t i = 0; i < edges.size(); i++) {
    if (edges[i].idx == to) {
      outPos = i;
      return true;
    }
  }
  return false;
}

bool Graph::EdgeExists(size_t from, size_t to) const noexcept {
  size_t pos;
  return NeighborPosition(from, to, pos);
}

bool Graph::GetEdgeWeight(size_t from, size_t to, double &outWeight) const noexcept {
  const auto &edges = vertices_[from].edges;
  auto it = std::find_if(edges.begin(), edges.end(),
                          [to](const Edge &e) { return e.idx == to; });
  if (it == edges.end())
    return false;
  outWeight = it->weight;
  return true;
}

bool Graph::SetEdgeWeight(size_t from, size_t to, double weight) noexcept {
  auto &fromEdges = vertices_[from].edges;
  auto it = std::find_if(fromEdges.begin(), fromEdges.end(),
                          [to](const Edge &e) { return e.idx == to; });
  if (it == fromEdges.end())
    return false;
  it->weight = weight;

  if (!directed_) {
    auto &toEdges = vertices_[to].edges;
    auto it2 = std::find_if(toEdges.begin(), toEdges.end(),
                             [from](const Edge &e) { return e.idx == from; });
    if (it2 == toEdges.end())
      return false;
    it2->weight = weight;
  }

  return true;
}

bool Graph::InsertNeighborAt(size_t from, size_t to, double weight, size_t pos) {
  auto &edges = vertices_[from].edges;
  if (pos > edges.size())
    return false;
  edges.insert(edges.begin() + static_cast<std::ptrdiff_t>(pos), Edge{to, weight});
  mutationCount_++;
  return true;
}

bool Graph::ReorderNeighbors(size_t idx, const std::vector<size_t> &order) {
  auto &edges = vertices_[idx].edges;
  if (order.size() != edges.size())
    return false;

  std::vector<Edge> rebuilt;
  rebuilt.reserve(order.size());
  for (size_t v : order) {
    auto it = std::find_if(edges.begin(), edges.end(),
                            [v](const Edge &e) { return e.idx == v; });
    if (it == edges.end())
      return false;
    rebuilt.push_back(*it);
  }

  edges = std::move(rebuilt);
  return true;
}

} // namespace gviz
