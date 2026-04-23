#include "renderer/embeddings/gvizTutteSolveEmbedding.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "lapacke.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_TUTTE_SOLVE_DEFAULT_EPSILON 1e-10

static size_t numVertices(const gvizTutteSolveState *s) {
    return ((gvizEmbeddedGraph *)s)->graph->vertices.count;
}

static size_t dim(const gvizTutteSolveState *s) {
    return ((gvizEmbeddedGraph *)s)->embedding.dim;
}

static double *livePos(gvizTutteSolveState *s, size_t u) {
    return gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)s, u);
}

static void setPos(gvizTutteSolveState *s, size_t u, double *p) {
    gvizEmbeddedGraphSetVPosition((gvizEmbeddedGraph *)s, u, p);
}

/* ---- init / release ------------------------------------------------------- */

int gvizTutteSolveEmbeddingInit(gvizTutteSolveState *s, gvizGraph *g,
                                size_t dimension, double epsilon) {
    int res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)s, g, dimension);
    if (res < 0)
        return res;

    size_t N = g->vertices.count;

    s->boundary = NULL;
    s->boundaryCount = 0;

    s->isBoundary = GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));
    if (!s->isBoundary)
        return -1;
    memset(s->isBoundary, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));

    s->matLII = NULL;
    s->rhs = NULL;
    s->interiorIdx = NULL;
    s->vertexToInterior = NULL;
    s->numInterior = 0;
    s->matrixReady = 0;

    s->iteration = 0;
    s->lastMaxDelta = 0.0;
    s->converged = 0;
    s->epsilon = (epsilon > 0.0) ? epsilon : GVIZ_TUTTE_SOLVE_DEFAULT_EPSILON;

    return 0;
}

void gvizTutteSolveEmbeddingRelease(gvizTutteSolveState *s) {
    if (s->boundary)         GVIZ_DEALLOC(s->boundary);
    if (s->isBoundary)       GVIZ_DEALLOC(s->isBoundary);
    if (s->matLII)           GVIZ_DEALLOC(s->matLII);
    if (s->rhs)              GVIZ_DEALLOC(s->rhs);
    if (s->interiorIdx)      GVIZ_DEALLOC(s->interiorIdx);
    if (s->vertexToInterior) GVIZ_DEALLOC(s->vertexToInterior);
    gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)s);
}

/* ---- boundary setup ------------------------------------------------------- */

int gvizTutteSolveEmbeddingSetBoundary(gvizTutteSolveState *s,
                                       const size_t *boundary, size_t count,
                                       const double *positions) {
    if (count < 3)
        return -1;

    size_t N = numVertices(s);
    size_t d = dim(s);

    for (size_t i = 0; i < count; i++)
        if (boundary[i] >= N)
            return -1;

    s->boundary = GVIZ_ALLOC(sizeof(size_t) * count);
    if (!s->boundary)
        return -1;
    memcpy(s->boundary, boundary, sizeof(size_t) * count);
    s->boundaryCount = count;

    for (size_t i = 0; i < count; i++) {
        gvizSetBit(s->isBoundary, boundary[i]);
        setPos(s, boundary[i], (double *)(positions + i * d));
    }

    return gvizTutteSolveEmbeddingBuildMatrix(s);
}

void gvizTutteSolveFixConvexPolygon(gvizTutteSolveState *s,
                                    const size_t *boundary, size_t count,
                                    double radius) {
    size_t d = dim(s);
    double positions[count * d];
    memset(positions, 0, sizeof(double) * count * d);

    for (size_t k = 0; k < count; k++) {
        double angle = 2.0 * M_PI * (double)k / (double)count;
        positions[k * d + 0] = radius * cos(angle);
        if (d > 1)
            positions[k * d + 1] = radius * sin(angle);
    }

    gvizTutteSolveEmbeddingSetBoundary(s, boundary, count, positions);
}

/* ---- matrix build --------------------------------------------------------- */

