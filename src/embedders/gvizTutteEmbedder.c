#include "embedders/gvizTutteEmbedder.h"
#include "core/alloc.h"
#include "ds/gvizArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizPlanarEmbedder.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static size_t numVertices(const gvizTutteState *s) {
    return gvizGraphSize(((gvizEmbeddedGraph *)s)->subgraph.g);
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

static void clearBoundaryBits(gvizTutteState *s) {
    size_t N = numVertices(s);
    memset(s->isBoundary, 0, sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(N));
}

static void tutteActionStep(gvizEmbeddedGraph *embedding, void *userData,
                            const gvizActionPayload *payload) {
    (void)userData;
    gvizTutteState *s = (gvizTutteState *)embedding;
    if (!s->begun || !s->boundary)
        return;
    double dt = payload && payload->deltaTime > 0.0 ? payload->deltaTime : 1.0 / 60.0;
    gvizTutteEmbedderStep(s, dt);
}

static void tutteActionFixOuterFace(gvizEmbeddedGraph *embedding, void *userData,
                                    const gvizActionPayload *payload) {
    (void)userData;
    (void)payload;
    gvizTutteState *s = (gvizTutteState *)embedding;
    gvizTutteEmbedderFixOuterFace(s);
}

int gvizTutteEmbedderInit(gvizTutteState *s, gvizSubgraph subgraph,
                           size_t dimension, double epsilon) {
    memset(s, 0, sizeof(*s));

    int res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)s, subgraph, dimension);
    if (res < 0)
        return res;

    size_t N = gvizGraphSize(subgraph.g);

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

    gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)s;
    if (gvizEmbeddedGraphAddAction(embedding, "tutte.step", tutteActionStep,
                                   NULL) < 0)
        return -1;
    if (gvizEmbeddedGraphAddAction(embedding, "tutte.fixOuterFace",
                                   tutteActionFixOuterFace, NULL) < 0)
        return -1;

    if (!gvizEmbeddedGraphAddStatSeries(embedding, "tutte.maxDelta",
                                        GVIZ_STAT_CHART_LINE_LOG))
        return -1;

    return 0;
}

void gvizTutteEmbedderRelease(gvizTutteState *s) {
    if (s->boundary)
        GVIZ_DEALLOC(s->boundary);
    if (s->isBoundary)
        GVIZ_DEALLOC(s->isBoundary);
    if (s->scratch)
        GVIZ_DEALLOC(s->scratch);
    gvizEmbeddedGraphRelease((gvizEmbeddedGraph *)s);
}

static int boundaryCycleFromSubgraph(const gvizSubgraph *sg, size_t *out,
                                     size_t max, size_t *count) {
    *count = 0;
    gvizPlanarHalfEdge start = {0};
    int found = 0;

    gvizSubgraphVertexIterator vit = gvizSubgraphVertexIteratorCreate(sg);
    size_t u;
    while (gvizSubgraphVertexIterate(&vit, &u)) {
        gvizSubgraphNeighborIterator nit =
            gvizSubgraphNeighborIteratorCreate(sg, u);
        size_t v;
        while (gvizSubgraphNeighborIterate(&nit, &v)) {
            if (!gvizSubgraphHasEdge(sg, u, v))
                continue;
            start.u = u;
            start.v = v;
            found = 1;
            break;
        }
        if (found)
            break;
    }
    if (!found)
        return -1;

    gvizPlanarFaceWalk walk;
    if (gvizPlanarFaceWalkBegin(sg, start, &walk) < 0)
        return -1;

    int step;
    do {
        if (*count >= max)
            return -1;
        step = gvizPlanarFaceWalkStep(&walk, &out[(*count)++]);
    } while (step == 1);

    return (step == 0 && *count >= 3) ? 0 : -1;
}

int gvizTutteEmbedderBegin(gvizTutteState *s) {
    if (!s || dim(s) != 2)
        return -1;

    gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)s;
    int planar = gvizPlanarApplyRotationToEmbedding(embedding);
    if (planar == -2)
        return -2;
    if (planar < 0)
        return -1;

    size_t N = numVertices(s);
    size_t *boundary = GVIZ_ALLOC(sizeof(size_t) * N);
    if (!boundary)
        return -1;

    size_t boundaryCount = 0;
    if (gvizPlanarLargestFaceBoundary(embedding->subgraph.g, &embedding->subgraph,
                                      boundary, N, &boundaryCount) < 0) {
        GVIZ_DEALLOC(boundary);
        return -1;
    }

    gvizTutteFixConvexPolygon(s, boundary, boundaryCount, 200.0);
    GVIZ_DEALLOC(boundary);
    gvizTutteEmbedderSeedInterior(s);
    s->begun = 1;
    return 0;
}

