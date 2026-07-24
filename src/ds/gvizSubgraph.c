#include "ds/gvizSubgraph.h"
#include "ds/gvizArray.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizGraph.h"
#include <stdlib.h>
#include <string.h>

static bool subgraph_has_vertices(const gvizSubgraph *sg) {
  return sg && sg->g && sg->vs;
}

static bool subgraph_is_full(const gvizSubgraph *sg) {
  return subgraph_has_vertices(sg) && sg->es.bitset;
}

static bool subgraph_is_vertex_induced(const gvizSubgraph *sg) {
  return subgraph_has_vertices(sg) && !sg->es.bitset;
}

static void bitarray_fill_all(GVIZ_BIT_ARRAY arr, size_t nbits) {
  if (!arr || nbits == 0)
    return;
  size_t units = GVIZ_ARRAY_UNITS(nbits);
  memset(arr, 0xFF, units);
  size_t rem = nbits % GVIZ_BITS_PER_UNIT;
  if (rem != 0)
    arr[units - 1] &= (GVIZ_BIT_UNIT)(((GVIZ_BIT_UNIT)1 << rem) - 1);
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

void gvizVertexSubsetClearAll(const gvizVertexSubset vs, size_t nvertices) {
  gvizBitArrayClearAll(vs, nvertices);
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
  return (gvizSubgraph){g, vs, (gvizEdgeSubset){0}, gvizGraphSize(g)};
}

gvizSubgraph gvizSubgraphCreateEmpty(const struct gvizGraph *g) {
  gvizVertexSubset vs = gvizVertexSubsetCreateEmpty(g);
  if (!vs)
    return (gvizSubgraph){NULL, NULL, (gvizEdgeSubset){0}, 0};
  gvizEdgeSubset es = gvizEdgeSubsetCreateEmpty(g);
  if (!es.bitset) {
    gvizVertexSubsetRelease(vs);
    return (gvizSubgraph){NULL, NULL, (gvizEdgeSubset){0}, 0};
  }

  return (gvizSubgraph){g, vs, es, gvizGraphSize(g)};
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
  sg->vertexCapacity = 0;
}

bool gvizSubgraphIsFull(const gvizSubgraph *sg) { return subgraph_is_full(sg); }

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

void gvizSubgraphShowVertex(const gvizSubgraph *sg, size_t u) {
  if (!sg || !sg->vs)
    return;
  gvizVertexSubsetShowVertex(sg->vs, u);
}

void gvizSubgraphHideVertex(const gvizSubgraph *sg, size_t u) {
  if (!sg || !sg->vs)
    return;
  gvizVertexSubsetHideVertex(sg->vs, u);
  if (subgraph_is_full(sg))
    subgraph_clear_incident_edges(sg, u);
}

void gvizSubgraphShowEdge(const gvizSubgraph *sg, size_t u, size_t v) {
  if (!subgraph_is_full(sg))
    return;
  size_t idx;
  if (subgraph_find_edge_idx(sg, u, v, &idx) < 0)
    return;
  gvizEdgeSubsetShowEdge(sg->es, u, idx);
}

void gvizSubgraphHideEdge(const gvizSubgraph *sg, size_t u, size_t v) {
  if (!subgraph_is_full(sg))
    return;
  size_t idx;
  if (subgraph_find_edge_idx(sg, u, v, &idx) < 0)
    return;
  gvizEdgeSubsetHideEdge(sg->es, u, idx);
}

static int subgraph_rebuild_vertex_induced(gvizSubgraph *sg) {
  size_t needed = gvizGraphSize(sg->g);
  if (needed <= sg->vertexCapacity)
    return 0;

  size_t newCap = sg->vertexCapacity ? sg->vertexCapacity * 2 : 64;
  while (newCap < needed)
    newCap *= 2;

  GVIZ_BIT_ARRAY neu = gvizBitArrayResize(sg->vs, sg->vertexCapacity, newCap);
  if (!neu)
    return -1;
  gvizVertexSubsetRelease(sg->vs);
  sg->vs = neu;
  sg->vertexCapacity = newCap;
  return 0;
}

static int subgraph_rebuild_full(gvizSubgraph *sg) {
  size_t old_nverts = sg->vertexCapacity;

  gvizVertexSubset old_vs = sg->vs;
  gvizEdgeSubset old_es = sg->es;
  sg->es.bitset = NULL;

  // gvizGraphBuildLayout rewrites the shared layout in place, but the old
  // edge bits are positioned by the old offsets. Snapshot them so the
  // migration below decodes old bit positions with the layout they were
  // written under.
  gvizGraphLayout old_layout_snapshot = {0};
  if (old_es.layout) {
    size_t n = old_es.layout->nvertices;
    old_layout_snapshot.nvertices = n;
    old_layout_snapshot.edgeCount = old_es.layout->edgeCount;
    old_layout_snapshot.vertexOffsets = GVIZ_ALLOC((n + 1) * sizeof(size_t));
    if (!old_layout_snapshot.vertexOffsets) {
      sg->es = old_es;
      return -1;
    }
    memcpy(old_layout_snapshot.vertexOffsets, old_es.layout->vertexOffsets,
           (n + 1) * sizeof(size_t));
    old_es.layout = &old_layout_snapshot;
  }

  gvizGraphBuildLayout((gvizGraph *)sg->g);
  if (!sg->g->layout) {
    GVIZ_DEALLOC(old_layout_snapshot.vertexOffsets);
    return -1;
  }

  size_t new_nverts = sg->g->layout->nvertices;

  GVIZ_BIT_ARRAY neu = gvizBitArrayResize(old_vs, old_nverts, new_nverts);
  if (!neu) {
    if (old_es.bitset && !sg->es.bitset) {
      old_es.layout = sg->g->layout; // never leave a dangling snapshot ref
      sg->es = old_es;
    }
    GVIZ_DEALLOC(old_layout_snapshot.vertexOffsets);
    return -1;
  }
  if (old_vs && old_vs != neu)
    gvizVertexSubsetRelease(old_vs);
  sg->vs = neu;
  sg->vertexCapacity = new_nverts;

  gvizEdgeSubset migrated = (gvizEdgeSubset){0};
  int err = edge_subset_migrate(&migrated, sg->g, old_es);
  if (old_es.bitset)
    gvizEdgeSubsetRelease(old_es);
  GVIZ_DEALLOC(old_layout_snapshot.vertexOffsets);
  if (err < 0)
    return -1;
  sg->es = migrated;

  return 0;
}

int gvizSubgraphRebuild(gvizSubgraph *sg) {
  if (!sg || !sg->g || !sg->vs)
    return -1;

  if (subgraph_is_vertex_induced(sg))
    return subgraph_rebuild_vertex_induced(sg);
  if (subgraph_is_full(sg))
    return subgraph_rebuild_full(sg);
  return -1;
}

bool gvizSubgraphHasVertex(const gvizSubgraph *sg, size_t u) {
  if (!subgraph_has_vertices(sg))
    return false;
  return gvizVertexSubsetTest(sg->vs, u);
}

size_t gvizSubgraphVertexCount(const gvizSubgraph *sg) {
  if (!subgraph_has_vertices(sg))
    return 0;
  return gvizVertexSubsetCount(sg->vs, sg->vertexCapacity);
}

bool gvizSubgraphHasEdge(const gvizSubgraph *sg, size_t u, size_t v) {
  if (!subgraph_has_vertices(sg))
    return false;
  if (!gvizVertexSubsetTest(sg->vs, u) || !gvizVertexSubsetTest(sg->vs, v))
    return false;

  size_t idx;
  if (subgraph_find_edge_idx(sg, u, v, &idx) < 0)
    return false;

  if (subgraph_is_full(sg))
    return gvizEdgeSubsetTestEdge(sg->es, u, idx);
  return true;
}

size_t gvizSubgraphDegree(const gvizSubgraph *sg, size_t u) {
  if (!subgraph_has_vertices(sg) || !gvizVertexSubsetTest(sg->vs, u))
    return 0;

  if (subgraph_is_full(sg))
    return gvizEdgeSubsetVertexEdgeCount(sg->es, u);

  gvizArray *nb = gvizGraphGetVertexNeighbors(sg->g, u);
  if (!nb)
    return 0;

  size_t degree = 0;
  for (size_t i = 0; i < nb->count; i++) {
    size_t v = *(size_t *)gvizArrayAtIndex(nb, i);
    if (gvizVertexSubsetTest(sg->vs, v))
      degree++;
  }
  return degree;
}

size_t gvizSubgraphEdgeCount(const gvizSubgraph *sg) {
  if (!subgraph_has_vertices(sg))
    return 0;

  if (subgraph_is_full(sg))
    return gvizEdgeSubsetCount(sg->es);

  size_t count = 0;
  size_t n = sg->vertexCapacity;
  gvizBitArrayIterator vit = gvizVertexSubsetIteratorCreate(sg->vs, n);
  size_t u;
  while (gvizBitArrayIterate(&vit, &u)) {
    gvizArray *nb = gvizGraphGetVertexNeighbors(sg->g, u);
    if (!nb)
      continue;
    for (size_t i = 0; i < nb->count; i++) {
      size_t v = *(size_t *)gvizArrayAtIndex(nb, i);
      if (gvizVertexSubsetTest(sg->vs, v))
        count++;
    }
  }
  return count;
}

gvizSubgraphVertexIterator gvizSubgraphVertexIteratorCreate(
    const gvizSubgraph *sg) {
  gvizSubgraphVertexIterator it = {sg, {0}};
  if (!subgraph_has_vertices(sg))
    return it;
  it.it = gvizVertexSubsetIteratorCreate(sg->vs, sg->vertexCapacity);
  return it;
}

bool gvizSubgraphVertexIterate(gvizSubgraphVertexIterator *it, size_t *out_u) {
  if (!it || !subgraph_has_vertices(it->sg))
    return false;
  return gvizBitArrayIterate(&it->it, out_u);
}

gvizSubgraphNeighborIterator gvizSubgraphNeighborIteratorCreate(
    const gvizSubgraph *sg, size_t u) {
  gvizSubgraphNeighborIterator it = {
      sg, u, 0, {0}, 0, NULL, GVIZ_SUBGRAPH_NEIGHBOR_ITER_NONE};
  if (!subgraph_has_vertices(sg) || !gvizVertexSubsetTest(sg->vs, u))
    return it;

  it.nb = gvizGraphGetVertexNeighbors(sg->g, u);
  if (!it.nb)
    return it;

  if (subgraph_is_full(sg)) {
    const gvizGraphLayout *layout = sg->g->layout;
    it.base = layout->vertexOffsets[u];
    it.it = gvizEdgeSubsetIteratorCreateVertexRange(sg->es, u);
    it.adj_idx = SIZE_MAX;
    it.mode = GVIZ_SUBGRAPH_NEIGHBOR_ITER_FULL;
    return it;
  }

  it.adj_idx = 0;
  it.mode = GVIZ_SUBGRAPH_NEIGHBOR_ITER_INDUCED;
  return it;
}

bool gvizSubgraphNeighborIterate(gvizSubgraphNeighborIterator *it,
                                 size_t *out_v) {
  if (!it)
    return false;

  if (it->mode == GVIZ_SUBGRAPH_NEIGHBOR_ITER_FULL) {
    size_t pos;
    if (!gvizBitArrayIterate(&it->it, &pos))
      return false;

    size_t idx = pos - it->base;
    if (idx >= it->nb->count)
      return false;

    *out_v = *(size_t *)gvizArrayAtIndex(it->nb, idx);
    return true;
  }

  if (it->mode == GVIZ_SUBGRAPH_NEIGHBOR_ITER_INDUCED) {
    while (it->adj_idx < it->nb->count) {
      size_t v = *(size_t *)gvizArrayAtIndex(it->nb, it->adj_idx++);
      if (gvizVertexSubsetTest(it->sg->vs, v)) {
        *out_v = v;
        return true;
      }
    }
  }
  return false;
}

void gvizSubgraphMakeFull(gvizSubgraph *sg) {
  if (!sg || !sg->g || !sg->g->layout || !sg->vs || !sg->es.bitset)
    return;

  const gvizGraphLayout *layout = sg->g->layout;
  bitarray_fill_all(sg->vs, layout->nvertices);
  bitarray_fill_all(sg->es.bitset, layout->vertexOffsets[layout->nvertices]);
}

gvizSubgraph gvizSubgraphCreateFull(const struct gvizGraph *g) {
  if (!g || !g->layout)
    return (gvizSubgraph){NULL, NULL, (gvizEdgeSubset){0}};

  gvizSubgraph sg = gvizSubgraphCreateEmpty(g);
  if (!sg.vs)
    return sg;

  gvizSubgraphMakeFull(&sg);
  return sg;
}
