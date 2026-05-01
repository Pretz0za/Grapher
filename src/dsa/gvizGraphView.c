#include "dsa/gvizGraphView.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizDeque.h"
#include "dsa/gvizGraph.h"
#include <stdlib.h>
#include <string.h>

size_t gvizGraphViewTotalEdgeSlots(const gvizGraphView *view) {
  if (!view || !view->graph) {
    return 0;
  }
  size_t total = 0;
  size_t n = view->graph->vertices.count;
  for (size_t u = 0; u < n; u++) {
    gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, u);
    if (adj) {
      total += adj->count;
    }
  }
  return total;
}

int gvizGraphViewRebuildEdgeStart(gvizGraphView *view) {
  if (!view || !view->graph) {
    return -1;
  }
  size_t n = view->graph->vertices.count;
  size_t *arr =
      (size_t *)GVIZ_REALLOC(view->edgeStart, (n + 1) * sizeof(size_t));
  if (!arr) {
    return -1;
  }
  arr[0] = 0;
  for (size_t u = 0; u < n; u++) {
    gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, u);
    size_t deg = adj ? adj->count : 0;
    arr[u + 1] = arr[u] + deg;
  }
  view->edgeStart = arr;
  view->edgeStartStale = 0;
  return 0;
}

int gvizGraphViewInitFull(gvizGraphView *view, gvizGraph *graph) {
  if (!view || !graph) {
    return -1;
  }
  view->graph = graph;
  view->vertexMask = NULL;
  view->edgeMask = NULL;
  view->edgeStart = NULL;
  view->count = graph->vertices.count;
  view->edgeStartStale = 0;
  return 0;
}

int gvizGraphViewInitFromVertices(gvizGraphView *view, gvizGraph *graph,
                                  const GVIZ_BIT_ARRAY mask) {
  if (!view || !graph) {
    return -1;
  }
  view->graph = graph;
  view->edgeMask = NULL;
  view->edgeStart = NULL;
  view->edgeStartStale = 0;

  if (!mask) {
    view->vertexMask = NULL;
    view->count = graph->vertices.count;
    return 0;
  }

  size_t n = graph->vertices.count;
  view->vertexMask = gvizBitArrayClone(mask, n);
  if (n > 0 && !view->vertexMask) {
    return -1;
  }
  view->count = gvizBitArrayCount(view->vertexMask, n);
  return 0;
}

int gvizGraphViewInitFromEdges(gvizGraphView *view, gvizGraph *graph,
                               const GVIZ_BIT_ARRAY edgeMask) {
  if (!view || !graph) {
    return -1;
  }
  view->graph = graph;
  view->vertexMask = NULL;
  view->edgeMask = NULL;
  view->edgeStart = NULL;
  view->edgeStartStale = 0;
  view->count = graph->vertices.count;

  if (gvizGraphViewRebuildEdgeStart(view) != 0) {
    return -1;
  }

  if (edgeMask) {
    size_t total = view->edgeStart[graph->vertices.count];
    if (total > 0) {
      view->edgeMask = gvizBitArrayClone(edgeMask, total);
      if (!view->edgeMask) {
        GVIZ_DEALLOC(view->edgeStart);
        view->edgeStart = NULL;
        return -1;
      }
    }
  }
  return 0;
}

size_t gvizGraphViewVertexCount(const gvizGraphView *view) {
  if (!view) {
    return 0;
  }
  return view->count;
}

int gvizGraphViewVertexInView(const gvizGraphView *view, size_t u) {
  if (!view || !view->graph) {
    return -1;
  }
  if (u >= view->graph->vertices.count) {
    return 0;
  }
  if (!view->vertexMask) {
    return 1;
  }
  return gvizTestBit(view->vertexMask, u) ? 1 : 0;
}

static int viewEnsureEdgeStart(gvizGraphView *view) {
  if (!view->edgeStart || view->edgeStartStale) {
    return gvizGraphViewRebuildEdgeStart(view);
  }
  return 0;
}

