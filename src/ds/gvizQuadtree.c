#include "ds/gvizQuadtree.h"
#include "core/alloc.h"
#include <stdlib.h>
#include <string.h>

#define GVIZ_QUADTREE_MIN_HALF_SIZE 1e-9
#define GVIZ_QUADTREE_ARENA_BLOCK_NODES 1024

static double gvizQuadtreeMax(double a, double b) { return a > b ? a : b; }

static gvizQuadtreeNode *gvizQuadtreeArenaAlloc(gvizQuadtree *tree, double cx,
                                                double cy, double halfSize) {
  size_t blockIdx = tree->nodesUsed / GVIZ_QUADTREE_ARENA_BLOCK_NODES;
  size_t offset = tree->nodesUsed % GVIZ_QUADTREE_ARENA_BLOCK_NODES;

  if (blockIdx == tree->nodeBlocks.count) {
    gvizQuadtreeNode *nodeBlock =
        GVIZ_ALLOC(sizeof(gvizQuadtreeNode) * GVIZ_QUADTREE_ARENA_BLOCK_NODES);
    if (!nodeBlock)
      return NULL;

    size_t *pointBlock =
        tree->nodesPerCell > 0
            ? GVIZ_ALLOC(sizeof(size_t) * GVIZ_QUADTREE_ARENA_BLOCK_NODES *
                        tree->nodesPerCell)
            : NULL;
    if (tree->nodesPerCell > 0 && !pointBlock) {
      GVIZ_DEALLOC(nodeBlock);
      return NULL;
    }

    if (gvizArrayPush(&tree->nodeBlocks, &nodeBlock) != 0) {
      GVIZ_DEALLOC(nodeBlock);
      GVIZ_DEALLOC(pointBlock);
      return NULL;
    }
    if (gvizArrayPush(&tree->pointBlocks, &pointBlock) != 0) {
      GVIZ_DEALLOC(pointBlock);
      return NULL;
    }
  }

  gvizQuadtreeNode *nodeBlock =
      *(gvizQuadtreeNode **)gvizArrayAtIndex(&tree->nodeBlocks, blockIdx);
  size_t *pointBlock = *(size_t **)gvizArrayAtIndex(&tree->pointBlocks, blockIdx);

  gvizQuadtreeNode *node = &nodeBlock[offset];
  node->cx = cx;
  node->cy = cy;
  node->halfSize = halfSize;
  node->comX = 0.0;
  node->comY = 0.0;
  node->mass = 0;
  node->pointIndices =
      tree->nodesPerCell > 0 ? &pointBlock[offset * tree->nodesPerCell] : NULL;
  node->pointCount = 0;
  node->pointCapacity = tree->nodesPerCell;
  memset(node->children, 0, sizeof(node->children));

  tree->nodesUsed++;
  return node;
}

static gvizQuadrant gvizQuadtreeQuadrantFor(const gvizQuadtreeNode *node,
                                            double px, double py) {
  int east = px >= node->cx;
  int north = py >= node->cy;

  if (north)
    return east ? GVIZ_QUAD_NE : GVIZ_QUAD_NW;
  return east ? GVIZ_QUAD_SE : GVIZ_QUAD_SW;
}

static int gvizQuadtreeSubdivide(gvizQuadtree *tree, gvizQuadtreeNode *node) {
  double quarter = node->halfSize / 2.0;
  double offsets[GVIZ_QUAD_COUNT][2] = {
      {-quarter, quarter},
      {quarter, quarter},
      {-quarter, -quarter},
      {quarter, -quarter},
  };

  for (int i = 0; i < GVIZ_QUAD_COUNT; i++) {
    node->children[i] = gvizQuadtreeArenaAlloc(
        tree, node->cx + offsets[i][0], node->cy + offsets[i][1], quarter);
    if (!node->children[i])
      return -1;
  }

  return 0;
}

static int gvizQuadtreeGrowOverflow(gvizQuadtree *tree, gvizQuadtreeNode *node) {
  size_t newCapacity = node->pointCapacity > 0 ? node->pointCapacity * 2 : 1;
  size_t *grown = GVIZ_ALLOC(sizeof(size_t) * newCapacity);
  if (!grown)
    return -1;

  memcpy(grown, node->pointIndices, sizeof(size_t) * node->pointCount);

  if (gvizArrayPush(&tree->overflowBuffers, &grown) != 0) {
    GVIZ_DEALLOC(grown);
    return -1;
  }

  node->pointIndices = grown;
  node->pointCapacity = newCapacity;
  return 0;
}

static int gvizQuadtreeInsertPoint(gvizQuadtreeNode *node, gvizQuadtree *tree,
                                   size_t idx) {
  double px = tree->points[2 * idx];
  double py = tree->points[2 * idx + 1];

  size_t newMass = node->mass + 1;
  node->comX = (node->comX * (double)node->mass + px) / (double)newMass;
  node->comY = (node->comY * (double)node->mass + py) / (double)newMass;
  node->mass = newMass;

  if (!gvizQuadtreeNodeIsLeaf(node))
    return gvizQuadtreeInsertPoint(
        node->children[gvizQuadtreeQuadrantFor(node, px, py)], tree, idx);

  if (node->pointCount < node->pointCapacity) {
    node->pointIndices[node->pointCount++] = idx;
    return 0;
  }

  if (node->halfSize <= GVIZ_QUADTREE_MIN_HALF_SIZE) {
    if (gvizQuadtreeGrowOverflow(tree, node) != 0)
      return -1;
    node->pointIndices[node->pointCount++] = idx;
    return 0;
  }

  if (gvizQuadtreeSubdivide(tree, node) != 0)
    return -1;

  for (size_t i = 0; i < node->pointCount; i++) {
    size_t heldIdx = node->pointIndices[i];
    double hx = tree->points[2 * heldIdx];
    double hy = tree->points[2 * heldIdx + 1];
    gvizQuadtreeNode *child =
        node->children[gvizQuadtreeQuadrantFor(node, hx, hy)];
    if (gvizQuadtreeInsertPoint(child, tree, heldIdx) != 0)
      return -1;
  }
  node->pointCount = 0;

  return gvizQuadtreeInsertPoint(
      node->children[gvizQuadtreeQuadrantFor(node, px, py)], tree, idx);
}

