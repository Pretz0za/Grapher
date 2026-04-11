#include "renderer/embeddings/gvizSchnyderWood.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include <assert.h>
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
static size_t swFaceThird(const gvizGraph *g, size_t u, size_t v)
{
    gvizArray *nb  = gvizGraphGetVertexNeighbors(g, v);
    int        idx = gvizArrayFindOne(nb, &u);
    assert(idx >= 0);
    size_t prev = ((size_t)idx == 0 ? nb->count : (size_t)idx) - 1;
    return *(size_t *)gvizArrayAtIndex(nb, prev);
}

/* Returns non-zero if edge (u, v) exists in g. */
static int swHasEdge(const gvizGraph *g, size_t u, size_t v)
{
    gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
    return nb && gvizArrayFindOne(nb, &v) >= 0;
}

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

int gvizSchnyderWoodInit(gvizSchnyderWood *sw, const gvizGraph *g)
{
    if (!sw || !g) return -1;

    size_t N = g->vertices.count;
    sw->n = N;

    if (N < 3) return -1;

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
    char   *done  = (char   *)GVIZ_ALLOC(N * sizeof(char));
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
        done[v]  = 0;
    }

    /* Boundary path starts as  s0 ↔ s1  (the base edge). */
    bnext[s0] = s1;
    bprev[s1] = s0;
    done[s0]  = 1;
    done[s1]  = 1;

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
        size_t w       = GVIZ_SW_NONE;
        size_t found_L = GVIZ_SW_NONE;

        /* Scan boundary edges from s0 toward s1 for an eligible candidate. */
        size_t L = s0;
        while (bnext[L] != GVIZ_SW_NONE) {
            size_t R    = bnext[L];
            size_t cand = swFaceThird(g, R, L);
            if (cand != s2 && !done[cand]) {
                w       = cand;
                found_L = L;
                break;
            }
            L = R;
        }

        /* For a valid triangulated planar graph this must always succeed. */
        assert(w != GVIZ_SW_NONE);
        if (w == GVIZ_SW_NONE) break; /* defensive: guard for release builds */

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
            c        = nc;
        }

        /* Insert w into the boundary between Lp and Rp. */
        bnext[Lp] = w;
        bprev[w]  = Lp;
        bnext[w]  = Rp;
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

void gvizSchnyderWoodRelease(gvizSchnyderWood *sw)
{
    if (!sw) return;
    for (int c = 0; c < 3; c++) {
        GVIZ_DEALLOC(sw->parent[c]);
        sw->parent[c] = NULL;
    }
    sw->n = 0;
}
