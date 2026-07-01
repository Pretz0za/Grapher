#include "dsa/gvizSubgraph.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include <stdlib.h>
#include <string.h>

gvizVertexSubset gvizVertexSubsetCreateEmpty(const struct gvizGraph *g) {
  GVIZ_BIT_ARRAY v =
      GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(gvizGraphSize(g)));
  if (!v)
    return NULL;
  memset(v, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(gvizGraphSize(g)));
  return v;
}

void gvizVertexSubsetShowVertex(const gvizVertexSubset vs, size_t u) {
  gvizSetBit(vs, u);
}
void gvizVertexSubsetHideVertex(const gvizVertexSubset vs, size_t u) {
  gvizClearBit(vs, u);
}
void gvizVertexSubsetRelease(const gvizVertexSubset vs) { GVIZ_DEALLOC(vs); }

gvizEdgeSubset gvizEdgeSubsetCreateEmpty(const struct gvizGraph *g) {
  const gvizGraphLayout *layout = g->layout;
  size_t nbits = layout->vertexOffsets[layout->nvertices];
  GVIZ_BIT_ARRAY es =
      GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(nbits));
  if (!es)
    return (gvizEdgeSubset){NULL, NULL};
  memset(es, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(nbits));

  return (gvizEdgeSubset){es, layout};
}

void gvizEdgeSubsetShowEdge(const gvizEdgeSubset es, size_t u, size_t idx) {
  gvizSetBit(es.bitset, es.layout->vertexOffsets[u] + idx);
}
void gvizEdgeSubsetHideEdge(const gvizEdgeSubset es, size_t u, size_t idx) {
  gvizClearBit(es.bitset, es.layout->vertexOffsets[u] + idx);
}
void gvizEdgeSubsetRelease(const gvizEdgeSubset es) {
  GVIZ_DEALLOC(es.bitset);
}

gvizSubgraph gvizSubgraphCreateVertexInduced(const struct gvizGraph *g,
                                             gvizVertexSubset vs) {
  return (gvizSubgraph){g, vs, (gvizEdgeSubset){0}};
}
gvizSubgraph gvizSubgraphCreateEdgeInduced(const struct gvizGraph *g,
                                           gvizEdgeSubset es) {
  return (gvizSubgraph){g, NULL, es};
}
gvizSubgraph gvizSubgraphCreateEmpty(const struct gvizGraph *g) {
  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(g);
  if (!vs)
    return (gvizSubgraph){NULL, NULL, (gvizEdgeSubset){0}};
  gvizEdgeSubset es = gvizEdgeSubsetCreateEmpty(g);
  if (!es.bitset)
    return (gvizSubgraph){NULL, NULL, (gvizEdgeSubset){0}};

  return (gvizSubgraph){g, vs, es};
}

void gvizSubgraphRelease(gvizSubgraph *sg) {
  if (!sg)
    return;
  if (sg->vs)
    gvizVertexSubsetRelease(sg->vs);
  if (sg->es.bitset)
    gvizEdgeSubsetRelease(sg->es);
  sg->g = NULL;
  sg->vs = NULL;
  sg->es = (gvizEdgeSubset){0};
}

void gvizSubgraphMakeEdgeSubset(gvizSubgraph *sg);
void gvizSubgraphMakeVertexSubset(gvizSubgraph *sg);
void gvizSubgraphViewVertex(const gvizSubgraph *sg, size_t u);
void gvizSubgraphHideVertex(const gvizSubgraph *sg, size_t u);
void gvizSubgraphViewEdge(const gvizSubgraph *sg, size_t u, size_t v);
void gvizSubgraphHideEdge(const gvizSubgraph *sg, size_t u, size_t v);
int gvizSubgraphRebuild(gvizSubgraph *sg);
