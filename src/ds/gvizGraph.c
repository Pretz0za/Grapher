#include "ds/gvizGraph.h"
#include "ds/gvizArray.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int inBoundsVertex(const gvizGraph *g, size_t idx) {
  return idx < g->vertices.count;
}

static int inBoundsVertices(const gvizGraph *g, size_t idx1, size_t idx2) {
  return idx1 < g->vertices.count && idx2 < g->vertices.count;
}

/* Finds the position of a neighbor with vertex id @p to in @p edges,
 * comparing only the idx field (edges may carry arbitrary weights, so a raw
 * memcmp via gvizArrayFindOne would also compare weight bytes). */
static int findNeighborPos(const gvizArray *edges, size_t to) {
  for (size_t i = 0; i < edges->count; i++) {
    if (((gvizEdge *)gvizArrayAtIndex(edges, i))->idx == to)
      return (int)i;
  }
  return -1;
}

int gvizVertexInit(gvizVertex *v, void *data) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err = gvizArrayInit(&v->edges, sizeof(gvizEdge));
  return err;
}

int gvizVertexInitAtCapacity(gvizVertex *v, void *data,
                             size_t initialCapacity) {
  if (v == NULL)
    return -1;
  v->data = data;
  int err =
      gvizArrayInitAtCapacity(&v->edges, sizeof(gvizEdge), initialCapacity);
  return err;
}

int gvizVertexCopy(gvizVertex *dest, const gvizVertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = gvizArrayCopy(&dest->edges, &src->edges);
  return err;
}

int gvizVertexClone(gvizVertex *dest, const gvizVertex *src) {
  if (!dest || !src)
    return -1;

  dest->data = src->data;
  int err = gvizArrayClone(&dest->edges, &src->edges);
  return err;
}

int gvizGraphInit(gvizGraph *g, int directed) {
  int err;
  if (g == NULL)
    return -1;
  err = gvizArrayInit(&g->vertices, sizeof(gvizVertex));
  if (err < 0)
    return err;
  g->directed = directed;
  g->map = NULL;
  g->layout = NULL;
  return 0;
}

int gvizGraphInitAtCapacity(gvizGraph *g, int directed,
                            size_t initialCapacity) {
  int err;
  if (g == NULL)
    return -1;
  err = gvizArrayInitAtCapacity(&g->vertices, sizeof(gvizVertex),
                                initialCapacity);
  if (err < 0)
    return err;

  g->directed = directed;
  g->map = NULL;
  g->layout = NULL;
  return 0;
}


size_t gvizGraphSize(const gvizGraph *g) {
	return g->vertices.count;
}

int gvizGraphIsDirected(const gvizGraph *g) {
	return g->directed;
}

size_t gvizGraphEdgeCount(const gvizGraph *g) {
  if (!g->layout)
    return 0;
  return g->layout->edgeCount;
}

void gvizGraphBuildLayout(gvizGraph *g) {
  size_t n = gvizGraphSize(g);
  if (!g->layout) {
    g->layout = GVIZ_ALLOC(sizeof(gvizGraphLayout));
    if (!g->layout)
      return;
    g->layout->nvertices = n;
    g->layout->vertexOffsets = GVIZ_ALLOC((n + 1) * sizeof(size_t));
    if (!g->layout->vertexOffsets) {
      GVIZ_DEALLOC(g->layout);
      g->layout = NULL;
      return;
    }
  } else if (g->layout->nvertices != n) {
    size_t *grown =
        GVIZ_REALLOC(g->layout->vertexOffsets, (n + 1) * sizeof(size_t));
    if (!grown) {
      GVIZ_DEALLOC(g->layout->vertexOffsets);
      GVIZ_DEALLOC(g->layout);
      g->layout = NULL;
      return;
    }
    g->layout->nvertices = n;
    g->layout->vertexOffsets = grown;
  }

  size_t off = 0;
  for (size_t i = 0; i < n; i++) {
    g->layout->vertexOffsets[i] = off;
    off += gvizGraphDegree(g, i);
  }
  g->layout->vertexOffsets[n] = off;
  g->layout->edgeCount = g->directed ? off : off / 2;
}

int gvizGraphAddVertex(gvizGraph *g, void *data, gvizArray *in,
                       gvizArray *out) {
  gvizVertex v;
  int err;

  if (out != NULL) {
    err = gvizVertexInitAtCapacity(&v, data, out->count);
    if (err < 0)
      return err;
    gvizArrayCopy(&v.edges, out);
  } else {
    err = gvizVertexInit(&v, data);
    if (err < 0)
      return err;
  }

  gvizArrayPush(&g->vertices, &v);

  if (in != NULL) {
    size_t idx = g->vertices.count - 1;
    for (size_t i = 0; i < in->count; i++) {
      gvizEdge e = *(gvizEdge *)gvizArrayAtIndex(in, i);
      e.idx = idx;
      gvizVertex *src = gvizArrayAtIndex(&g->vertices, e.idx);
      err = gvizArrayPush(&src->edges, &e);
      if (err < 0)
        return err;
    }
  }

  return 0;
}

