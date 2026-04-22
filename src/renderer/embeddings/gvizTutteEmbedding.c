#include "renderer/embeddings/gvizTutteEmbedding.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static size_t numVertices(const gvizTutteState *s) {
    return ((gvizEmbeddedGraph *)s)->graph->vertices.count;
}

static size_t dim(const gvizTutteState *s) {
    return ((gvizEmbeddedGraph *)s)->embedding.dim;
}

static double *livePos(gvizTutteState *s, size_t u) {
    return gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)s, u);
}

static void setPos(gvizTutteState *s, size_t u, double *p) {
    gvizEmbeddedGraphSetVPosition((gvizEmbeddedGraph *)s, u, p);
}

/* ---- init / release ------------------------------------------------------- */

int gvizTutteEmbeddingInit(gvizTutteState *s, gvizGraph *g, size_t dimension,
                           double epsilon) {
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

    s->scratch = GVIZ_ALLOC(sizeof(double) * N * dimension);
    if (!s->scratch)
        return -1;
    memset(s->scratch, 0, sizeof(double) * N * dimension);

    s->iteration = 0;
    s->lastMaxDelta = 0.0;
    s->converged = 0;
    s->useGaussSeidel = 0;
    s->epsilon = (epsilon > 0.0) ? epsilon : GVIZ_TUTTE_DEFAULT_EPSILON;
    s->relaxationRate = 5.0;

    return 0;
}

void gvizTutteEmbeddingRelease(gvizTutteState *s) {
    if (s->boundary)   GVIZ_DEALLOC(s->boundary);
    if (s->isBoundary) GVIZ_DEALLOC(s->isBoundary);
    if (s->scratch)    GVIZ_DEALLOC(s->scratch);
    gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)s);
}

/* ---- boundary setup ------------------------------------------------------- */

int gvizTutteEmbeddingSetBoundary(gvizTutteState *s, const size_t *boundary,
                                  size_t count, const double *positions) {
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

    return 0;
}

void gvizTutteEmbeddingSeedInterior(gvizTutteState *s) {
    size_t N = numVertices(s);
    size_t d = dim(s);

    double centroid[d];
    memset(centroid, 0, sizeof(double) * d);

    for (size_t i = 0; i < s->boundaryCount; i++) {
        double *p = livePos(s, s->boundary[i]);
        for (size_t k = 0; k < d; k++)
            centroid[k] += p[k];
    }
    if (s->boundaryCount > 0)
        for (size_t k = 0; k < d; k++)
            centroid[k] /= (double)s->boundaryCount;

    for (size_t u = 0; u < N; u++)
        if (!gvizTestBit(s->isBoundary, u))
            setPos(s, u, centroid);

    s->iteration = 0;
    s->lastMaxDelta = 0.0;
    s->converged = 0;
}

void gvizTutteFixConvexPolygon(gvizTutteState *s, const size_t *boundary,
                               size_t count, double radius) {
    size_t d = dim(s);
    double positions[count * d];
    memset(positions, 0, sizeof(double) * count * d);

    for (size_t k = 0; k < count; k++) {
        double angle = 2.0 * M_PI * (double)k / (double)count;
        positions[k * d + 0] = radius * cos(angle);
        if (d > 1)
            positions[k * d + 1] = radius * sin(angle);
    }

    gvizTutteEmbeddingSetBoundary(s, boundary, count, positions);
}

/* ---- step ----------------------------------------------------------------- */

/* Snapshot interior positions into scratch so Jacobi reads a consistent generation. */
static void snapshotInterior(gvizTutteState *s) {
    size_t N = numVertices(s);
    size_t d = dim(s);
    for (size_t u = 0; u < N; u++) {
        if (!gvizTestBit(s->isBoundary, u))
            memcpy(s->scratch + u * d, livePos(s, u), sizeof(double) * d);
    }
}

/* Returns a pointer to vertex v's position to use as a neighbor read source. */
static double *neighborReadPos(gvizTutteState *s, size_t v) {
    if (!s->useGaussSeidel && !gvizTestBit(s->isBoundary, v))
        return s->scratch + v * dim(s);
    return livePos(s, v);
}

/* Computes the unweighted barycenter of u's neighbors into out[dim]. */
static void computeBarycenter(gvizTutteState *s, size_t u, double *out) {
    gvizGraph *g = ((gvizEmbeddedGraph *)s)->graph;
    gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
    size_t d = dim(s);
    memset(out, 0, sizeof(double) * d);
    for (size_t j = 0; j < nb->count; j++) {
        size_t v = *(size_t *)gvizArrayAtIndex(nb, j);
        double *vp = neighborReadPos(s, v);
        for (size_t k = 0; k < d; k++)
            out[k] += vp[k];
    }
    for (size_t k = 0; k < d; k++)
        out[k] /= (double)nb->count;
}

/* Moves u by alpha toward its barycenter; returns L2 displacement. */
static double relaxVertex(gvizTutteState *s, size_t u, double alpha) {
    gvizArray *nb = gvizGraphGetVertexNeighbors(((gvizEmbeddedGraph *)s)->graph, u);
    if (nb->count == 0)
        return 0.0;

    size_t d = dim(s);
    double bary[d];
    computeBarycenter(s, u, bary);

    double *old = livePos(s, u);
    double newp[d];
    double delta = 0.0;
    for (size_t k = 0; k < d; k++) {
        newp[k] = old[k] + alpha * (bary[k] - old[k]);
        double diff = newp[k] - old[k];
        delta += diff * diff;
    }

    setPos(s, u, newp);
    return sqrt(delta);
}

double gvizTutteEmbeddingStep(gvizTutteState *s, double dt) {
    size_t N = numVertices(s);

    double alpha = s->relaxationRate * dt;
    if (alpha <= 0.0) return 0.0;
    if (alpha > 1.0)  alpha = 1.0;

    if (!s->useGaussSeidel)
        snapshotInterior(s);

    double maxDelta = 0.0;
    for (size_t u = 0; u < N; u++) {
        if (gvizTestBit(s->isBoundary, u))
            continue;
        double d = relaxVertex(s, u, alpha);
        if (d > maxDelta)
            maxDelta = d;
    }

    s->iteration++;
    s->lastMaxDelta = maxDelta;
    if (maxDelta < s->epsilon)
        s->converged = 1;

    return maxDelta;
}

int gvizTutteEmbeddingRun(gvizTutteState *s, size_t maxIters) {
    if (!s || !s->boundary)
        return -1;

    while (!s->converged && s->iteration < maxIters)
        gvizTutteEmbeddingStep(s, 1.0);

    return (int)s->iteration;
}
