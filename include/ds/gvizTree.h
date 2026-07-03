#ifndef _GVIZ_TREE_H_
#define _GVIZ_TREE_H_

#include "gvizGraph.h"

/**
 * Tests whether @p graph is a directed tree rooted at a single vertex.
 * When @p parents is non-NULL, fills parent indices (-1 for the root).
 *
 * @return 1 if a tree, 0 if not, -1 if invalid (e.g. multiple roots),
 *         -2 if @p graph is undirected.
 */
int gvizGraphIsTree(const gvizGraph *graph, int *parents);

/** Returns 1 if @p index is a leaf in the directed tree @p tree, 0 otherwise. */
int gvizTreeIsLeaf(const gvizGraph *tree, size_t index);

/** Returns the number of leaves in the subtree rooted at @p root. */
size_t gvizTreeCountLeaves(const gvizGraph *tree, size_t root);

#endif
