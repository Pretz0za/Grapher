#ifndef _GVIZ_APP_DEMO_GRAPH_LOAD_H_
#define _GVIZ_APP_DEMO_GRAPH_LOAD_H_

#include "app/gvizLayerCreatePanel.h"
#include "dsa/gvizGraph.h"

/*
 * Build a demo graph (per-type) into @p out using the panel params'
 * graphType / graphParam1 / graphParam2.
 *
 * @p outerFace + @p outerFaceLen receive a candidate outer face for
 * Sierpinski / OBJ-style sources (caller may pass NULL/NULL).
 * @p outRoot receives a tree-root vertex for the random-tree generator
 * (caller may pass NULL).
 *
 * Returns 0 on success.
 */
int gvizLoadDemoGraph(const gvizLayerCreateParams *p, gvizGraph *out,
                      size_t *outerFace, size_t *outerFaceLen,
                      size_t *outRoot);

/*
 * Promote a stack gvizGraph to heap (transfers contents). On allocation
 * failure releases @p src and returns NULL.
 */
gvizGraph *gvizGraphToHeap(gvizGraph *src);

#endif
