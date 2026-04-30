#ifndef _GVIZ_GRAPH_VIEW_H_
#define _GVIZ_GRAPH_VIEW_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include <stddef.h>

/**
 * @brief A non-materializing view over a `gvizGraph`.
 *
 * A view borrows its graph (it does not own it) and selects a subset of
 * vertices and/or edges via bit masks. There are four canonical mask modes:
 *   - vertexMask = NULL, edgeMask = NULL     -> the whole graph (Full view).
 *   - vertexMask set,    edgeMask = NULL     -> induced subgraph on those
 *                                                vertices; an edge (u, v) is
 *                                                in the view iff both
 *                                                endpoints are.
 *   - vertexMask = NULL, edgeMask set        -> edge-defined subgraph; every
 *                                                vertex of the underlying
 *                                                graph is in the view.
 *   - both set                               -> intersection: vertex must be
 *                                                in vertexMask AND each edge
 *                                                must have its bit set.
 *
 * `edgeStart` is a per-vertex prefix sum into `edgeMask`. `edgeStart[u]` is
 * the offset where u's outgoing-edge bits begin; `edgeStart[N]` equals the
 * total number of directed edge slots (sum of neighbor list lengths). It is
 * computed against the underlying graph's adjacency lists and is therefore
 * stable for the view's lifetime as long as the graph topology does not
 * change. To check whether edge (u, v) is in the view, locate v's index in
 * u's neighbor list and test bit `edgeStart[u] + neighborIndex`.
 *
 * Callers must keep the underlying `gvizGraph *` alive while any view
 * references it. Releasing the view never touches the graph.
 */
typedef struct gvizGraphView {
  gvizGraph *graph;          /* borrowed; never freed by the view */
  GVIZ_BIT_ARRAY vertexMask; /* NULL = all vertices, length = graph N bits */
  GVIZ_BIT_ARRAY edgeMask;   /* NULL = all edges, length = totalEdgeSlots bits */
  size_t *edgeStart;         /* length N+1; NULL until first edge query */
  size_t count;              /* number of vertices in the view */
  size_t edgeStartStale;     /* nonzero when edgeStart needs rebuild */
} gvizGraphView;

/**
 * Initialize a view that selects the entire graph (no masks). `count` is
 * set to the graph's vertex count. Both masks and `edgeStart` start NULL.
 */
int gvizGraphViewInitFull(gvizGraphView *view, gvizGraph *graph);

/**
 * Initialize a view from a vertex bit mask. `mask` is cloned (the view does
 * not borrow caller-supplied storage). `count` is computed from the mask.
 * `edgeMask` and `edgeStart` are left lazy.
 *
 * Passing `mask == NULL` is equivalent to `gvizGraphViewInitFull`.
 */
int gvizGraphViewInitFromVertices(gvizGraphView *view, gvizGraph *graph,
                                  const GVIZ_BIT_ARRAY mask);

/**
 * Initialize a view from an edge bit mask. The mask is cloned. `edgeStart`
 * is built immediately and the vertex space is left as the full graph
 * (`vertexMask == NULL`). Length of the edge mask is `totalEdgeSlots` bits,
 * where `totalEdgeSlots` is the sum of `|neighbors(u)|` over all u.
 */
int gvizGraphViewInitFromEdges(gvizGraphView *view, gvizGraph *graph,
                               const GVIZ_BIT_ARRAY edgeMask);

/**
 * Release any allocations owned by the view: masks, edgeStart array. Does
 * not touch the underlying graph.
 */
void gvizGraphViewRelease(gvizGraphView *view);

/**
 * Total number of directed edge slots over the underlying graph (sum of
 * neighbor list lengths). Useful for sizing edgeMask and tests.
 */
size_t gvizGraphViewTotalEdgeSlots(const gvizGraphView *view);

/**
 * Compute or recompute `edgeStart` against the underlying graph topology.
 * Allocates if missing. Safe to call repeatedly; `edgeStartStale` is
 * cleared on success.
 */