static size_t neighborIndex(const gvizArray *adj, size_t v, int *found) {
  for (size_t i = 0; i < adj->count; i++) {
    size_t w = *(size_t *)gvizArrayAtIndex((gvizArray *)adj, i);
    if (w == v) {
      *found = 1;
      return i;
    }
  }
  *found = 0;
  return 0;
}

int gvizGraphViewEdgeInView(gvizGraphView *view, size_t u, size_t v) {
  if (!view || !view->graph) {
    return -1;
  }
  size_t n = view->graph->vertices.count;
  if (u >= n || v >= n) {
    return 0;
  }
  if (!gvizGraphViewVertexInView(view, u) ||
      !gvizGraphViewVertexInView(view, v)) {
    return 0;
  }
  gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, u);
  if (!adj) {
    return 0;
  }
  int found = 0;
  size_t idx = neighborIndex(adj, v, &found);
  if (!found) {
    return 0;
  }
  if (!view->edgeMask) {
    return 1;
  }
  if (viewEnsureEdgeStart(view) != 0) {
    return -1;
  }
  size_t bit = view->edgeStart[u] + idx;
  return gvizTestBit(view->edgeMask, bit) ? 1 : 0;
}

void gvizGraphViewVertexIterInit(gvizGraphViewVertexIter *iter,
                                 const gvizGraphView *view) {
  if (!iter) {
    return;
  }
  iter->view = view;
  iter->cursor = 0;
  iter->hasMask = 0;
  iter->bitIter.bits = NULL;
  iter->bitIter.cursor = 0;
  iter->bitIter.units = 0;
  iter->bitIter.nbits = 0;
  if (view && view->vertexMask) {
    iter->hasMask = 1;
    gvizBitArrayIterInit(&iter->bitIter, view->vertexMask,
                         view->graph->vertices.count);
  }
}

int gvizGraphViewVertexIterNext(gvizGraphViewVertexIter *iter, size_t *out) {
  if (!iter || !iter->view || !out) {
    return 0;
  }
  if (iter->hasMask) {
    return gvizBitArrayIterNext(&iter->bitIter, out);
  }
  size_t n = iter->view->graph->vertices.count;
  if (iter->cursor >= n) {
    return 0;
  }
  *out = iter->cursor++;
  return 1;
}

void gvizGraphViewVertexIterRelease(gvizGraphViewVertexIter *iter) {
  if (!iter) {
    return;
  }
  if (iter->hasMask) {
    gvizBitArrayIterRelease(&iter->bitIter);
    iter->hasMask = 0;
  }
  iter->view = NULL;
  iter->cursor = 0;
}

int gvizGraphViewNeighborsIterInit(gvizGraphViewNeighborsIter *iter,
                                   gvizGraphView *view, size_t u) {
  if (!iter || !view || !view->graph) {
    return -1;
  }
  iter->view = view;
  iter->u = u;
  iter->i = 0;
  iter->edgeBase = 0;
  iter->neighborN = 0;
  if (u >= view->graph->vertices.count) {
    return 0;
  }
  gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, u);
  iter->neighborN = adj ? adj->count : 0;
  if (view->edgeMask) {
    if (viewEnsureEdgeStart(view) != 0) {
      return -1;
    }
    iter->edgeBase = view->edgeStart[u];
  }
  return 0;
}

int gvizGraphViewNeighborsIterNext(gvizGraphViewNeighborsIter *iter,
                                   size_t *out) {
  if (!iter || !iter->view || !out) {
    return 0;
  }
  gvizGraphView *view = iter->view;
  gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, iter->u);
  if (!adj) {
    return 0;
  }
  while (iter->i < iter->neighborN) {
    size_t v = *(size_t *)gvizArrayAtIndex(adj, iter->i);
    size_t slot = iter->i;
    iter->i++;

    if (view->vertexMask && !gvizTestBit(view->vertexMask, v)) {
      continue;
    }
    if (view->edgeMask) {
      if (!gvizTestBit(view->edgeMask, iter->edgeBase + slot)) {
        continue;
      }
    }
    *out = v;
    return 1;
  }
  return 0;
}

