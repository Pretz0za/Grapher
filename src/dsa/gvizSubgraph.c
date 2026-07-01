#include "dsa/gvizSubgraph.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include <stdlib.h>

static bool subgraph_is_full(const gvizSubgraph *sg) {
  return sg && sg->g && sg->g->layout && sg->vs && sg->es.bitset;
}

static size_t layout_vertex_from_bit(const gvizGraphLayout *layout, size_t bit,
                                     size_t *out_idx) {
  size_t lo = 0;
  size_t hi = layout->nvertices;
  while (lo < hi) {
    size_t mid = lo + (hi - lo) / 2;
    if (layout->vertexOffsets[mid + 1] <= bit)
      lo = mid + 1;
    else
      hi = mid;
  }
  *out_idx = bit - layout->vertexOffsets[lo];
  return lo;
}

static int subgraph_find_edge_idx(const gvizSubgraph *sg, size_t u, size_t v,
                                  size_t *out_idx) {
  gvizArray *nb = gvizGraphGetVertexNeighbors(sg->g, u);
  if (!nb)
    return -1;
  int idx = gvizArrayFindOne(nb, &v);
  if (idx < 0)
    return -1;
  *out_idx = (size_t)idx;
  return 0;
}

static void subgraph_clear_incident_edges(const gvizSubgraph *sg, size_t u) {
  const gvizGraphLayout *layout = sg->es.layout;
  gvizEdgeSubsetClearVertex(sg->es, u);

  size_t u_start = layout->vertexOffsets[u];
  size_t u_end = layout->vertexOffsets[u + 1];
  gvizBitArrayIterator it = gvizEdgeSubsetIteratorCreate(sg->es);
  size_t pos;
  while (gvizBitArrayIterate(&it, &pos)) {
    if (pos >= u_start && pos < u_end)
      continue;
    size_t idx;
    size_t w = layout_vertex_from_bit(layout, pos, &idx);
    if (w == u)
      continue;
    gvizArray *nb = gvizGraphGetVertexNeighbors(sg->g, w);
    if (!nb || idx >= nb->count)
      continue;
    if (*(size_t *)gvizArrayAtIndex(nb, idx) == u)
      gvizEdgeSubsetHideEdge(sg->es, w, idx);
  }
}

static int edge_subset_migrate(gvizEdgeSubset *dest, const gvizGraph *g,
                               gvizEdgeSubset old) {
  *dest = gvizEdgeSubsetCreateEmpty(g);
  if (!dest->bitset)
    return -1;
  if (!old.bitset || !old.layout)
    return 0;

  gvizBitArrayIterator it = gvizEdgeSubsetIteratorCreate(old);
  size_t pos;
  while (gvizBitArrayIterate(&it, &pos)) {
    size_t idx;
    size_t u = layout_vertex_from_bit(old.layout, pos, &idx);
    gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
    if (!nb || idx >= nb->count)
      continue;
    size_t v = *(size_t *)gvizArrayAtIndex(nb, idx);
    int idx_new = gvizArrayFindOne(nb, &v);
    if (idx_new < 0)
      continue;
    gvizEdgeSubsetShowEdge(*dest, u, (size_t)idx_new);
  }
  return 0;
}

gvizVertexSubset gvizVertexSubsetCreateEmpty(const struct gvizGraph *g) {
  return gvizBitArrayAlloc(gvizGraphSize(g));
}

void gvizVertexSubsetShowVertex(const gvizVertexSubset vs, size_t u) {
  gvizBitArraySet(vs, u);
}

void gvizVertexSubsetHideVertex(const gvizVertexSubset vs, size_t u) {
  gvizBitArrayClear(vs, u);
}

void gvizVertexSubsetRelease(const gvizVertexSubset vs) {
  gvizBitArrayFree(vs);
}

bool gvizVertexSubsetTest(const gvizVertexSubset vs, size_t u) {
  return gvizBitArrayTest(vs, u);
}

