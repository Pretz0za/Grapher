#include "renderer/embeddings/gvizSchnyderWood.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizDeque.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Internal helpers
 * ----------------------------------------------------------------------- */

/*
 * Third vertex of the triangular face traversed by dart u→v.
 *
 * Uses the same rotation as gvizPlanarEmbeddingFaces:
 *   dart u→v  →  next dart v→w  where w = prevNeighbor(v, u)
 *            (i.e. the neighbour just before u in v's CCW adjacency list).
 */
static size_t swFaceThird(const gvizGraph *g, size_t u, size_t v) {
  gvizArray *nb = gvizGraphGetVertexNeighbors(g, v);
  int idx = gvizArrayFindOne(nb, &u);
  assert(idx >= 0);
  size_t prev = ((size_t)idx == 0 ? nb->count : (size_t)idx) - 1;
  return *(size_t *)gvizArrayAtIndex(nb, prev);
}

/* Returns non-zero if edge (u, v) exists in g. */
static int swHasEdge(const gvizGraph *g, size_t u, size_t v) {
  gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
  return nb && gvizArrayFindOne(nb, &v) >= 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

int gvizSchnyderWoodInit(gvizSchnyderWood *sw, const gvizGraph *g) {
  if (!sw || !g)
    return -1;

  size_t N = g->vertices.count;
  sw->n = N;

  if (N < 3)
    return -1;

  /* Allocate parent arrays, all initialised to GVIZ_SW_NONE. */
  for (int c = 0; c < 3; c++) {
    sw->parent[c] = (size_t *)GVIZ_ALLOC(N * sizeof(size_t));
    if (!sw->parent[c]) {
      for (int j = 0; j < c; j++) {
        GVIZ_DEALLOC(sw->parent[j]);
        sw->parent[j] = NULL;
      }
      return -1;
    }
    for (size_t i = 0; i < N; i++)
      sw->parent[c][i] = GVIZ_SW_NONE;
  }

  /*
   * Identify the outer-face triangle (s0, s1, s2):
   *
   *   s0 = vertex 0
   *   s1 = first neighbour of s0 in its adjacency list
   *   s2 = third vertex of the face traced by dart s0→s1
   *
   * Dart s0→s1 traces one of the two faces incident to edge s0-s1.
   * That face becomes the "outer" triangle — s2 is the root of T2, while
   * s0 and s1 are the roots of T0 and T1 respectively.
   * The opposite face (traced by dart s1→s0) contains the first interior
   * vertex to be processed.
   */
  gvizArray *nb0 = gvizGraphGetVertexNeighbors(g, 0);
  if (!nb0 || nb0->count == 0) {
    gvizSchnyderWoodRelease(sw);
    return -1;
  }
  size_t s0 = 0;
  size_t s1 = *(size_t *)gvizArrayAtIndex(nb0, 0);
  size_t s2 = swFaceThird(g, s0, s1);

  sw->root[0] = s0;
  sw->root[1] = s1;
  sw->root[2] = s2;

  /* Trivial case: only the outer triangle, no interior vertices. */
  if (N == 3) {
    sw->parent[0][s2] = s0;
    sw->parent[1][s2] = s1;
    return 0;
  }

  /* --- Allocate working storage --- */
  size_t *bnext = (size_t *)GVIZ_ALLOC(N * sizeof(size_t));
  size_t *bprev = (size_t *)GVIZ_ALLOC(N * sizeof(size_t));
  char *done = (char *)GVIZ_ALLOC(N * sizeof(char));
  if (!bnext || !bprev || !done) {
    GVIZ_DEALLOC(bnext);
    GVIZ_DEALLOC(bprev);
    GVIZ_DEALLOC(done);
    gvizSchnyderWoodRelease(sw);
    return -1;
  }
  for (size_t v = 0; v < N; v++) {
    bnext[v] = GVIZ_SW_NONE;
    bprev[v] = GVIZ_SW_NONE;
    done[v] = 0;
  }

  /* Boundary path starts as  s0 ↔ s1  (the base edge). */
  bnext[s0] = s1;
  bprev[s1] = s0;
  done[s0] = 1;
  done[s1] = 1;

  /*
   * Process N-3 interior vertices (all vertices except s0, s1, s2) in
   * canonical order using a boundary-edge scan.
   *
   * At each step:
   *   1. Walk the boundary left-to-right and find a boundary edge (L, R)
   *      whose interior face vertex w = swFaceThird(g, R, L) is unprocessed
   *      and is not s2.
   *   2. Extend the attachment arc leftward while bprev[Lp] is a neighbour
   *      of w, and rightward while bnext[Rp] is a neighbour of w, giving
   *      the full consecutive arc [Lp … Rp] of w's boundary neighbours.
   *   3. Assign:
   *        parent[0][w]  = Lp   (T0 edge: w→Lp, toward s0)
   *        parent[1][w]  = Rp   (T1 edge: w→Rp, toward s1)
   *        parent[2][ci] = w    for every "hidden" boundary vertex ci
   *                              strictly between Lp and Rp (T2 edge: ci→w)
   *   4. Remove hidden vertices from boundary; insert w between Lp and Rp.
   */
  size_t remaining = N - 3;

  while (remaining > 0) {
    size_t w = GVIZ_SW_NONE;
    size_t found_L = GVIZ_SW_NONE;

    /* Scan boundary edges from s0 toward s1 for an eligible candidate. */
    size_t L = s0;
    while (bnext[L] != GVIZ_SW_NONE) {
      size_t R = bnext[L];
      size_t cand = swFaceThird(g, R, L);
      if (cand != s2 && !done[cand]) {
        w = cand;
        found_L = L;
        break;
      }
      L = R;
    }

    /* For a valid triangulated planar graph this must always succeed. */
    assert(w != GVIZ_SW_NONE);
    if (w == GVIZ_SW_NONE)
      break; /* defensive: guard for release builds */

    size_t found_R = bnext[found_L];

    /* Extend the attachment arc leftward. */
    size_t Lp = found_L;
    while (bprev[Lp] != GVIZ_SW_NONE && swHasEdge(g, w, bprev[Lp]))
      Lp = bprev[Lp];

    /* Extend the attachment arc rightward. */
    size_t Rp = found_R;
    while (bnext[Rp] != GVIZ_SW_NONE && swHasEdge(g, w, bnext[Rp]))
      Rp = bnext[Rp];

    /* Assign T0 and T1 parents for w. */
    sw->parent[0][w] = Lp;
    sw->parent[1][w] = Rp;

    /* "Hidden" boundary vertices strictly between Lp and Rp become T2
     * children of w; remove them from the boundary linked list. */
    size_t c = bnext[Lp];
    while (c != Rp) {
      sw->parent[2][c] = w;
      size_t nc = bnext[c];
      bnext[c] = GVIZ_SW_NONE;
      bprev[c] = GVIZ_SW_NONE;
      c = nc;
    }

    /* Insert w into the boundary between Lp and Rp. */
    bnext[Lp] = w;
    bprev[w] = Lp;
    bnext[w] = Rp;
    bprev[Rp] = w;

    done[w] = 1;
    remaining--;
  }

  /*
   * Final step — add s2 (the T2 root):
   *
   * Any boundary vertex still between s0 and s1 that was not covered by
   * an earlier step becomes a T2 child of s2.
   *
   * s2's own tree parents:
   *   parent[0][s2] = s0   outer-triangle edge s2→s0 belongs to T0
   *   parent[1][s2] = s1   outer-triangle edge s2→s1 belongs to T1
   */
  size_t cv = bnext[s0];
  while (cv != s1 && cv != GVIZ_SW_NONE) {
    sw->parent[2][cv] = s2;
    cv = bnext[cv];
  }
  sw->parent[0][s2] = s0;
  sw->parent[1][s2] = s1;

  GVIZ_DEALLOC(bnext);
  GVIZ_DEALLOC(bprev);
  GVIZ_DEALLOC(done);

  return 0;
}

void gvizSchnyderWoodRelease(gvizSchnyderWood *sw) {
  if (!sw)
    return;
  for (int c = 0; c < 3; c++) {
    GVIZ_DEALLOC(sw->parent[c]);
    sw->parent[c] = NULL;
  }
  sw->n = 0;
}

size_t nextNeighborIdx(const gvizGraph *g, size_t v, size_t u) {
  printf("finding next vertex in %zu adjacency list after %zu.\n", v, u);
  gvizArray *neighbors = gvizGraphGetVertexNeighbors(g, v);
  size_t idx = gvizArrayFindOne(neighbors, &u);
  printf("u index: %zu, adjacency list length: %zu.\n", idx, neighbors->count);
  assert(idx >= 0);
  return (++idx == neighbors->count ? 0 : idx);
}

// Given a vertex inside on region, counts all the other vertices in the same
// region. Performs a bfs from that vertex, stopping at any vertex along the
// boundary all roots and the the partition vertex bits must all be set
// v must not be any of the roots, or the partition vertex
size_t verticesInRegion(const gvizGraph *g, size_t v,
                        GVIZ_BIT_ARRAY pathVertices) {
  int err;

  // init queue
  gvizDeque queue;
  err = gvizDequeInit(&queue, sizeof(size_t));
  if (err < 0)
    return -1;

  // seen bitset over global vertex IDs
  GVIZ_BIT_UNIT seen[GVIZ_ARRAY_UNITS(g->vertices.count)];
  memset(seen, 0, sizeof(seen));
  gvizSetBit(seen, v);
  size_t count = 1;

  // global-id -> index in 'out'
  gvizArray invMap;
  gvizArrayInitAtCapacity(&invMap, sizeof(size_t), g->vertices.count);
  memset(invMap.arr, 0, sizeof(size_t) * invMap.capacity);

  err = gvizDequePush(&queue, &v);
  if (err < 0)
    return -1;

  // BFS
  while (!gvizDequeIsEmpty(&queue)) {
    size_t curr;
    gvizDequePopLeft(&queue, &curr);

    gvizArray *currNeighbors = gvizGraphGetVertexNeighbors(g, curr);

    for (size_t i = 0; i < currNeighbors->count; i++) {
      size_t currNeighbor = *(size_t *)gvizArrayAtIndex(currNeighbors, i);

      // seen keyed by vertex ID or on the boundary
      if (gvizTestBit(seen, currNeighbor) ||
          gvizTestBit(pathVertices, currNeighbor))
        continue;
      gvizSetBit(seen, currNeighbor);

      count++;

      err = gvizDequePush(&queue, &currNeighbor);
      if (err < 0)
        return -1;
    }
  }

  gvizDequeRelease(&queue);
  return count;
}

int findVertexInsideFace(const gvizGraph *g, const gvizArray *face,
                         const GVIZ_BIT_ARRAY faceVertices) {
  for (size_t i = 0; i < face->count; i++) {
    size_t next = (i + 1) % face->count;
    size_t v = *(size_t *)gvizArrayAtIndex(face, i);
    size_t u = *(size_t *)gvizArrayAtIndex(face, next);
    gvizArray *neighbors = gvizGraphGetVertexNeighbors(g, v);
    size_t potentialInside = nextNeighborIdx(g, v, u);
    printf("next vertex index: %zu, ", potentialInside);
    potentialInside = *(size_t *)gvizArrayAtIndex(neighbors, potentialInside);
    printf("next vertex: %zu\n", potentialInside);
    if (!gvizTestBit(faceVertices, potentialInside))
      return (int)potentialInside;
  }
  // no vertices inside the face
  return -1;
}

// returns the cycle surrounding the region with the same color as path1
gvizArray getRegionBoundary(size_t partitionV, const gvizArray *path1,
                            const gvizArray *path2) {
  gvizArray out;
  gvizArrayInitAtCapacity(&out, sizeof(size_t),
                          path1->count + path2->count + 1);

  gvizArrayCopy(&out, path2);

  // copy in reverse order
  for (size_t i = path1->count; i-- > 0;) {
    gvizArrayPush(&out, gvizArrayAtIndex(path1, i));
  }

  gvizArrayPush(&out, &partitionV);

  return out;
}

void gvizSchnyderWoodEmbed(const gvizSchnyderWood *sw,
                           gvizEmbeddedGraph *embedding) {
#define IS_ROOT(i) (i == sw->root[0] || i == sw->root[1] || i == sw->root[2])

  gvizArray paths[3];
  for (size_t r = 0; r < 3; r++) {
    gvizArrayInit(paths + r, sizeof(size_t));
  }

  for (size_t i = 0; i < sw->n; i++) {
    if (IS_ROOT(i))
      continue;

    for (size_t r = 0; r < 3; r++) {
      paths[r].count = 0;
    }

    GVIZ_BIT_UNIT pathVertices[GVIZ_ARRAY_UNITS(sw->n)];
    memset(pathVertices, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(sw->n));
    gvizSetBit(pathVertices, i);

    size_t curr;
    for (size_t r = 0; r < 3; r++) {
      curr = sw->parent[r][i];
      while (1) {
        gvizSetBit(pathVertices, curr);
        gvizArrayPush(paths + r, &curr);

        if (curr == sw->root[r])
          break;
        curr = sw->parent[r][curr];
      }
    }

    double coordinates[3];
    for (size_t r = 0; r < 3; r++) {
      // TODO: fix this. finding previous neighbor is not enough

      gvizArray boundary = getRegionBoundary(i, &paths[r], &paths[(r + 1) % 3]);
      int v = findVertexInsideFace(embedding->graph, &boundary, pathVertices);

      size_t count = 0;
      if (v != -1 && !(gvizTestBit(pathVertices, v))) {
        count = verticesInRegion(embedding->graph, v, pathVertices);
      }
      count += paths[r].count;
      coordinates[r] = (double)count;
    }

    printf("counts for vertex %zu: (%f, %f, %f)\n", i, coordinates[0],
           coordinates[1], coordinates[2]);

    // will only copy the first two, since embedding is two dimensional
    gvizEmbeddedGraphSetVPosition(embedding, i, coordinates);
  }

  for (size_t r = 0; r < 3; r++) {
    gvizArrayRelease(paths + r);
  }

  return;
}
