#ifndef _GVIZ_TREE_H_
#define _GVIZ_TREE_H_

#include "gvizGraph.h"

int gvizGraphIsTree(const gvizGraph *graph, int *parents);

int gvizTreeIsLeaf(const gvizGraph *tree, size_t index);
size_t gvizTreeCountLeaves(const gvizGraph *tree, size_t root);

#endif
