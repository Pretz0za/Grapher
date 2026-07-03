#ifndef _GVIZ_EMBEDDED_TREE_H_
#define _GVIZ_EMBEDDED_TREE_H_

#include "core/alloc.h"
#include "dsa/gvizBitArray.h"
#include "embedders/gvizEmbeddedGraph.h"

// Per vertex decorators for Reingold-Tilford
typedef struct gvizRTDecorators {
  float *offsets; /**< Array of float offset of each child. */
  size_t lMost;   /**< The left-most Vertex in the deepest layer of the subtree
                       rooted at this Vertex. */
  size_t rMost;   /**< The right-most Vertex in the deepest layer of the subtree
                       rooted at this Vertex. */
  size_t depth;   /**< The depth of this Vertex in the whole tree. */
  int ancestor; /**< The root of the subtree (if known) this vertex belongs to.
                 */
} gvizRTDecorators;

// Embedding algorithm state for Reingold-Tilford tidy tree layouts.
typedef struct gvizEmbeddedTree {
  gvizEmbeddedGraph graph; /**< Base graph embedded we want to create. */
  gvizRTDecorators *dec;   /**< The data of each Vertex. This is populated
                                   by the algorithm. */
  float *offsetsOrigin;    /**< Points the a big memory block that stores all
                                decorator offset arrays contiguously. */
  int *parents;
  GVIZ_BIT_ARRAY thread; /**< Boolean array tracking vertices were threaded. */
  size_t height;         /**< The calculated height of the whole tree. */
  size_t defaultAncestor;
  size_t offsetsOffset; /**< The current memory offset from offsetsOrigin. Used
                           to track allocation of offsetsOrigin. */

} gvizEmbeddedTree;

/**
 * Initializes Reingold-Tilford state for the directed tree @p graph rooted at @p root.
 * @p state is zeroed first so gvizEmbeddedTreeRTRelease is safe after a failed init.
 *
 * @return 0 on success, -1 on allocation failure or if @p graph is not a tree.
 */
int gvizEmbeddedTreeRTInit(gvizEmbeddedTree *state, gvizGraph *graph,
                           size_t root);

/**
 * Computes horizontal offsets for the subtree rooted at @p root at @p level.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizEmbeddedTreeCalculateOffsets(gvizEmbeddedTree *state, size_t root,
                                     size_t level);

/** Releases all memory owned by @p state. */
void gvizEmbeddedTreeRTRelease(gvizEmbeddedTree *state);

/**
 * Recursively assigns 2D positions starting from @p root at @p position.
 * Call gvizEmbeddedTreeCalculateOffsets first.
 *
 * @return 0 on success.
 */
int gvizEmbeddedTreeEmbed(gvizEmbeddedTree *state, size_t root,
                          double *position);

#endif