size_t gvizVertexSubsetCount(const gvizVertexSubset vs, size_t nvertices) {
  return gvizBitArrayPopcount(vs, nvertices);
}

gvizBitArrayIterator gvizVertexSubsetIteratorCreate(const gvizVertexSubset vs,
                                                    size_t nvertices) {
  return gvizBitArrayIteratorCreate(vs, nvertices);
}

gvizEdgeSubset gvizEdgeSubsetCreateEmpty(const struct gvizGraph *g) {
  const gvizGraphLayout *layout = g->layout;
  size_t nbits = layout->vertexOffsets[layout->nvertices];
  GVIZ_BIT_ARRAY es = gvizBitArrayAlloc(nbits);
  if (!es)
    return (gvizEdgeSubset){NULL, NULL};
  return (gvizEdgeSubset){es, layout};
}

void gvizEdgeSubsetShowEdge(const gvizEdgeSubset es, size_t u, size_t idx) {
  gvizBitArraySet(es.bitset, es.layout->vertexOffsets[u] + idx);
}

void gvizEdgeSubsetHideEdge(const gvizEdgeSubset es, size_t u, size_t idx) {
  gvizBitArrayClear(es.bitset, es.layout->vertexOffsets[u] + idx);
}

void gvizEdgeSubsetRelease(const gvizEdgeSubset es) {
  gvizBitArrayFree(es.bitset);
}

size_t gvizEdgeSubsetCount(const gvizEdgeSubset es) {
  if (!es.bitset || !es.layout)
    return 0;
  return gvizBitArrayPopcount(
      es.bitset, es.layout->vertexOffsets[es.layout->nvertices]);
}

size_t gvizEdgeSubsetVertexEdgeCount(const gvizEdgeSubset es, size_t u) {
  if (!es.bitset || !es.layout || u >= es.layout->nvertices)
    return 0;
  size_t start = es.layout->vertexOffsets[u];
  size_t end = es.layout->vertexOffsets[u + 1];
  return gvizBitArrayPopcountRange(es.bitset, start, end);
}

bool gvizEdgeSubsetTestEdge(const gvizEdgeSubset es, size_t u, size_t idx) {
  if (!es.bitset || !es.layout || u >= es.layout->nvertices)
    return false;
  return gvizBitArrayTest(es.bitset, es.layout->vertexOffsets[u] + idx);
}

void gvizEdgeSubsetClearVertex(const gvizEdgeSubset es, size_t u) {
  if (!es.bitset || !es.layout || u >= es.layout->nvertices)
    return;
  gvizBitArrayClearRange(es.bitset, es.layout->vertexOffsets[u],
                         es.layout->vertexOffsets[u + 1]);
}

gvizBitArrayIterator gvizEdgeSubsetIteratorCreate(const gvizEdgeSubset es) {
  if (!es.bitset || !es.layout)
    return gvizBitArrayIteratorCreate(NULL, 0);
  return gvizBitArrayIteratorCreate(
      es.bitset, es.layout->vertexOffsets[es.layout->nvertices]);
}

