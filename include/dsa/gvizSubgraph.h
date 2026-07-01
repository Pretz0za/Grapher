#ifndef __GVIZ_SUBGRAPH_H__
#define __GVIZ_SUBGRAPH_H__

#include "dsa/gvizBitArray.h"

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

gvizVertexSubset gvizVertexSubsetCreateEmpty(const struct gvizGraph *g);
void gvizVertexSubsetShowVertex(const gvizVertexSubset vs, size_t u);
void gvizVertexSubsetHideVertex(const gvizVertexSubset vs, size_t u);
void gvizVertexSubsetRelease(const gvizVertexSubset vs);

gvizEdgeSubset gvizEdgeSubsetCreateEmpty(const struct gvizGraph *g);
void gvizEdgeSubsetShowEdge(const gvizEdgeSubset es, size_t u, size_t idx);
void gvizEdgeSubsetHideEdge(const gvizEdgeSubset es, size_t u, size_t idx);
void gvizEdgeSubsetRelease(const gvizEdgeSubset es);

gvizSubgraph gvizSubgraphCreateVertexInduced(const struct gvizGraph *g, gvizVertexSubset v);
gvizSubgraph gvizSubgraphCreateEdgeInduced(const struct gvizGraph *g, gvizEdgeSubset es);
gvizSubgraph gvizSubgraphCreateEmpty(const struct gvizGraph *g);
void gvizSubgraphRelease(gvizSubgraph *sg);

void gvizSubgraphMakeEdgeSubset(gvizSubgraph *sg);
void gvizSubgraphMakeVertexSubset(gvizSubgraph *sg);

void gvizSubgraphViewVertex(const gvizSubgraph *sg, size_t u);
void gvizSubgraphHideVertex(const gvizSubgraph *sg, size_t u);
void gvizSubgraphViewEdge(const gvizSubgraph *sg, size_t u, size_t v);
void gvizSubgraphHideEdge(const gvizSubgraph *sg, size_t u, size_t v);
int gvizSubgraphRebuild(gvizSubgraph *sg);

#endif 
