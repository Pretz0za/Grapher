#ifndef _GVIZ_TUTTE_EMBEDDING_H_
#define _GVIZ_TUTTE_EMBEDDING_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"

#define GVIZ_TUTTE_DEFAULT_EPSILON 1e-5

/**
 * State for the real-time Tutte barycentric embedding.
 *
 * Interior vertices are iteratively moved toward the barycenter of their
 * neighbors. Boundary vertices are pinned at fixed positions. Each call to
 * gvizTutteEmbeddingStep advances the simulation by one time-weighted
 * relaxation pass: P_u += (barycenter(N(u)) - P_u) * clamp(rate * dt, 0, 1).
 *
 * The first field MUST remain gvizEmbeddedGraph so that a pointer to this
 * struct may be safely cast to gvizEmbeddedGraph * for renderer/layer code.
 */
typedef struct gvizTutteState {
    gvizEmbeddedGraph graph;    /* MUST be first */
    size_t *boundary;           /* owned; boundary vertex indices */
    size_t boundaryCount;
    GVIZ_BIT_ARRAY isBoundary;  /* 1 bit per vertex */
    double *scratch;            /* N*dim Jacobi double-buffer */
    size_t iteration;
    double lastMaxDelta;
    int converged;
    int useGaussSeidel;         /* 0=Jacobi (default), 1=Gauss-Seidel */
    double epsilon;
    double relaxationRate;      /* blend factor per second (default 5.0) */
} gvizTutteState;

/**
 * Initializes the Tutte embedding state for graph @p g in @p dimension dims.
 * @p epsilon is the convergence threshold; pass 0 for GVIZ_TUTTE_DEFAULT_EPSILON.
 * Returns 0 on success, -1 on allocation failure.
 */
int gvizTutteEmbeddingInit(gvizTutteState *s, gvizGraph *g, size_t dimension,
                           double epsilon);

/** Releases all memory owned by @p s. */
void gvizTutteEmbeddingRelease(gvizTutteState *s);

/**
 * Pins @p count boundary vertices and copies their positions from
 * @p polygonPositions (row-major, dim doubles per vertex).
 * @p boundary must contain valid vertex indices; count must be >= 3.
 * Safe to call once. Returns 0 on success, -1 on invalid input.
 */
int gvizTutteEmbeddingSetBoundary(gvizTutteState *s, const size_t *boundary,
                                  size_t count, const double *polygonPositions);

/**
 * Seeds all interior vertices to the centroid of the pinned boundary polygon.
 * Also resets iteration, lastMaxDelta, and converged.
 */
void gvizTutteEmbeddingSeedInterior(gvizTutteState *s);

/**
 * Advances the embedding by one dt-weighted relaxation pass.
 * Each interior vertex moves fraction min(relaxationRate*dt, 1) toward its
 * neighbor barycenter. Jacobi mode (default) reads old positions and writes
 * from scratch so all updates within a step are order-independent.
 * Returns the maximum per-vertex L2 displacement this step.
 */
double gvizTutteEmbeddingStep(gvizTutteState *s, double dt);

/**
 * Runs until converged or @p maxIters steps taken (dt=1 per step).
 * Returns iteration count used, or -1 if state is invalid.
 */
int gvizTutteEmbeddingRun(gvizTutteState *s, size_t maxIters);

/**
 * Places @p count boundary vertices on a regular convex polygon of @p radius
 * in the first two dimensions, then calls gvizTutteEmbeddingSetBoundary.
 */
void gvizTutteFixConvexPolygon(gvizTutteState *s, const size_t *boundary,
                               size_t count, double radius);

#endif