gvizBitArrayIterator gvizEdgeSubsetIteratorCreateVertexRange(
    const gvizEdgeSubset es, size_t u) {
  if (!es.bitset || !es.layout || u >= es.layout->nvertices)
    return gvizBitArrayIteratorCreate(NULL, 0);
  return gvizBitArrayIteratorCreateRange(es.bitset,
                                         es.layout->vertexOffsets[u],
                                         es.layout->vertexOffsets[u + 1]);
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
  if (!es.bitset) {
    gvizVertexSubsetRelease(vs);
    return (gvizSubgraph){NULL, NULL, (gvizEdgeSubset){0}};
  }

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

void gvizSubgraphMakeEdgeSubset(gvizSubgraph *sg) {
  if (!sg || !sg->g || !sg->g->layout || !sg->vs || sg->es.bitset)
    return;

  sg->es = gvizEdgeSubsetCreateEmpty(sg->g);
  if (!sg->es.bitset)
    return;

  const struct gvizGraph *g = sg->g;
  size_t n = sg->g->layout->nvertices;

  gvizBitArrayIterator it = gvizVertexSubsetIteratorCreate(sg->vs, n);
  size_t u;
  while (gvizBitArrayIterate(&it, &u)) {
    gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
    for (size_t idx = 0; idx < nb->count; idx++) {
      size_t v = *(size_t *)gvizArrayAtIndex(nb, idx);
      if (gvizVertexSubsetTest(sg->vs, v))
        gvizEdgeSubsetShowEdge(sg->es, u, idx);
    }
  }
}

void gvizSubgraphMakeVertexSubset(gvizSubgraph *sg) {
  if (!sg || !sg->g || !sg->es.bitset || !sg->es.layout || sg->vs)
    return;

  sg->vs = gvizVertexSubsetCreateEmpty(sg->g);
  if (!sg->vs)
    return;

  const struct gvizGraph *g = sg->g;
  const gvizGraphLayout *layout = sg->es.layout;

  gvizBitArrayIterator it = gvizEdgeSubsetIteratorCreate(sg->es);
  size_t pos;
  while (gvizBitArrayIterate(&it, &pos)) {
    size_t idx;
    size_t u = layout_vertex_from_bit(layout, pos, &idx);
    gvizVertexSubsetShowVertex(sg->vs, u);
    gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
    if (!nb || idx >= nb->count)
      continue;
    size_t v = *(size_t *)gvizArrayAtIndex(nb, idx);
    gvizVertexSubsetShowVertex(sg->vs, v);
  }
}

void gvizSubgraphShowVertex(const gvizSubgraph *sg, size_t u) {
  if (!sg || !sg->vs)
    return;
  gvizVertexSubsetShowVertex(sg->vs, u);
}

void gvizSubgraphHideVertex(const gvizSubgraph *sg, size_t u) {
  if (!sg || !sg->vs)
    return;
  gvizVertexSubsetHideVertex(sg->vs, u);
  if (sg->es.bitset)
    subgraph_clear_incident_edges(sg, u);
}

void gvizSubgraphShowEdge(const gvizSubgraph *sg, size_t u, size_t v) {
  if (!sg || !sg->es.bitset)
    return;
  size_t idx;
  if (subgraph_find_edge_idx(sg, u, v, &idx) < 0)
    return;
  gvizEdgeSubsetShowEdge(sg->es, u, idx);
}

void gvizSubgraphHideEdge(const gvizSubgraph *sg, size_t u, size_t v) {
  if (!sg || !sg->es.bitset)
    return;
  size_t idx;
  if (subgraph_find_edge_idx(sg, u, v, &idx) < 0)
    return;
  gvizEdgeSubsetHideEdge(sg->es, u, idx);
}

int gvizSubgraphRebuild(gvizSubgraph *sg) {
  if (!sg || !sg->g)
    return -1;

  int full = sg->vs && sg->es.bitset;
  int vertex_induced = sg->vs && !sg->es.bitset;
  int edge_induced = !sg->vs && sg->es.bitset;
  if (!full && !vertex_induced && !edge_induced)
    return -1;

  size_t old_nverts =
      sg->g->layout ? sg->g->layout->nvertices : gvizGraphSize(sg->g);

  gvizVertexSubset old_vs = sg->vs;
  gvizEdgeSubset old_es = sg->es;
  if (full || edge_induced)
    sg->es.bitset = NULL;

  gvizGraphBuildLayout((gvizGraph *)sg->g);
  if (!sg->g->layout)
    return -1;

  size_t new_nverts = sg->g->layout->nvertices;

  if (sg->vs || old_vs) {
    GVIZ_BIT_ARRAY neu = gvizBitArrayResize(old_vs, old_nverts, new_nverts);
    if (!neu) {
      if (old_es.bitset && !sg->es.bitset)
        sg->es = old_es;
      return -1;
    }
    if (old_vs && old_vs != neu)
      gvizVertexSubsetRelease(old_vs);
    sg->vs = neu;
  }

  if (full || edge_induced) {
    gvizEdgeSubset neu = (gvizEdgeSubset){0};
    if (edge_subset_migrate(&neu, sg->g, old_es) < 0) {
      if (old_es.bitset)
        gvizEdgeSubsetRelease(old_es);
      return -1;
    }
    if (old_es.bitset)
      gvizEdgeSubsetRelease(old_es);
    sg->es = neu;
  }

  return 0;
}

bool gvizSubgraphHasVertex(const gvizSubgraph *sg, size_t u) {
  if (!subgraph_is_full(sg))
    return false;
  return gvizVertexSubsetTest(sg->vs, u);
}

size_t gvizSubgraphVertexCount(const gvizSubgraph *sg) {
  if (!subgraph_is_full(sg))
    return 0;
  return gvizVertexSubsetCount(sg->vs, sg->g->layout->nvertices);
}

bool gvizSubgraphHasEdge(const gvizSubgraph *sg, size_t u, size_t v) {
  if (!subgraph_is_full(sg))
    return false;
  if (!gvizVertexSubsetTest(sg->vs, u) || !gvizVertexSubsetTest(sg->vs, v))
    return false;
  size_t idx;
  if (subgraph_find_edge_idx(sg, u, v, &idx) < 0)
    return false;
  return gvizEdgeSubsetTestEdge(sg->es, u, idx);
}

size_t gvizSubgraphDegree(const gvizSubgraph *sg, size_t u) {
  if (!subgraph_is_full(sg) || !gvizVertexSubsetTest(sg->vs, u))
    return 0;
  return gvizEdgeSubsetVertexEdgeCount(sg->es, u);
}

size_t gvizSubgraphEdgeCount(const gvizSubgraph *sg) {
  if (!subgraph_is_full(sg))
    return 0;
  return gvizEdgeSubsetCount(sg->es);
}

gvizSubgraphVertexIterator gvizSubgraphVertexIteratorCreate(
    const gvizSubgraph *sg) {
  gvizSubgraphVertexIterator it = {sg, {0}};
  if (!subgraph_is_full(sg))
    return it;
  it.it = gvizVertexSubsetIteratorCreate(sg->vs, sg->g->layout->nvertices);
  return it;
}

bool gvizSubgraphVertexIterate(gvizSubgraphVertexIterator *it, size_t *out_u) {
  if (!it || !subgraph_is_full(it->sg))
    return false;
  return gvizBitArrayIterate(&it->it, out_u);
}

gvizSubgraphNeighborIterator gvizSubgraphNeighborIteratorCreate(
    const gvizSubgraph *sg, size_t u) {
  gvizSubgraphNeighborIterator it = {sg, u, 0, {0}};
  if (!subgraph_is_full(sg) || !gvizVertexSubsetTest(sg->vs, u))
    return it;
  const gvizGraphLayout *layout = sg->g->layout;
  it.base = layout->vertexOffsets[u];
  it.it = gvizEdgeSubsetIteratorCreateVertexRange(sg->es, u);
  return it;
}

bool gvizSubgraphNeighborIterate(gvizSubgraphNeighborIterator *it,
                                 size_t *out_v) {
  if (!it || !subgraph_is_full(it->sg))
    return false;

  size_t pos;
  if (!gvizBitArrayIterate(&it->it, &pos))
    return false;

  size_t idx = pos - it->base;
  gvizArray *nb = gvizGraphGetVertexNeighbors(it->sg->g, it->u);
  if (!nb || idx >= nb->count)
    return false;

  *out_v = *(size_t *)gvizArrayAtIndex(nb, idx);
  return true;
}