static int gvizQuadtreeBuild(gvizQuadtree *tree) {
  tree->root = NULL;

  if (tree->pointCount == 0)
    return 0;

  double minX = tree->points[0], maxX = tree->points[0];
  double minY = tree->points[1], maxY = tree->points[1];
  for (size_t i = 1; i < tree->pointCount; i++) {
    double x = tree->points[2 * i], y = tree->points[2 * i + 1];
    minX = x < minX ? x : minX;
    maxX = x > maxX ? x : maxX;
    minY = y < minY ? y : minY;
    maxY = y > maxY ? y : maxY;
  }

  double span = gvizQuadtreeMax(maxX - minX, maxY - minY);
  double halfSize = span > 0.0 ? span / 2.0 : 1.0;
  double cx = (minX + maxX) / 2.0;
  double cy = (minY + maxY) / 2.0;

  tree->root = gvizQuadtreeArenaAlloc(tree, cx, cy, halfSize);
  if (!tree->root)
    return -1;

  for (size_t i = 0; i < tree->pointCount; i++) {
    if (gvizQuadtreeInsertPoint(tree->root, tree, i) != 0) {
      tree->root = NULL;
      return -1;
    }
  }

  return 0;
}

static void gvizQuadtreeFreeOverflowBuffers(gvizQuadtree *tree) {
  for (size_t i = 0; i < tree->overflowBuffers.count; i++)
    GVIZ_DEALLOC(*(size_t **)gvizArrayAtIndex(&tree->overflowBuffers, i));
  tree->overflowBuffers.count = 0;
}

int gvizQuadtreeInit(gvizQuadtree *tree, const double *points, size_t count,
                     size_t nodesPerCell) {
  if (!tree)
    return -1;

  tree->points = points;
  tree->pointCount = count;
  tree->nodesPerCell = nodesPerCell;
  tree->root = NULL;
  tree->nodesUsed = 0;

  if (gvizArrayInit(&tree->nodeBlocks, sizeof(gvizQuadtreeNode *)) != 0)
    return -1;
  if (gvizArrayInit(&tree->pointBlocks, sizeof(size_t *)) != 0) {
    gvizArrayRelease(&tree->nodeBlocks);
    return -1;
  }
  if (gvizArrayInit(&tree->overflowBuffers, sizeof(size_t *)) != 0) {
    gvizArrayRelease(&tree->nodeBlocks);
    gvizArrayRelease(&tree->pointBlocks);
    return -1;
  }

  return gvizQuadtreeBuild(tree);
}

int gvizQuadtreeRebuild(gvizQuadtree *tree, const double *points,
                        size_t count) {
  if (!tree)
    return -1;

  gvizQuadtreeFreeOverflowBuffers(tree);
  tree->nodesUsed = 0;
  tree->points = points;
  tree->pointCount = count;

  return gvizQuadtreeBuild(tree);
}

void gvizQuadtreeRelease(gvizQuadtree *tree) {
  if (!tree)
    return;

  for (size_t i = 0; i < tree->nodeBlocks.count; i++)
    GVIZ_DEALLOC(*(gvizQuadtreeNode **)gvizArrayAtIndex(&tree->nodeBlocks, i));
  for (size_t i = 0; i < tree->pointBlocks.count; i++) {
    size_t *pointBlock = *(size_t **)gvizArrayAtIndex(&tree->pointBlocks, i);
    if (pointBlock)
      GVIZ_DEALLOC(pointBlock);
  }
  gvizQuadtreeFreeOverflowBuffers(tree);

  gvizArrayRelease(&tree->nodeBlocks);
  gvizArrayRelease(&tree->pointBlocks);
  gvizArrayRelease(&tree->overflowBuffers);

  tree->root = NULL;
  tree->nodesUsed = 0;
}

const gvizQuadtreeNode *gvizQuadtreeRoot(const gvizQuadtree *tree) {
  return tree->root;
}

int gvizQuadtreeNodeIsLeaf(const gvizQuadtreeNode *node) {
  return node->children[0] == NULL;
}

const gvizQuadtreeNode *gvizQuadtreeNodeChild(const gvizQuadtreeNode *node,
                                              gvizQuadrant quadrant) {
  return node->children[quadrant];
}

size_t gvizQuadtreeNodePointCount(const gvizQuadtreeNode *node) {
  return gvizQuadtreeNodeIsLeaf(node) ? node->pointCount : 0;
}

size_t gvizQuadtreeNodePointAt(const gvizQuadtreeNode *node, size_t i) {
  return node->pointIndices[i];
}

void gvizQuadtreeNodeCenterOfMass(const gvizQuadtreeNode *node, double *outX,
                                  double *outY) {
  if (outX)
    *outX = node->comX;
  if (outY)
    *outY = node->comY;
}
