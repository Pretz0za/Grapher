#ifndef GVIZ_QUADTREE_HPP
#define GVIZ_QUADTREE_HPP

#include <cstddef>
#include <memory>
#include <vector>

namespace gviz {

/**
 * A quadtree indexing a fixed buffer of 2D points -- the Barnes-Hut spatial
 * index used by force-directed layout's repulsion approximation. Purely
 * spatial: no knowledge of graphs, vertices, or edges.
 *
 * points/masses are borrowed, not owned or copied -- refreshed and
 * re-pointed by the caller every simulation step (see Rebuild). points is
 * interleaved x0, y0, x1, y1, ...; masses has one entry per point. Every
 * Node's mass is the sum of the masses of the points in its subtree, and
 * its center of mass is their mass-weighted average.
 *
 * Node objects live in an arena of fixed-size blocks (kArenaBlockNodes
 * each), reused across Rebuild() calls and never shrunk, so a Node* handed
 * out by Root()/Child() stays valid across an entire Barnes-Hut traversal
 * even as later rebuilds grow the arena. A leaf that overflows its per-cell
 * point capacity gets an individually growable overflow buffer, freed and
 * reallocated fresh on every Rebuild.
 */
class QuadTree {
public:
  /** Default leaf capacity; see the constructor. */
  static constexpr size_t kNodesPerCellDefault = 1;

  /** A child's position within its parent's square region, y-axis up. */
  enum class Quadrant : int { NW = 0, NE = 1, SW = 2, SE = 3 };
  static constexpr size_t kQuadrantCount = 4;

  /**
   * One node of a QuadTree: a square region of 2D space. A leaf (every
   * Child() is nullptr) holds up to PointCount()-many point indices
   * directly; once a leaf would overflow its capacity it subdivides into
   * four equal quadrants (see Quadrant) and redistributes its points among
   * them.
   *
   * Nodes are owned by the tree's arena and are never individually
   * constructed or destroyed by callers -- only ever reached through
   * QuadTree::Root()/Child(). Every accessor here is unchecked/noexcept:
   * this is walked in a tight Barnes-Hut traversal loop.
   */
  class Node {
  public:
    double CenterX() const noexcept { return cx_; }
    double CenterY() const noexcept { return cy_; }
    double HalfSize() const noexcept { return halfSize_; }

    /** Sum of the caller-supplied masses of every point in this subtree. */
    double Mass() const noexcept { return mass_; }

    /** Writes the center of mass of every point in this subtree to @p outX
     *  and @p outY. Either may be nullptr to ignore that coordinate. */
    void CenterOfMass(double *outX, double *outY) const noexcept {
      if (outX)
        *outX = comX_;
      if (outY)
        *outY = comY_;
    }

    /** Whether this node holds points directly, i.e. has not subdivided. */
    bool IsLeaf() const noexcept { return children_[0] == nullptr; }

    /** Returns the @p quadrant child, or nullptr when IsLeaf(). */
    const Node *Child(Quadrant quadrant) const noexcept {
      return children_[static_cast<size_t>(quadrant)];
    }

    /** Points held directly by this node -- 0 for internal nodes; use
     *  Mass()/CenterOfMass() for their subtree aggregate. */
    size_t PointCount() const noexcept { return IsLeaf() ? pointCount_ : 0; }

    /** Source-buffer index of the @p i-th point held directly by this leaf.
     *  Unchecked; @p i must be < PointCount(). */
    size_t PointAt(size_t i) const noexcept { return pointIndices_[i]; }

  private:
    friend class QuadTree;

    double cx_ = 0.0, cy_ = 0.0;
    double halfSize_ = 0.0;
    double comX_ = 0.0, comY_ = 0.0;
    double mass_ = 0.0;
    size_t *pointIndices_ = nullptr;
    size_t pointCount_ = 0;
    size_t pointCapacity_ = 0;
    Node *children_[kQuadrantCount] = {nullptr, nullptr, nullptr, nullptr};
  };

  /**
   * Builds a quadtree over @p points, an interleaved x0, y0, x1, y1, ...
   * buffer describing @p count 2D points, and allocates its node arena.
   * @p points/@p masses are borrowed and must outlive the tree (or until the
   * next Rebuild re-points them). @p nodesPerCell is the maximum number of
   * points a leaf holds before it subdivides; pass kNodesPerCellDefault for
   * the default of 1.
   *
   * @throws std::bad_alloc on allocation failure.
   */
  QuadTree(const double *points, const double *masses, size_t count,
           size_t nodesPerCell = kNodesPerCellDefault);

  QuadTree(const QuadTree &) = delete;
  QuadTree &operator=(const QuadTree &) = delete;
  QuadTree(QuadTree &&) noexcept = default;
  QuadTree &operator=(QuadTree &&) noexcept = default;
  ~QuadTree() = default;

  /**
   * Rebuilds the tree over @p points (a new buffer, or the same one with
   * updated coordinates), @p masses (parallel to @p points), and @p count,
   * reusing the arena allocated at construction instead of freeing and
   * reallocating it. Cheap -- no allocation at all -- once the arena has
   * grown to cover the largest build seen so far.
   */
  void Rebuild(const double *points, const double *masses, size_t count);

  /** Returns the root node, or nullptr if the tree indexes zero points. */
  const Node *Root() const noexcept { return root_; }

  /** Number of arena node blocks allocated so far. Diagnostic: lets tests
   *  verify a Rebuild() no larger than before allocates no new block. */
  size_t NodeBlockCount() const noexcept { return nodeBlocks_.size(); }

  /** Number of overflow point buffers currently held (freed and rebuilt
   *  every Rebuild()). Diagnostic. */
  size_t OverflowBufferCount() const noexcept { return overflowBuffers_.size(); }

private:
  static constexpr double kMinHalfSize = 1e-9;
  static constexpr double kMinMass = 1e-9;
  static constexpr size_t kArenaBlockNodes = 1024;

  Node *ArenaAlloc(double cx, double cy, double halfSize);
  Quadrant QuadrantFor(const Node *node, double px, double py) const noexcept;
  void Subdivide(Node *node);
  void GrowOverflow(Node *node);
  void InsertPoint(Node *node, size_t idx);
  void Build();

  size_t nodesPerCell_;
  const double *points_ = nullptr;
  const double *masses_ = nullptr;
  size_t pointCount_ = 0;

  Node *root_ = nullptr; /**< nullptr when the tree indexes zero points. */

  /** Owned; Node[] arena blocks. Individually allocated, not a flat
   *  std::vector<Node>, so a live Node* is never invalidated by growth. */
  std::vector<std::unique_ptr<Node[]>> nodeBlocks_;
  /** Owned; size_t[] point-storage blocks, one per entry of nodeBlocks_ (and
   *  always the same length -- see ArenaAlloc). Null entries when
   *  nodesPerCell_ == 0 (every leaf overflows immediately). */
  std::vector<std::unique_ptr<size_t[]>> pointBlocks_;
  size_t nodesUsed_ = 0; /**< Nodes handed out in the current build. */
  /** Owned; size_t[] buffers backing leaves that outgrew their block-reserved
   *  capacity. Freed at the start of every Rebuild(). */
  std::vector<std::unique_ptr<size_t[]>> overflowBuffers_;
};

} // namespace gviz

#endif
