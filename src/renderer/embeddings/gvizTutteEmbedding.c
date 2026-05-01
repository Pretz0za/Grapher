#include "renderer/embeddings/gvizTutteEmbedding.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizGraphView.h"
#include "cblas.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t numVertices(const gvizTutteState *s) {
    return ((gvizEmbeddedGraph *)s)->view.graph->vertices.count;
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

int gvizTutteEmbeddingInitView(gvizTutteState *s, gvizGraphView view,
                               size_t dimension, double epsilon) {
    size_t N = view.graph->vertices.count;
    int res = gvizEmbeddedGraphInitView((gvizEmbeddedGraph *)s, view,
                                        GVIZ_EMBED_FULL_GRAPH, dimension);
    if (res < 0)
        return res;

    s->boundary = NULL;
    s->boundaryCount = 0;

    s->isBoundary = GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));
    if (!s->isBoundary)
        return -1;
    memset(s->isBoundary, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));

    s->matII = NULL;
    s->rhs = NULL;
    s->xiBuf = NULL;
    s->baryBuf = NULL;
    s->interiorIdx = NULL;
    s->vertexToInterior = NULL;
    s->numInterior = 0;
    s->matrixBuilt = 0;

    s->iteration = 0;
    s->lastMaxDelta = 0.0;
    s->converged = 0;
    s->epsilon = (epsilon > 0.0) ? epsilon : GVIZ_TUTTE_DEFAULT_EPSILON;

    return 0;
}

void gvizTutteEmbeddingRelease(gvizTutteState *s) {
    if (s->boundary)         GVIZ_DEALLOC(s->boundary);
    if (s->isBoundary)       GVIZ_DEALLOC(s->isBoundary);
    if (s->matII)            GVIZ_DEALLOC(s->matII);
    if (s->rhs)              GVIZ_DEALLOC(s->rhs);
    if (s->xiBuf)            GVIZ_DEALLOC(s->xiBuf);
    if (s->baryBuf)          GVIZ_DEALLOC(s->baryBuf);
    if (s->interiorIdx)      GVIZ_DEALLOC(s->interiorIdx);
    if (s->vertexToInterior) GVIZ_DEALLOC(s->vertexToInterior);
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

    return gvizTutteEmbeddingBuildMatrix(s);
}

