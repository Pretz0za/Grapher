#ifndef __GVIZ_SUBGRAPH_H__
#define __GVIZ_SUBGRAPH_H__

#include "dsa/gvizBitArray.h"
#include <stdbool.h>

struct gvizGraph;

/**
 * Shared edge-bitset layout for a graph. Populated by gvizGraphBuildLayout;
 * callers must rebuild after structural edge changes before using edge subsets
 * or subgraphs that depend on it.
 */
typedef struct {
	size_t *vertexOffsets;
	size_t nvertices;
	size_t edgeCount;
} gvizGraphLayout;

typedef GVIZ_BIT_ARRAY gvizVertexSubset;
typedef struct {
	GVIZ_BIT_ARRAY bitset;
	const gvizGraphLayout *layout;
} gvizEdgeSubset;

typedef struct {
	const struct gvizGraph *g;
	gvizVertexSubset vs;
	gvizEdgeSubset es;
} gvizSubgraph;

typedef struct {
	const gvizSubgraph *sg;
	gvizBitArrayIterator it;
} gvizSubgraphVertexIterator;

typedef struct {
	const gvizSubgraph *sg;
	size_t u;
	size_t base;
	gvizBitArrayIterator it;
} gvizSubgraphNeighborIterator;

/** Allocates an empty vertex subset sized for @p g. */
gvizVertexSubset gvizVertexSubsetCreateEmpty(const struct gvizGraph *g);

/** Marks vertex @p u as present in @p vs. */
void gvizVertexSubsetShowVertex(const gvizVertexSubset vs, size_t u);

/** Marks vertex @p u as absent in @p vs. */
void gvizVertexSubsetHideVertex(const gvizVertexSubset vs, size_t u);

/** Frees @p vs. */
void gvizVertexSubsetRelease(const gvizVertexSubset vs);

/** Returns whether vertex @p u is present in @p vs. */
bool gvizVertexSubsetTest(const gvizVertexSubset vs, size_t u);

/** Returns the number of set bits in the first @p nvertices indices of @p vs. */
size_t gvizVertexSubsetCount(const gvizVertexSubset vs, size_t nvertices);

/** Creates an iterator over vertices present in @p vs. */
gvizBitArrayIterator gvizVertexSubsetIteratorCreate(const gvizVertexSubset vs,
                                                    size_t nvertices);

/**
 * Allocates an empty edge subset sized from @p g->layout.
 * Requires a current layout from gvizGraphBuildLayout.
 */
gvizEdgeSubset gvizEdgeSubsetCreateEmpty(const struct gvizGraph *g);

/**
 * Marks the @p idx-th neighbor of @p u (adjacency-list index, not vertex id)
 * as present in @p es.
 */
void gvizEdgeSubsetShowEdge(const gvizEdgeSubset es, size_t u, size_t idx);

/** Clears the @p idx-th neighbor edge of @p u in @p es. */
void gvizEdgeSubsetHideEdge(const gvizEdgeSubset es, size_t u, size_t idx);

/** Frees the edge bitset in @p es. */
void gvizEdgeSubsetRelease(const gvizEdgeSubset es);

/** Returns the total number of edges marked in @p es. */
size_t gvizEdgeSubsetCount(const gvizEdgeSubset es);

/** Returns the number of marked edges incident to @p u in @p es. */
size_t gvizEdgeSubsetVertexEdgeCount(const gvizEdgeSubset es, size_t u);

/** Returns whether the @p idx-th neighbor edge of @p u is marked in @p es. */
bool gvizEdgeSubsetTestEdge(const gvizEdgeSubset es, size_t u, size_t idx);

/** Clears all marked edges incident to @p u in @p es. */
void gvizEdgeSubsetClearVertex(const gvizEdgeSubset es, size_t u);

/** Creates an iterator over all marked edges in @p es. */
gvizBitArrayIterator gvizEdgeSubsetIteratorCreate(const gvizEdgeSubset es);

/** Creates an iterator over marked edges incident to @p u in @p es. */
gvizBitArrayIterator gvizEdgeSubsetIteratorCreateVertexRange(
    const gvizEdgeSubset es, size_t u);