int gvizGraphAddEdge(gvizGraph *g, size_t from, size_t to, double weight) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  int err;
  gvizEdge forward = {to, weight};
  err = gvizArrayPush(&((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges,
                      &forward);
  if (err == 0 && !g->directed) {
    gvizEdge backward = {from, weight};
    err = gvizArrayPush(&((gvizVertex *)gvizArrayAtIndex(&g->vertices, to))->edges,
                        &backward);
  }
  return err;
}

int gvizGraphRemoveEdge(gvizGraph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  gvizArray *fromEdges =
      &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges;
  int pos = findNeighborPos(fromEdges, to);
  if (pos < 0)
    return -1;
  gvizArrayDeleteAtIndex(fromEdges, (size_t)pos);

  if (!g->directed) {
    gvizArray *toEdges =
        &((gvizVertex *)gvizArrayAtIndex(&g->vertices, to))->edges;
    int backPos = findNeighborPos(toEdges, from);
    if (backPos < 0)
      return -1;
    gvizArrayDeleteAtIndex(toEdges, (size_t)backPos);
  }
  return 0;
}

void gvizGraphSetVertexData(gvizGraph *g, size_t idx, void *data) {
  ((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->data = data;
  return;
}

void *gvizGraphGetVertexData(gvizGraph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return NULL;
  return ((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->data;
}

size_t gvizGraphDegree(const gvizGraph *g, size_t idx) {
  if (!inBoundsVertex(g, idx))
    return 0;
  return ((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->edges.count;
}

size_t gvizGraphNeighbor(const gvizGraph *g, size_t idx, size_t i) {
  gvizArray *edges = &((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->edges;
  return ((gvizEdge *)gvizArrayAtIndex(edges, i))->idx;
}

double gvizGraphNeighborWeight(const gvizGraph *g, size_t idx, size_t i) {
  gvizArray *edges = &((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->edges;
  return ((gvizEdge *)gvizArrayAtIndex(edges, i))->weight;
}

int gvizGraphNeighborPosition(const gvizGraph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  return findNeighborPos(
      &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges, to);
}

int gvizGraphEdgeExists(gvizGraph *g, size_t from, size_t to) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  return findNeighborPos(
             &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges,
             to) >= 0
             ? 1
             : 0;
}

int gvizGraphGetEdgeWeight(const gvizGraph *g, size_t from, size_t to,
                           double *outWeight) {
  if (!inBoundsVertices(g, from, to))
    return -1;
  gvizArray *edges = &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges;
  int pos = findNeighborPos(edges, to);
  if (pos < 0)
    return -1;
  *outWeight = ((gvizEdge *)gvizArrayAtIndex(edges, (size_t)pos))->weight;
  return 0;
}

int gvizGraphSetEdgeWeight(gvizGraph *g, size_t from, size_t to,
                           double weight) {
  if (!inBoundsVertices(g, from, to))
    return -1;

  gvizArray *fromEdges =
      &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges;
  int pos = findNeighborPos(fromEdges, to);
  if (pos < 0)
    return -1;
  ((gvizEdge *)gvizArrayAtIndex(fromEdges, (size_t)pos))->weight = weight;

  if (!g->directed) {
    gvizArray *toEdges =
        &((gvizVertex *)gvizArrayAtIndex(&g->vertices, to))->edges;
    int backPos = findNeighborPos(toEdges, from);
    if (backPos < 0)
      return -1;
    ((gvizEdge *)gvizArrayAtIndex(toEdges, (size_t)backPos))->weight = weight;
  }
  return 0;
}

int gvizGraphInsertNeighborAt(gvizGraph *g, size_t from, size_t to,
                              double weight, size_t pos) {
  if (!inBoundsVertex(g, from))
    return -1;
  gvizArray *edges = &((gvizVertex *)gvizArrayAtIndex(&g->vertices, from))->edges;
  gvizEdge e = {to, weight};
  return gvizArrayInsert(edges, &e, pos);
}

int gvizGraphReorderNeighbors(gvizGraph *g, size_t idx, const size_t *order,
                              size_t n) {
  if (!inBoundsVertex(g, idx))
    return -1;

  gvizArray *edges = &((gvizVertex *)gvizArrayAtIndex(&g->vertices, idx))->edges;
  if (n != edges->count)
    return -1;

  gvizArray rebuilt;
  if (gvizArrayInitAtCapacity(&rebuilt, sizeof(gvizEdge), n) < 0)
    return -1;

  for (size_t i = 0; i < n; i++) {
    int pos = findNeighborPos(edges, order[i]);
    if (pos < 0) {
      gvizArrayRelease(&rebuilt);
      return -1;
    }
    gvizEdge e = *(gvizEdge *)gvizArrayAtIndex(edges, (size_t)pos);
    gvizArrayPush(&rebuilt, &e);
  }

  gvizArrayRelease(edges);
  *edges = rebuilt;
  return 0;
}

void gvizVertexRelease(gvizVertex *v) { gvizArrayRelease(&v->edges); }

void gvizGraphRelease(gvizGraph *g) {
  if (!gvizArrayIsEmpty(&g->vertices)) {
    for (size_t i = 0; i < g->vertices.count; i++) {
      gvizVertexRelease(gvizArrayAtIndex(&g->vertices, i));
    }
  }
  gvizArrayRelease(&g->vertices);

  if (g->map)
    GVIZ_DEALLOC(g->map);

  if (g->layout) {
    GVIZ_DEALLOC(g->layout->vertexOffsets);
    GVIZ_DEALLOC(g->layout);
    g->layout = NULL;
  }
}

void gvizGraphClear(gvizGraph *g) {
  if (g->vertices.count == 0)
    return;
  for (size_t i = 0; i < g->vertices.count; i++) {
    gvizVertexRelease(gvizArrayAtIndex(&g->vertices, i));
  }
  g->vertices.count = 0;
  if (g->layout) {
    GVIZ_DEALLOC(g->layout->vertexOffsets);
    GVIZ_DEALLOC(g->layout);
    g->layout = NULL;
  }
}

int gvizGraphCopy(gvizGraph *dest, const gvizGraph *src) {
  if (!dest || !src)
    return -1;

  gvizGraphClear(dest);

  // NOTE: We can't just gvizArrayCopy() since that is a shallow copy
  // The vertices in dest and src will share the memory for their adjacency
  // lists, causing possible double frees if both graphs are released.

  if (dest->vertices.capacity < src->vertices.count) {
    gvizArrayRelease(&dest->vertices);
    gvizArrayInitAtCapacity(&dest->vertices, sizeof(gvizVertex),
                            src->vertices.count);
  }

  for (size_t i = 0; i < src->vertices.count; i++) {
    int err = gvizVertexClone(gvizArrayAtIndex(&dest->vertices, i),
                              gvizArrayAtIndex(&src->vertices, i));
    if (err < 0)
      return err;
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

int gvizGraphClone(gvizGraph *dest, const gvizGraph *src) {
  if (!dest || !src)
    return -1;
  gvizGraphInitAtCapacity(dest, src->directed, src->vertices.count);

  for (size_t i = 0; i < src->vertices.count; i++) {
    int err = gvizVertexClone(gvizArrayAtIndex(&dest->vertices, i),
                              gvizArrayAtIndex(&src->vertices, i));
    if (err < 0)
      return err;
  }

  dest->vertices.count = src->vertices.count;
  dest->directed = src->directed;

  return 0;
}

static int graphFillReversed(gvizGraph *dest, const gvizGraph *src) {
  size_t n = src->vertices.count;
  gvizArray *reversedLists = GVIZ_ALLOC(sizeof(gvizArray) * n);
  if (!reversedLists)
    return -1;
  for (size_t i = 0; i < n; i++) {
    gvizArrayInit(&reversedLists[i], sizeof(gvizEdge));
  }

  for (size_t i = 0; i < n; i++) {
    size_t degree = gvizGraphDegree(src, i);
    for (size_t j = 0; j < degree; j++) {
      size_t v = gvizGraphNeighbor(src, i, j);
      double weight = gvizGraphNeighborWeight(src, i, j);
      gvizEdge reversed = {i, weight};
      gvizArrayPush(reversedLists + v, &reversed);
    }
  }

  for (size_t i = 0; i < n; i++) {
    gvizVertex *target = gvizArrayAtIndex(&dest->vertices, i);
    gvizVertex *source = gvizArrayAtIndex(&src->vertices, i);
    target->data = source->data;
    gvizArrayMove(&target->edges, &reversedLists[i]);
  }
  GVIZ_DEALLOC(reversedLists);

  dest->vertices.count = n;
  dest->directed = src->directed;
  return 0;
}

int gvizGraphCopyReversed(gvizGraph *dest, const gvizGraph *src) {
  if (!src->directed)
    return gvizGraphCopy(dest, src);

  gvizGraphClear(dest);

  if (dest->vertices.capacity < src->vertices.count) {
    gvizArrayRelease(&dest->vertices);
    gvizArrayInitAtCapacity(&dest->vertices, sizeof(gvizVertex),
                            src->vertices.count);
  }

  return graphFillReversed(dest, src);
}

int gvizGraphCloneReversed(gvizGraph *dest, const gvizGraph *src) {
  if (!src->directed)
    return gvizGraphClone(dest, src);

  gvizGraphInitAtCapacity(dest, src->directed, src->vertices.count);

  return graphFillReversed(dest, src);
}