int gvizTutteSolveEmbeddingBuildMatrix(gvizTutteSolveState *s) {
    gvizGraph *g = ((gvizEmbeddedGraph *)s)->graph;
    size_t N = numVertices(s);
    size_t d = dim(s);

    if (s->matLII)           { GVIZ_DEALLOC(s->matLII);           s->matLII = NULL; }
    if (s->rhs)              { GVIZ_DEALLOC(s->rhs);              s->rhs = NULL; }
    if (s->interiorIdx)      { GVIZ_DEALLOC(s->interiorIdx);      s->interiorIdx = NULL; }
    if (s->vertexToInterior) { GVIZ_DEALLOC(s->vertexToInterior); s->vertexToInterior = NULL; }
    s->matrixReady = 0;

    s->vertexToInterior = GVIZ_ALLOC(sizeof(size_t) * N);
    if (!s->vertexToInterior)
        return -1;

    size_t NI = 0;
    for (size_t u = 0; u < N; u++) {
        if (!gvizTestBit(s->isBoundary, u))
            s->vertexToInterior[u] = NI++;
        else
            s->vertexToInterior[u] = SIZE_MAX;
    }
    s->numInterior = NI;

    if (NI == 0)
        return 0;

    s->interiorIdx = GVIZ_ALLOC(sizeof(size_t) * NI);
    if (!s->interiorIdx) return -1;
    for (size_t u = 0, i = 0; u < N; u++)
        if (s->vertexToInterior[u] != SIZE_MAX)
            s->interiorIdx[i++] = u;

    s->matLII = GVIZ_ALLOC(sizeof(double) * NI * NI);
    s->rhs    = GVIZ_ALLOC(sizeof(double) * NI * d);
    if (!s->matLII || !s->rhs)
        return -1;

    memset(s->matLII, 0, sizeof(double) * NI * NI);

    /* L_II: diagonal = total degree; off-diagonal = -1 for interior-interior edges.
       Solve system: L_II * x_I = A_IB * x_B (each boundary neighbor contributes
       its position to the rhs). */
    for (size_t i = 0; i < NI; i++) {
        size_t u = s->interiorIdx[i];
        gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
        s->matLII[i * NI + i] = (double)nb->count;
        for (size_t j = 0; j < nb->count; j++) {
            size_t v = *(size_t *)gvizArrayAtIndex(nb, j);
            size_t vi = s->vertexToInterior[v];
            if (vi != SIZE_MAX)
                s->matLII[i * NI + vi] = -1.0;
        }
    }

    lapack_int info = LAPACKE_dpotrf(LAPACK_ROW_MAJOR, 'U',
                                     (lapack_int)NI, s->matLII, (lapack_int)NI);
    if (info != 0)
        return -1;

    s->matrixReady = 1;
    s->converged = 0;
    return 0;
}

/* ---- step ----------------------------------------------------------------- */

double gvizTutteSolveEmbeddingStep(gvizTutteSolveState *s, double dt) {
    (void)dt;

    if (!s->matrixReady)
        return 0.0;

    gvizGraph *g = ((gvizEmbeddedGraph *)s)->graph;
    size_t NI = s->numInterior;
    size_t d = dim(s);

    /* Build rhs: b[i*d + k] = sum of boundary-neighbor positions for dim k. */
    memset(s->rhs, 0, sizeof(double) * NI * d);
    for (size_t i = 0; i < NI; i++) {
        size_t u = s->interiorIdx[i];
        gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
        for (size_t j = 0; j < nb->count; j++) {
            size_t v = *(size_t *)gvizArrayAtIndex(nb, j);
            if (gvizTestBit(s->isBoundary, v)) {
                double *vp = livePos(s, v);
                for (size_t k = 0; k < d; k++)
                    s->rhs[i * d + k] += vp[k];
            }
        }
    }

    /* Solve L_II * X = rhs; rhs is overwritten with the solution (row-major NI×d). */
    lapack_int info = LAPACKE_dpotrs(LAPACK_ROW_MAJOR, 'U',
                                     (lapack_int)NI, (lapack_int)d,
                                     s->matLII, (lapack_int)NI,
                                     s->rhs, (lapack_int)d);
    if (info != 0)
        return 0.0;

    double maxDelta = 0.0;
    for (size_t i = 0; i < NI; i++) {
        double *old = livePos(s, s->interiorIdx[i]);
        double newp[d];
        double sq = 0.0;
        for (size_t k = 0; k < d; k++) {
            newp[k] = s->rhs[i * d + k];
            double diff = newp[k] - old[k];
            sq += diff * diff;
        }
        setPos(s, s->interiorIdx[i], newp);
        double l2 = sqrt(sq);
        if (l2 > maxDelta)
            maxDelta = l2;
    }

    s->iteration++;
    s->lastMaxDelta = maxDelta;
    s->converged = (maxDelta < s->epsilon) ? 1 : 0;

    return maxDelta;
}

int gvizTutteSolveEmbeddingRun(gvizTutteSolveState *s, size_t maxIters) {
    if (!s || !s->matrixReady)
        return -1;

    while (!s->converged && s->iteration < maxIters)
        gvizTutteSolveEmbeddingStep(s, 1.0);

    return (int)s->iteration;
}
