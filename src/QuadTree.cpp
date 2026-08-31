#include "QuadTree.hpp"

#include <algorithm>

namespace gviz {

QuadTree::Node *QuadTree::ArenaAlloc(double cx, double cy, double halfSize) {
  size_t blockIdx = nodesUsed_ / kArenaBlockNodes;
  size_t offset = nodesUsed_ % kArenaBlockNodes;

  if (blockIdx == nodeBlocks_.size()) {
    auto nodeBlock = std::make_unique<Node[]>(kArenaBlockNodes);
    auto pointBlock = nodesPerCell_ > 0
                           ? std::make_unique<size_t[]>(kArenaBlockNodes * nodesPerCell_)
                           : nullptr;

    // Reserve both indices before either push_back: unique_ptr's move
    // constructor is noexcept, so once capacity is reserved neither
    // push_back below can throw, and nodeBlocks_/pointBlocks_ can never end
    // up with mismatched sizes (which ArenaAlloc's indexing below assumes).
    nodeBlocks_.reserve(nodeBlocks_.size() + 1);
    pointBlocks_.reserve(pointBlocks_.size() + 1);
    nodeBlocks_.push_back(std::move(nodeBlock));
    pointBlocks_.push_back(std::move(pointBlock));
  }

  Node *node = &nodeBlocks_[blockIdx][offset];
  size_t *pointBlock = pointBlocks_[blockIdx].get();

  node->cx_ = cx;
  node->cy_ = cy;
  node->halfSize_ = halfSize;
  node->comX_ = 0.0;
  node->comY_ = 0.0;
  node->mass_ = 0.0;
  node->pointIndices_ = nodesPerCell_ > 0 ? &pointBlock[offset * nodesPerCell_] : nullptr;
  node->pointCount_ = 0;
  node->pointCapacity_ = nodesPerCell_;
  for (size_t i = 0; i < kQuadrantCount; i++)
    node->children_[i] = nullptr;

  nodesUsed_++;
  return node;
}

QuadTree::Quadrant QuadTree::QuadrantFor(const Node *node, double px, double py) const noexcept {
  bool east = px >= node->cx_;
  bool north = py >= node->cy_;

  if (north)
    return east ? Quadrant::NE : Quadrant::NW;
  return east ? Quadrant::SE : Quadrant::SW;
}

void QuadTree::Subdivide(Node *node) {
  double quarter = node->halfSize_ / 2.0;
  static constexpr double kOffsets[kQuadrantCount][2] = {
      {-1.0, 1.0},
      {1.0, 1.0},
      {-1.0, -1.0},
      {1.0, -1.0},
  };

  for (size_t i = 0; i < kQuadrantCount; i++) {
    node->children_[i] = ArenaAlloc(node->cx_ + kOffsets[i][0] * quarter,
                                     node->cy_ + kOffsets[i][1] * quarter, quarter);
  }
}

void QuadTree::GrowOverflow(Node *node) {
  size_t newCapacity = node->pointCapacity_ > 0 ? node->pointCapacity_ * 2 : 1;
  auto grown = std::make_unique<size_t[]>(newCapacity);
  std::copy_n(node->pointIndices_, node->pointCount_, grown.get());

  size_t *raw = grown.get();
  overflowBuffers_.push_back(std::move(grown));

  node->pointIndices_ = raw;
  node->pointCapacity_ = newCapacity;
}

void QuadTree::InsertPoint(Node *node, size_t idx) {
  double px = points_[2 * idx];
  double py = points_[2 * idx + 1];
  double ptMass = masses_[idx];

  double newMass = node->mass_ + ptMass;
  if (newMass > kMinMass || newMass < -kMinMass) {
    node->comX_ = (node->comX_ * node->mass_ + px * ptMass) / newMass;
    node->comY_ = (node->comY_ * node->mass_ + py * ptMass) / newMass;
  }
  node->mass_ = newMass;

  if (!node->IsLeaf()) {
    InsertPoint(node->children_[static_cast<size_t>(QuadrantFor(node, px, py))], idx);
    return;
  }

  if (node->pointCount_ < node->pointCapacity_) {
    node->pointIndices_[node->pointCount_++] = idx;
    return;
  }

  if (node->halfSize_ <= kMinHalfSize) {
    GrowOverflow(node);
    node->pointIndices_[node->pointCount_++] = idx;
    return;
  }

  Subdivide(node);

  for (size_t i = 0; i < node->pointCount_; i++) {
    size_t heldIdx = node->pointIndices_[i];
    double hx = points_[2 * heldIdx];
    double hy = points_[2 * heldIdx + 1];
    InsertPoint(node->children_[static_cast<size_t>(QuadrantFor(node, hx, hy))], heldIdx);
  }
  node->pointCount_ = 0;

  InsertPoint(node->children_[static_cast<size_t>(QuadrantFor(node, px, py))], idx);
}

void QuadTree::Build() {
  root_ = nullptr;

  if (pointCount_ == 0)
    return;

  double minX = points_[0], maxX = points_[0];
  double minY = points_[1], maxY = points_[1];
  for (size_t i = 1; i < pointCount_; i++) {
    double x = points_[2 * i], y = points_[2 * i + 1];
    minX = std::min(minX, x);
    maxX = std::max(maxX, x);
    minY = std::min(minY, y);
    maxY = std::max(maxY, y);
  }

  double span = std::max(maxX - minX, maxY - minY);
  double halfSize = span > 0.0 ? span / 2.0 : 1.0;
  double cx = (minX + maxX) / 2.0;
  double cy = (minY + maxY) / 2.0;

  // Only commit root_ once every point has been inserted successfully --
  // mirrors the old C's "tree->root = NULL" reset on a failed build, now
  // driven by exception propagation instead of a checked -1 return.
  Node *newRoot = ArenaAlloc(cx, cy, halfSize);
  for (size_t i = 0; i < pointCount_; i++)
    InsertPoint(newRoot, i);
  root_ = newRoot;
}

QuadTree::QuadTree(const double *points, const double *masses, size_t count,
                    size_t nodesPerCell)
    : nodesPerCell_(nodesPerCell), points_(points), masses_(masses), pointCount_(count) {
  Build();
}

void QuadTree::Rebuild(const double *points, const double *masses, size_t count) {
  overflowBuffers_.clear();
  nodesUsed_ = 0;
  points_ = points;
  masses_ = masses;
  pointCount_ = count;

  Build();
}

} // namespace gviz