int gvizTutteEmbedderSetBoundary(gvizTutteState *s, const size_t *boundary,
                                  size_t count, const double *positions) {
    if (count < 3)
        return -1;

    size_t N = numVertices(s);
    size_t d = dim(s);

    for (size_t i = 0; i < count; i++)
        if (boundary[i] >= N)
            return -1;

    clearBoundaryBits(s);
    if (s->boundary)
        GVIZ_DEALLOC(s->boundary);

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

void gvizTutteEmbedderSeedInterior(gvizTutteState *s) {
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
    double *positions = GVIZ_ALLOC(sizeof(double) * count * d);
    if (!positions)
        return;
    memset(positions, 0, sizeof(double) * count * d);

    for (size_t k = 0; k < count; k++) {
        double angle = 2.0 * M_PI * (double)k / (double)count;
        positions[k * d + 0] = radius * cos(angle);
        if (d > 1)
            positions[k * d + 1] = radius * sin(angle);
    }

    gvizTutteEmbedderSetBoundary(s, boundary, count, positions);
    GVIZ_DEALLOC(positions);
}

static void snapshotInterior(gvizTutteState *s) {
    size_t N = numVertices(s);
    size_t d = dim(s);
    for (size_t u = 0; u < N; u++) {
        if (!gvizTestBit(s->isBoundary, u))
            memcpy(s->scratch + u * d, livePos(s, u), sizeof(double) * d);
    }
}

static double *neighborReadPos(gvizTutteState *s, size_t v) {
    if (!s->useGaussSeidel && !gvizTestBit(s->isBoundary, v))
        return s->scratch + v * dim(s);
    return livePos(s, v);
}

static void computeBarycenter(gvizTutteState *s, size_t u, double *out) {
    const gvizSubgraph *sg = &((gvizEmbeddedGraph *)s)->subgraph;
    size_t d = dim(s);
    size_t count = 0;
    memset(out, 0, sizeof(double) * d);

    gvizSubgraphNeighborIterator nit = gvizSubgraphNeighborIteratorCreate(sg, u);
    size_t v;
    while (gvizSubgraphNeighborIterate(&nit, &v)) {
        double *vp = neighborReadPos(s, v);
        for (size_t k = 0; k < d; k++)
            out[k] += vp[k];
        count++;
    }
    if (count == 0)
        return;
    for (size_t k = 0; k < d; k++)
        out[k] /= (double)count;
}

static double relaxVertex(gvizTutteState *s, size_t u, double alpha) {
    if (gvizSubgraphDegree(&((gvizEmbeddedGraph *)s)->subgraph, u) == 0)
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

double gvizTutteEmbedderStep(gvizTutteState *s, double dt) {
    size_t N = numVertices(s);

    double alpha = s->relaxationRate * dt;
    if (alpha <= 0.0)
        return 0.0;
    if (alpha > 1.0)
        alpha = 1.0;

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

    gvizEmbeddedGraphStatAppend((gvizEmbeddedGraph *)s, "tutte.maxDelta",
                                maxDelta);

    return maxDelta;
}

int gvizTutteEmbedderRun(gvizTutteState *s, size_t maxIters) {
    if (!s || !s->boundary)
        return -1;

    while (!s->converged && s->iteration < maxIters)
        gvizTutteEmbedderStep(s, 1.0);

    return (int)s->iteration;
}

int gvizTutteEmbedderFixOuterFace(gvizTutteState *s) {
    gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)s;
    if (!s || !gvizEmbeddedGraphIsPlanarEmbedded(embedding))
        return -1;

    const gvizSubgraph *highlight = gvizEmbeddedGraphGetHighlight(embedding);
    if (!gvizEmbeddedGraphHasHighlight(embedding))
        return -1;

    size_t N = numVertices(s);
    size_t *boundary = GVIZ_ALLOC(sizeof(size_t) * N);
    if (!boundary)
        return -1;

    size_t boundaryCount = 0;
    if (boundaryCycleFromSubgraph(highlight, boundary, N, &boundaryCount) < 0) {
        GVIZ_DEALLOC(boundary);
        return -1;
    }

    gvizTutteFixConvexPolygon(s, boundary, boundaryCount, 200.0);
    GVIZ_DEALLOC(boundary);
    gvizTutteEmbedderSeedInterior(s);
    s->begun = 1;
    return 0;
}