/**
 * Creates a vertex-induced subgraph view: @p vs is owned, edge subset is NULL.
 * Call gvizSubgraphMakeEdgeSubset to populate edges.
 */
gvizSubgraph gvizSubgraphCreateVertexInduced(const struct gvizGraph *g, gvizVertexSubset v);

/**
 * Creates an edge-induced subgraph view: @p es is owned, vertex subset is NULL.
 * Call gvizSubgraphMakeVertexSubset to populate vertices.
 */
gvizSubgraph gvizSubgraphCreateEdgeInduced(const struct gvizGraph *g, gvizEdgeSubset es);

/**
 * Creates a subgraph with empty vertex and edge subsets.
 * Requires a current layout from gvizGraphBuildLayout.
 */
gvizSubgraph gvizSubgraphCreateEmpty(const struct gvizGraph *g);

/** Releases both subsets owned by @p sg and clears @p sg. */
void gvizSubgraphRelease(gvizSubgraph *sg);

/**
 * Allocates and fills the edge subset for a vertex-induced subgraph.
 * Requires a current layout; no-op if @p sg already has an edge subset.
 */
void gvizSubgraphMakeEdgeSubset(gvizSubgraph *sg);

/**
 * Allocates and fills the vertex subset for an edge-induced subgraph.
 * No-op if @p sg already has a vertex subset.
 */
void gvizSubgraphMakeVertexSubset(gvizSubgraph *sg);

/** Marks vertex @p u present; also clears its incident edges if an edge subset exists. */
void gvizSubgraphShowVertex(const gvizSubgraph *sg, size_t u);

/** Marks vertex @p u absent and clears its incident edges in the edge subset. */
void gvizSubgraphHideVertex(const gvizSubgraph *sg, size_t u);

/** Marks edge (@p u, @p v) present; no-op if the edge is not in the parent graph. */
void gvizSubgraphShowEdge(const gvizSubgraph *sg, size_t u, size_t v);

/** Marks edge (@p u, @p v) absent; no-op if the edge is not in the parent graph. */
void gvizSubgraphHideEdge(const gvizSubgraph *sg, size_t u, size_t v);

/**
 * Rebuilds subsets after the parent graph structure changed.
 * Recomputes the layout, resizes the vertex subset, and remaps edge bits.
 * Preserves vertex-induced (no edge subset) or edge-induced (no vertex subset) mode.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizSubgraphRebuild(gvizSubgraph *sg);

/** Returns whether @p u is in the subgraph. Requires both subsets to be present. */
bool gvizSubgraphHasVertex(const gvizSubgraph *sg, size_t u);

/** Returns the vertex count. Requires both subsets to be present. */
size_t gvizSubgraphVertexCount(const gvizSubgraph *sg);

/** Returns whether edge (@p u, @p v) is in the subgraph. Requires both subsets. */
bool gvizSubgraphHasEdge(const gvizSubgraph *sg, size_t u, size_t v);

/** Returns the degree of @p u within the subgraph. Requires both subsets. */
size_t gvizSubgraphDegree(const gvizSubgraph *sg, size_t u);

/** Returns the edge count within the subgraph. Requires both subsets. */
size_t gvizSubgraphEdgeCount(const gvizSubgraph *sg);

/** Creates an iterator over vertices in @p sg. Requires both subsets. */
gvizSubgraphVertexIterator gvizSubgraphVertexIteratorCreate(const gvizSubgraph *sg);

/** Advances the vertex iterator; writes the next vertex index to @p *out_u. */
bool gvizSubgraphVertexIterate(gvizSubgraphVertexIterator *it, size_t *out_u);

/** Creates an iterator over neighbors of @p u within @p sg. Requires both subsets. */
gvizSubgraphNeighborIterator gvizSubgraphNeighborIteratorCreate(const gvizSubgraph *sg, size_t u);

/** Advances the neighbor iterator; writes the next neighbor to @p *out_v. */
bool gvizSubgraphNeighborIterate(gvizSubgraphNeighborIterator *it, size_t *out_v);

#endif
