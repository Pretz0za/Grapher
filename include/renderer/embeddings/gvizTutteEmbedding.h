#ifndef _GVIZ_TUTTE_EMBEDDING_H_
#define _GVIZ_TUTTE_EMBEDDING_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"

#define GVIZ_TUTTE_DEFAULT_EPSILON 1e-5

/**
 * State for the real-time Tutte barycentric embedding.
 *
 * On boundary setup, the N_I × N_I averaging matrix M_II (= D^{-1} * A_II)
 * and the constant boundary contribution rhs (= D^{-1} * A_IB * x_B) are
 * precomputed. Each call to gvizTutteEmbeddingStep performs one Jacobi
 * relaxation pass via cblas_dgemv:
 *   bary = M_II * x_I + rhs   (full neighbor barycenter for each interior vertex)
 *   x_I += alpha * (bary - x_I)
 *
 * The first field MUST remain gvizEmbeddedGraph so that a pointer to this
 * struct may be safely cast to gvizEmbeddedGraph * for renderer/layer code.
 */
typedef struct gvizTutteState {
    gvizEmbeddedGraph graph;     /* MUST be first */
    size_t *boundary;            /* owned; boundary vertex indices */
    size_t boundaryCount;
    GVIZ_BIT_ARRAY isBoundary;   /* 1 bit per vertex */
    double *matII;               /* N_I × N_I averaging matrix M_II (row-major) */
    double *rhs;                 /* N_I × dim precomputed boundary contribution (row-major) */
    double *xiBuf;               /* N_I snapshot buffer for cblas input */
    double *baryBuf;             /* N_I output buffer for cblas_dgemv result */
    size_t *interiorIdx;         /* [N_I]: interior index → vertex index */
    size_t *vertexToInterior;    /* [N]:   vertex index → interior index (SIZE_MAX = boundary) */
    size_t numInterior;
    int matrixBuilt;             /* 1 after BuildMatrix succeeds */
    size_t iteration;
    double lastMaxDelta;
    int converged;
    int useGaussSeidel;          /* reserved */
    double epsilon;
    double relaxationRate;       /* blend factor per second (default 5.0) */
} gvizTutteState;

/**
 * Initializes the Tutte embedding state for graph @p g in @p dimension dims.
 * @p epsilon is the convergence threshold; pass 0 for GVIZ_TUTTE_DEFAULT_EPSILON.
 * Returns 0 on success, -1 on allocation failure.
 */
int gvizTutteEmbeddingInit(gvizTutteState *s, gvizGraph *g, size_t dimension,
                           double epsilon);

/**
 * Builds the N_I × N_I averaging matrix M_II and precomputes the constant
 * boundary rhs from current pinned positions. Called automatically by
 * gvizTutteEmbeddingSetBoundary; call again when graph topology changes.
 * Returns 0 on success, -1 on allocation failure.
 */
int gvizTutteEmbeddingBuildMatrix(gvizTutteState *s);

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
 * Advances the embedding by one dt-weighted Jacobi relaxation pass using
 * cblas_dgemv. For each coordinate dimension:
 *   bary = M_II * x_I + rhs  (full neighbor barycenter via matrix multiply)
 *   x_I += clamp(relaxationRate * dt, 0, 1) * (bary - x_I)
 * Returns the maximum per-vertex L2 displacement, or 0 if matrix not built.
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