void gvizTutteEmbeddingSeedInterior(gvizTutteState *s) {
    gvizGraphView *view = &((gvizEmbeddedGraph *)s)->view;
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

    gvizGraphViewVertexIter vit;
    gvizGraphViewVertexIterInit(&vit, view);
    size_t u;
    while (gvizGraphViewVertexIterNext(&vit, &u)) {
        if (!gvizTestBit(s->isBoundary, u))
            setPos(s, u, centroid);
    }
    gvizGraphViewVertexIterRelease(&vit);

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

/* ---- matrix build --------------------------------------------------------- */

int gvizTutteEmbeddingBuildMatrix(gvizTutteState *s) {
    gvizGraphView *view = &((gvizEmbeddedGraph *)s)->view;
    size_t N = numVertices(s);
    size_t d = dim(s);

    if (s->matII)            { GVIZ_DEALLOC(s->matII);            s->matII = NULL; }
    if (s->rhs)              { GVIZ_DEALLOC(s->rhs);              s->rhs = NULL; }
    if (s->xiBuf)            { GVIZ_DEALLOC(s->xiBuf);            s->xiBuf = NULL; }
    if (s->baryBuf)          { GVIZ_DEALLOC(s->baryBuf);          s->baryBuf = NULL; }
    if (s->interiorIdx)      { GVIZ_DEALLOC(s->interiorIdx);      s->interiorIdx = NULL; }
    if (s->vertexToInterior) { GVIZ_DEALLOC(s->vertexToInterior); s->vertexToInterior = NULL; }
    s->matrixBuilt = 0;

    s->vertexToInterior = GVIZ_ALLOC(sizeof(size_t) * N);
    if (!s->vertexToInterior)
        return -1;
    for (size_t u = 0; u < N; u++)
        s->vertexToInterior[u] = SIZE_MAX;

    size_t NI = 0;
    {
        gvizGraphViewVertexIter vit;
        gvizGraphViewVertexIterInit(&vit, view);
        size_t u;
        while (gvizGraphViewVertexIterNext(&vit, &u)) {
            if (!gvizTestBit(s->isBoundary, u))
                s->vertexToInterior[u] = NI++;
        }
        gvizGraphViewVertexIterRelease(&vit);
    }
    s->numInterior = NI;

    if (NI == 0)
        return 0;

    s->interiorIdx = GVIZ_ALLOC(sizeof(size_t) * NI);
    if (!s->interiorIdx) return -1;
    for (size_t u = 0; u < N; u++) {
        size_t vi = s->vertexToInterior[u];
        if (vi != SIZE_MAX)
            s->interiorIdx[vi] = u;
    }

    s->matII   = GVIZ_ALLOC(sizeof(double) * NI * NI);
    s->rhs     = GVIZ_ALLOC(sizeof(double) * NI * d);
    s->xiBuf   = GVIZ_ALLOC(sizeof(double) * NI);
    s->baryBuf = GVIZ_ALLOC(sizeof(double) * NI);
    if (!s->matII || !s->rhs || !s->xiBuf || !s->baryBuf)
        return -1;

    memset(s->matII, 0, sizeof(double) * NI * NI);
    memset(s->rhs,   0, sizeof(double) * NI * d);

    for (size_t i = 0; i < NI; i++) {
        size_t u = s->interiorIdx[i];
        size_t deg = gvizGraphViewDegree(view, u);
        if (deg == 0)
            continue;
        double inv_deg = 1.0 / (double)deg;
        gvizGraphViewNeighborsIter nit;
        gvizGraphViewNeighborsIterInit(&nit, view, u);
        size_t v;
        while (gvizGraphViewNeighborsIterNext(&nit, &v)) {
            size_t vi = s->vertexToInterior[v];
            if (vi != SIZE_MAX) {
                s->matII[i * NI + vi] = inv_deg;
            } else {
                double *vp = livePos(s, v);
                for (size_t k = 0; k < d; k++)
                    s->rhs[i * d + k] += inv_deg * vp[k];
            }
        }
        gvizGraphViewNeighborsIterRelease(&nit);
    }

    s->matrixBuilt = 1;
    return 0;
}

/* ---- step ----------------------------------------------------------------- */

double gvizTutteEmbeddingStep(gvizTutteState *s) {
    if (!s->matrixBuilt)
        return 0.0;

    size_t NI = s->numInterior;
    size_t d = dim(s);

    /* Accumulate squared displacement per interior vertex across all dims. */
    double deltaSum[NI];
    memset(deltaSum, 0, sizeof(double) * NI);

    for (size_t k = 0; k < d; k++) {
        /* Snapshot x_I for this dimension (Jacobi: read before any writes). */
        for (size_t i = 0; i < NI; i++)
            s->xiBuf[i] = livePos(s, s->interiorIdx[i])[k];

        /* baryBuf = M_II * x_I  (interior-neighbor contribution). */
        cblas_dgemv(CblasRowMajor, CblasNoTrans,
                    (int)NI, (int)NI,
                    1.0, s->matII, (int)NI,
                    s->xiBuf, 1,
                    0.0, s->baryBuf, 1);

        /* baryBuf += rhs[:,k]  (precomputed boundary contribution, stride=d). */
        cblas_daxpy((int)NI, 1.0, s->rhs + k, (int)d, s->baryBuf, 1);

        for (size_t i = 0; i < NI; i++) {
            double diff = s->baryBuf[i] - s->xiBuf[i];
            livePos(s, s->interiorIdx[i])[k] = s->baryBuf[i];
            deltaSum[i] += diff * diff;
        }
    }

    double maxDelta = 0.0;
    for (size_t i = 0; i < NI; i++) {
        double l2 = sqrt(deltaSum[i]);
        if (l2 > maxDelta)
            maxDelta = l2;
    }

    s->iteration++;
    s->lastMaxDelta = maxDelta;
    s->converged = (maxDelta < s->epsilon) ? 1 : 0;

    return maxDelta;
}

int gvizTutteEmbeddingRun(gvizTutteState *s, size_t maxIters) {
    if (!s || !s->matrixBuilt)
        return -1;

    while (!s->converged && s->iteration < maxIters)
        gvizTutteEmbeddingStep(s);

    return (int)s->iteration;
}
