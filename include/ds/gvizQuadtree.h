#ifndef _GVIZ_QUADTREE_H_
#define _GVIZ_QUADTREE_H_

#include "ds/gvizArray.h"
#include <stddef.h>

/** Default leaf capacity; see gvizQuadtreeInit. */
#define GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT 1

/** A child's position within its parent's square region, y-axis up. */
typedef enum gvizQuadrant {
  GVIZ_QUAD_NW = 0,
  GVIZ_QUAD_NE = 1,
  GVIZ_QUAD_SW = 2,
  GVIZ_QUAD_SE = 3,
  GVIZ_QUAD_COUNT = 4,
} gvizQuadrant;

/**
 * One node of a gvizQuadtree: a square region of 2D space. A leaf (every
 * entry of @c children is NULL) holds up to @c pointCapacity point indices
 * directly in @c pointIndices; once a leaf would overflow it subdivides into
 * four equal quadrants and redistributes its points among them. Every node,
 * leaf or internal, tracks the running center of mass and total mass (the
 * sum of each point's caller-supplied mass, see gvizQuadtree::masses) of its
 * entire subtree, so future Barnes-Hut force traversal can approximate a
 * whole subtree by its centroid instead of visiting every point.
 *
 * Nodes are owned by the tree's arena (see gvizQuadtree), not individually
 * allocated or freed.
 */
typedef struct gvizQuadtreeNode {
  double cx, cy;     /**< Center of this node's square region. */
  double halfSize;   /**< Half the side length of the square region. */
  double comX, comY; /**< Center of mass of every point in this subtree. */
  double mass;       /**< Sum of the caller-supplied masses of every point in
                      *   this subtree (see gvizQuadtree::masses). */
  size_t *pointIndices; /**< Indices into the source point buffer. Only
                        *   populated, and only meaningful, on leaves. */
  size_t pointCount;    /**< Valid entries in pointIndices. */
  size_t pointCapacity; /**< Capacity of pointIndices; normally equal to the
                        *   tree's nodesPerCell, larger only for a leaf that
                        *   outgrew it (see gvizQuadtreeInit). */
  struct gvizQuadtreeNode *children[GVIZ_QUAD_COUNT]; /**< NULL on leaves. */
} gvizQuadtreeNode;

/**
 * A quadtree indexing a fixed buffer of 2D points. Purely spatial: it has no
 * knowledge of graphs, vertices, or edges, and takes raw coordinates only.
 *
 * Nodes and their point storage are carved out of block arenas that persist
 * across gvizQuadtreeRebuild calls instead of being freed and reallocated
 * every time — the intended usage is one gvizQuadtreeInit followed by many
 * gvizQuadtreeRebuild calls (e.g. once per layout iteration), then a single
 * gvizQuadtreeRelease at the end. A handful of leaves that end up with more
 * points than nodesPerCell (because they can't be spatially separated any
 * further, e.g. coincident points) get individually heap-allocated overflow
 * buffers instead; those are the only allocations freed and redone every
 * rebuild.
 */
typedef struct gvizQuadtree {
  gvizQuadtreeNode *root; /**< NULL when the tree indexes zero points. */
  size_t nodesPerCell;    /**< Max points a leaf holds before it subdivides. */
  const double *points;   /**< Borrowed; interleaved x0, y0, x1, y1, ... */
  const double *masses;   /**< Borrowed; one entry per point (not
                           *   interleaved), same indexing as point index. */
  size_t pointCount;

  gvizArray nodeBlocks;      /**< Owned; gvizQuadtreeNode* arena blocks. */
  gvizArray pointBlocks;     /**< Owned; size_t* point-storage blocks, one
                             *   per entry of nodeBlocks. */
  size_t nodesUsed;          /**< Nodes handed out in the current build. */
  gvizArray overflowBuffers; /**< Owned; size_t* buffers backing leaves that
                             *   outgrew their block-reserved capacity. */
} gvizQuadtree;

/**
 * Builds a quadtree over @p points, an interleaved x0, y0, x1, y1, ... buffer
 * describing @p count 2D points, and allocates its node arena. @p points is
 * borrowed and must outlive @p tree; @p tree takes no ownership of it.
 * @p masses is a borrowed array of @p count doubles, one entry per point
 * (not interleaved, same indexing as point index), giving each point's mass;
 * it must also outlive @p tree. Every node's mass (see gvizQuadtreeNode) is
 * the sum of the masses of the points in its subtree.
 * @p nodesPerCell is the maximum number of points a leaf holds before it
 * subdivides; pass GVIZ_QUADTREE_NODES_PER_CELL_DEFAULT for the default of 1.
 *
 * @return 0 on success, -1 on allocation failure. @p tree is left in a
 * releasable state either way.
 */
int gvizQuadtreeInit(gvizQuadtree *tree, const double *points,
                     const double *masses, size_t count, size_t nodesPerCell);

/**
 * Rebuilds @p tree over @p points (a new buffer, or the same one with updated
 * coordinates), @p masses (parallel to @p points, one entry per point), and
 * @p count, reusing the arena allocated by gvizQuadtreeInit instead of
 * freeing and reallocating it. Cheap once the arena has grown to cover the
 * largest build seen so far: no allocation occurs unless this build needs
 * more nodes than any previous one. @p tree must already be initialized with
 * gvizQuadtreeInit.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizQuadtreeRebuild(gvizQuadtree *tree, const double *points,
                        const double *masses, size_t count);

/** Releases the arena and every buffer owned by @p tree. Does not free
 *  @p tree->points. */
void gvizQuadtreeRelease(gvizQuadtree *tree);

/** Returns the root node, or NULL if @p tree indexes zero points. */
const gvizQuadtreeNode *gvizQuadtreeRoot(const gvizQuadtree *tree);

/** Returns whether @p node is a leaf, i.e. holds points directly. */
int gvizQuadtreeNodeIsLeaf(const gvizQuadtreeNode *node);

/** Returns the @p quadrant child of @p node, or NULL when @p node is a leaf. */
const gvizQuadtreeNode *gvizQuadtreeNodeChild(const gvizQuadtreeNode *node,
                                              gvizQuadrant quadrant);

/**
 * Returns the number of points held directly by @p node.
 * 0 for internal nodes; use gvizQuadtreeNodeCenterOfMass for their aggregate.
 */
size_t gvizQuadtreeNodePointCount(const gvizQuadtreeNode *node);

/**
 * Returns the source-buffer index of the @p i-th point held directly by leaf
 * @p node. Does not check bounds; @p i must be < gvizQuadtreeNodePointCount(node).
 */
size_t gvizQuadtreeNodePointAt(const gvizQuadtreeNode *node, size_t i);

/**
 * Writes the center of mass of every point in @p node's subtree to @p outX
 * and @p outY. Either may be NULL to ignore that coordinate.
 */
void gvizQuadtreeNodeCenterOfMass(const gvizQuadtreeNode *node, double *outX,
                                  double *outY);

#endif
