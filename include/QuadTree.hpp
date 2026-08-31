#ifndef GVIZ_QUADTREE_HPP
#define GVIZ_QUADTREE_HPP

#include <cstddef>
#include <memory>
#include <vector>

namespace gviz {

/**
 * A quadtree indexing a fixed buffer of 2D points -- the Barnes-Hut spatial
 * index used by force-directed layout's repulsion approximation. Purely
 * spatial: it has no knowledge of graphs, vertices, or edges, and takes raw
 * coordinates only.
 *
 * points/masses are borrowed, not owned or copied -- they are refreshed and
 * re-pointed by the caller every simulation step (see Rebuild). points is
 * interleaved x0, y0, x1, y1, ...; masses has one entry per point (not
 * interleaved), same indexing as point index. Every Node's mass (see below)
 * is the sum of the masses of the points in its subtree, and its center of
 * mass is their mass-weighted average -- what lets a Barnes-Hut traversal
 * approximate a whole subtree by one point instead of visiting every leaf.
 *
 * Arena allocation, the one thing that matters here: Node objects are never
 * individually new'd/deleted. They are carved out of fixed-size blocks
 * (kArenaBlockNodes each) that are each allocated once, as a
 * std::unique_ptr<Node[]>, and reused across Rebuild() calls -- the intended
 * usage is one construction followed by many Rebuild()s (e.g. once per
 * force-layout iteration, potentially thousands of times per embedding run),
 * with the arena only ever growing (never shrinking or reallocating existing
 * blocks) when a rebuild needs more nodes than any previous one has. Point
 * index storage for steady-state (non-overflowing) leaves is carved out of a
 * second, parallel block arena the same way, so a typical Rebuild allocates
 * nothing at all once the arena has grown to cover the largest tree built so
 * far.
 *
 * Why std::vector<std::unique_ptr<Node[]>> and not std::vector<Node>: the
 * outer std::vector of block pointers can safely grow on its own (appending
 * a new block only moves unique_ptr handles around, never the Node objects
 * they point to), but the Node storage itself must never move underneath an
 * already-handed-out Node* -- every internal Node holds raw Node* children,
 * and Root()/Node::Child() hand out raw Node* to callers that hold on to
 * them across an entire Barnes-Hut traversal. A single std::vector<Node>
 * arena would reallocate (and invalidate every live Node*) the moment a
 * build needed more nodes than the vector's current capacity; a vector of
 * individually-allocated fixed-size blocks never does, because growing the
 * outer index never touches a previously-allocated block's memory.
 *
 * A leaf that overflows its per-cell point capacity (only possible for
 * coincident/inseparable points once halfSize has hit the minimum
 * subdivision threshold) gets an individually growable overflow buffer,
 * freed and reallocated fresh on every Rebuild -- unlike the steady-state
 * arena, which is deliberately never freed early.
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
   * QuadTree::Root()/Child(). Every accessor here is unchecked/noexcept and
   * trivially inlinable on purpose: this is walked in a tight Barnes-Hut
   * traversal loop, and none of the old C free-function calls it replaces
   * did any bounds checking either.
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
   * the default of 1. Allocation failure throws std::bad_alloc, same as any
   * std::vector/new -- there is no bespoke error type to check.
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

  /** Number of arena node blocks allocated so far. Diagnostic: exposes the
   *  amortized arena-growth contract described in the class comment so
   *  tests can verify a Rebuild() with no larger a tree than before
   *  allocates no new block, the same way Subgraph::VertexCapacity()
   *  exposes its own amortized-growth contract. */
  size_t NodeBlockCount() const noexcept { return nodeBlocks_.size(); }

  /** Number of overflow point buffers currently held (freed and rebuilt
   *  every Rebuild(), see the class comment). Diagnostic, same rationale as
   *  NodeBlockCount(). */
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

  /** Owned; Node[] arena blocks. See the class comment for why this must be
   *  a vector of individually-allocated blocks, not a flat std::vector<Node>. */
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