void gvizGraphViewNeighborsIterRelease(gvizGraphViewNeighborsIter *iter) {
  if (!iter) {
    return;
  }
  iter->view = NULL;
  iter->i = 0;
  iter->edgeBase = 0;
  iter->neighborN = 0;
}

size_t gvizGraphViewDegree(gvizGraphView *view, size_t u) {
  gvizGraphViewNeighborsIter it;
  if (gvizGraphViewNeighborsIterInit(&it, view, u) != 0) {
    return 0;
  }
  size_t out;
  size_t count = 0;
  while (gvizGraphViewNeighborsIterNext(&it, &out)) {
    count++;
  }
  gvizGraphViewNeighborsIterRelease(&it);
  return count;
}

static int viewEnsureVertexMask(gvizGraphView *view, int initialAllSet) {
  if (view->vertexMask) {
    return 0;
  }
  size_t n = view->graph->vertices.count;
  if (n == 0) {
    return 0;
  }
  view->vertexMask = gvizBitArrayAlloc(n);
  if (!view->vertexMask) {
    return -1;
  }
  if (initialAllSet) {
    for (size_t i = 0; i < n; i++) {
      gvizSetBit(view->vertexMask, i);
    }
  }
  return 0;
}

static int viewEnsureEdgeMask(gvizGraphView *view, int initialAllSet) {
  if (viewEnsureEdgeStart(view) != 0) {
    return -1;
  }
  if (view->edgeMask) {
    return 0;
  }
  size_t total = view->edgeStart[view->graph->vertices.count];
  if (total == 0) {
    return 0;
  }
  view->edgeMask = gvizBitArrayAlloc(total);
  if (!view->edgeMask) {
    return -1;
  }
  if (initialAllSet) {
    size_t units = GVIZ_ARRAY_UNITS(total);
    memset(view->edgeMask, 0xFF, units * sizeof(GVIZ_BIT_UNIT));
    size_t leftover = total % GVIZ_BITS_PER_UNIT;
    if (leftover != 0) {
      GVIZ_BIT_UNIT mask = (GVIZ_BIT_UNIT)((1u << leftover) - 1u);
      view->edgeMask[units - 1] = mask;
    }
  }
  return 0;
}

int gvizGraphViewAddVertex(gvizGraphView *view, size_t u) {
  if (!view || !view->graph) {
    return -1;
  }
  size_t n = view->graph->vertices.count;
  if (u >= n) {
    return -1;
  }
  if (!view->vertexMask) {
    if (viewEnsureVertexMask(view, 1) != 0) {
      return -1;
    }
  }
  if (!gvizTestBit(view->vertexMask, u)) {
    gvizSetBit(view->vertexMask, u);
    view->count++;
  }
  view->edgeStartStale = 1;
  return 0;
}

int gvizGraphViewRemoveVertex(gvizGraphView *view, size_t u) {
  if (!view || !view->graph) {
    return -1;
  }
  size_t n = view->graph->vertices.count;
  if (u >= n) {
    return -1;
  }
  if (!view->vertexMask) {
    if (viewEnsureVertexMask(view, 1) != 0) {
      return -1;
    }
  }
  if (gvizTestBit(view->vertexMask, u)) {
    gvizClearBit(view->vertexMask, u);
    if (view->count > 0) {
      view->count--;
    }
  }
  view->edgeStartStale = 1;
  return 0;
}