int gvizGraphViewRebuildEdgeStart(gvizGraphView *view);

// READ INTERFACE: -------------------------------------------------------------

/**
 * Number of vertices currently in the view (equals `view->count`).
 */
size_t gvizGraphViewVertexCount(const gvizGraphView *view);

/**
 * Returns 1 if vertex `u` is in the view, 0 if not, -1 on bad input.
 * Out-of-range vertices report not-in-view.
 */
int gvizGraphViewVertexInView(const gvizGraphView *view, size_t u);

/**
 * Returns 1 if directed edge (u, v) is in the view, 0 if not, -1 on bad
 * input. Both endpoints must satisfy the vertex mask (when set), and the
 * edge bit must be set when `edgeMask` is non-NULL. Lazily builds
 * `edgeStart` if needed.
 */
int gvizGraphViewEdgeInView(gvizGraphView *view, size_t u, size_t v);

/**
 * Number of neighbors of `u` that are currently in the view.
 */
size_t gvizGraphViewDegree(gvizGraphView *view, size_t u);

/**
 * Walks the vertex indices that are currently in the view, in ascending
 * order. Pair init/release to free internal state.
 */
typedef struct gvizGraphViewVertexIter {
  const gvizGraphView *view;
  gvizBitArrayIter bitIter; /* used when vertexMask != NULL */
  size_t cursor;            /* used when vertexMask == NULL */
  int hasMask;
} gvizGraphViewVertexIter;

void gvizGraphViewVertexIterInit(gvizGraphViewVertexIter *iter,
                                 const gvizGraphView *view);
int gvizGraphViewVertexIterNext(gvizGraphViewVertexIter *iter, size_t *out);
void gvizGraphViewVertexIterRelease(gvizGraphViewVertexIter *iter);

/**
 * Walks the neighbors of vertex `u` that are currently in the view (i.e.
 * pass the vertex mask, and whose corresponding directed-edge bit is set
 * when `edgeMask` is present). Built on top of the underlying graph's
 * adjacency list — never materializes a new array. Lazily builds
 * `edgeStart` on init when an edgeMask is in play.
 */
typedef struct gvizGraphViewNeighborsIter {
  gvizGraphView *view;
  size_t u;
  size_t i;          /* cursor into the underlying neighbor array */
  size_t edgeBase;   /* edgeStart[u]; valid only when edgeMask != NULL */
  size_t neighborN;  /* cached neighbor count */
} gvizGraphViewNeighborsIter;

int gvizGraphViewNeighborsIterInit(gvizGraphViewNeighborsIter *iter,
                                   gvizGraphView *view, size_t u);
int gvizGraphViewNeighborsIterNext(gvizGraphViewNeighborsIter *iter,
                                   size_t *out);
void gvizGraphViewNeighborsIterRelease(gvizGraphViewNeighborsIter *iter);

// MUTATION INTERFACE: ---------------------------------------------------------

/**
 * Add `u` to the vertex set of the view. Allocates `vertexMask` lazily; if
 * the view was Full (mask NULL), the mask is materialized as all-ones first.
 * Marks `edgeStart` stale so the next edge query rebuilds it.
 */
int gvizGraphViewAddVertex(gvizGraphView *view, size_t u);

/**
 * Remove `u` from the vertex set of the view. Allocates `vertexMask`
 * lazily.
 */
int gvizGraphViewRemoveVertex(gvizGraphView *view, size_t u);

/**
 * Add the directed edge (u, v) to the view, allocating `edgeMask` lazily
 * (initialized empty so we can flip individual bits to enable specific
 * edges). Returns -1 if the edge does not exist in the underlying graph.
 */
int gvizGraphViewAddEdge(gvizGraphView *view, size_t u, size_t v);

/**
 * Remove the directed edge (u, v) from the view, allocating `edgeMask`
 * lazily as fully-set so we can clear specific bits.
 */
int gvizGraphViewRemoveEdge(gvizGraphView *view, size_t u, size_t v);

#endif
