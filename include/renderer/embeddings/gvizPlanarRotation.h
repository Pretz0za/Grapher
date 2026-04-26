#ifndef _GVIZ_PLANAR_ROTATION_H_
#define _GVIZ_PLANAR_ROTATION_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"

/*
 * Sort each vertex's adjacency list in @p eg->graph in counter-clockwise
 * order around the vertex, using the geometric atan2 of each neighbor's
 * 2D position relative to the vertex. The first two embedding dimensions
 * are interpreted as (x, y).
 *
 * Modifies the graph's neighbor arrays in place. Safe to call repeatedly.
 *
 * Implementation note: uses a file-static context pointer for the qsort
 * comparator since the project does not use qsort_r.
 */
void gvizComputeRotationSystem(gvizEmbeddedGraph *eg);

#endif