int gvizGraphViewAddEdge(gvizGraphView *view, size_t u, size_t v) {
  if (!view || !view->graph) {
    return -1;
  }
  size_t n = view->graph->vertices.count;
  if (u >= n || v >= n) {
    return -1;
  }
  gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, u);
  if (!adj) {
    return -1;
  }
  int found = 0;
  size_t idx = neighborIndex(adj, v, &found);
  if (!found) {
    return -1;
  }
  if (viewEnsureEdgeMask(view, 0) != 0) {
    return -1;
  }
  size_t bit = view->edgeStart[u] + idx;
  gvizSetBit(view->edgeMask, bit);
  return 0;
}

int gvizGraphViewRemoveEdge(gvizGraphView *view, size_t u, size_t v) {
  if (!view || !view->graph) {
    return -1;
  }
  size_t n = view->graph->vertices.count;
  if (u >= n || v >= n) {
    return -1;
  }
  gvizArray *adj = gvizGraphGetVertexNeighbors(view->graph, u);
  if (!adj) {
    return -1;
  }
  int found = 0;
  size_t idx = neighborIndex(adj, v, &found);
  if (!found) {
    return -1;
  }
  if (viewEnsureEdgeMask(view, 1) != 0) {
    return -1;
  }
  size_t bit = view->edgeStart[u] + idx;
  gvizClearBit(view->edgeMask, bit);
  return 0;
}

void gvizGraphViewRelease(gvizGraphView *view) {
  if (!view) {
    return;
  }
  if (view->vertexMask) {
    gvizBitArrayFree(view->vertexMask);
    view->vertexMask = NULL;
  }
  if (view->edgeMask) {
    gvizBitArrayFree(view->edgeMask);
    view->edgeMask = NULL;
  }
  if (view->edgeStart) {
    GVIZ_DEALLOC(view->edgeStart);
    view->edgeStart = NULL;
  }
  view->graph = NULL;
  view->count = 0;
  view->edgeStartStale = 0;
}

int gvizGraphViewKNearestNeighbors(gvizGraphView *view, gvizFoundVertex *out,
                                   size_t k, size_t source,
                                   GVIZ_BIT_ARRAY filter) {
  if (!view || !view->graph || k == 0)
    return 0;
  size_t N = view->graph->vertices.count;
  if (source >= N)
    return -1;

  gvizDeque queue;
  if (gvizDequeInit(&queue, sizeof(gvizFoundVertex)) < 0)
    return -1;

  GVIZ_BIT_ARRAY seen = gvizBitArrayAlloc(N);
  if (!seen) {
    gvizDequeRelease(&queue);
    return -1;
  }
  gvizSetBit(seen, source);

  gvizFoundVertex curr = {source, 0};
  if (gvizDequePush(&queue, &curr) < 0) {
    gvizBitArrayFree(seen);
    gvizDequeRelease(&queue);
    return -1;
  }

  size_t count = 0;
  while (!gvizDequeIsEmpty(&queue)) {
    gvizDequePopLeft(&queue, &curr);

    gvizGraphViewNeighborsIter nit;
    gvizGraphViewNeighborsIterInit(&nit, view, curr.v);
    size_t nb;
    while (gvizGraphViewNeighborsIterNext(&nit, &nb)) {
      if (gvizTestBit(seen, nb))
        continue;
      gvizSetBit(seen, nb);

      gvizFoundVertex next = {nb, curr.dist + 1};

      if (!filter || gvizTestBit(filter, nb)) {
        out[count++] = next;
        if (count >= k) {
          gvizGraphViewNeighborsIterRelease(&nit);
          gvizBitArrayFree(seen);
          gvizDequeRelease(&queue);
          return (int)k;
        }
      }

      if (gvizDequePush(&queue, &next) < 0) {
        gvizGraphViewNeighborsIterRelease(&nit);
        gvizBitArrayFree(seen);
        gvizDequeRelease(&queue);
        return -1;
      }
    }
    gvizGraphViewNeighborsIterRelease(&nit);
  }

  gvizBitArrayFree(seen);
  gvizDequeRelease(&queue);
  return (int)count;
}
