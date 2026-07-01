#ifndef __GVIZ_SUBGRAPH_H__
#define __GVIZ_SUBGRAPH_H__

#include "dsa/gvizBitArray.h"
#include <stdbool.h>

struct gvizGraph;

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

gvizVertexSubset gvizVertexSubsetCreateEmpty(const struct gvizGraph *g);
void gvizVertexSubsetShowVertex(const gvizVertexSubset vs, size_t u);
void gvizVertexSubsetHideVertex(const gvizVertexSubset vs, size_t u);
void gvizVertexSubsetRelease(const gvizVertexSubset vs);
bool gvizVertexSubsetTest(const gvizVertexSubset vs, size_t u);
size_t gvizVertexSubsetCount(const gvizVertexSubset vs, size_t nvertices);
gvizBitArrayIterator gvizVertexSubsetIteratorCreate(const gvizVertexSubset vs,
                                                    size_t nvertices);

gvizEdgeSubset gvizEdgeSubsetCreateEmpty(const struct gvizGraph *g);
void gvizEdgeSubsetShowEdge(const gvizEdgeSubset es, size_t u, size_t idx);
void gvizEdgeSubsetHideEdge(const gvizEdgeSubset es, size_t u, size_t idx);
void gvizEdgeSubsetRelease(const gvizEdgeSubset es);
size_t gvizEdgeSubsetCount(const gvizEdgeSubset es);
size_t gvizEdgeSubsetVertexEdgeCount(const gvizEdgeSubset es, size_t u);
bool gvizEdgeSubsetTestEdge(const gvizEdgeSubset es, size_t u, size_t idx);
void gvizEdgeSubsetClearVertex(const gvizEdgeSubset es, size_t u);
gvizBitArrayIterator gvizEdgeSubsetIteratorCreate(const gvizEdgeSubset es);
gvizBitArrayIterator gvizEdgeSubsetIteratorCreateVertexRange(
    const gvizEdgeSubset es, size_t u);

gvizSubgraph gvizSubgraphCreateVertexInduced(const struct gvizGraph *g, gvizVertexSubset v);
gvizSubgraph gvizSubgraphCreateEdgeInduced(const struct gvizGraph *g, gvizEdgeSubset es);
gvizSubgraph gvizSubgraphCreateEmpty(const struct gvizGraph *g);
void gvizSubgraphRelease(gvizSubgraph *sg);

void gvizSubgraphMakeEdgeSubset(gvizSubgraph *sg);
void gvizSubgraphMakeVertexSubset(gvizSubgraph *sg);

void gvizSubgraphShowVertex(const gvizSubgraph *sg, size_t u);
void gvizSubgraphHideVertex(const gvizSubgraph *sg, size_t u);
void gvizSubgraphShowEdge(const gvizSubgraph *sg, size_t u, size_t v);
void gvizSubgraphHideEdge(const gvizSubgraph *sg, size_t u, size_t v);
int gvizSubgraphRebuild(gvizSubgraph *sg);

bool gvizSubgraphHasVertex(const gvizSubgraph *sg, size_t u);
size_t gvizSubgraphVertexCount(const gvizSubgraph *sg);
bool gvizSubgraphHasEdge(const gvizSubgraph *sg, size_t u, size_t v);
size_t gvizSubgraphDegree(const gvizSubgraph *sg, size_t u);
size_t gvizSubgraphEdgeCount(const gvizSubgraph *sg);

gvizSubgraphVertexIterator gvizSubgraphVertexIteratorCreate(const gvizSubgraph *sg);
bool gvizSubgraphVertexIterate(gvizSubgraphVertexIterator *it, size_t *out_u);

gvizSubgraphNeighborIterator gvizSubgraphNeighborIteratorCreate(const gvizSubgraph *sg, size_t u);
bool gvizSubgraphNeighborIterate(gvizSubgraphNeighborIterator *it, size_t *out_v);

#endif
