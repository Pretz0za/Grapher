#ifndef _GVIZ_TUTTE_SOLVE_EMBEDDING_H_
#define _GVIZ_TUTTE_SOLVE_EMBEDDING_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"

/**
 * Tutte embedding solved directly via Cholesky factorization of the interior
 * Laplacian submatrix L_II. Boundary vertices are pinned; a single call to
 * gvizTutteSolveEmbeddingStep produces the exact Tutte positions.
 *
 * On boundary setup, L_II is assembled and factored via LAPACKE_dpotrf.
 * Each Step rebuilds the rhs from current boundary positions and calls
 * LAPACKE_dpotrs, writing the exact solution in O(N_I^2) work.
 *
 * The first field MUST remain gvizEmbeddedGraph so that a pointer to this
 * struct may be safely cast to gvizEmbeddedGraph * for renderer/layer code.
 */
typedef struct gvizTutteSolveState {
    gvizEmbeddedGraph graph;     /* MUST be first */
    size_t *boundary;            /* owned; boundary vertex indices */
    size_t boundaryCount;
    GVIZ_BIT_ARRAY isBoundary;   /* 1 bit per vertex */
    double *matLII;              /* N_I × N_I L_II; overwritten in-place by Cholesky factor */
    double *rhs;                 /* N_I × dim working buffer: rhs in, solution out (row-major) */
    size_t *interiorIdx;         /* [N_I]: interior index → vertex index */
    size_t *vertexToInterior;    /* [N]:   vertex index → interior index (SIZE_MAX = boundary) */
    size_t numInterior;
    int matrixReady;             /* 1 after dpotrf succeeds */
    size_t iteration;
    double lastMaxDelta;
    int converged;
    double epsilon;
} gvizTutteSolveState;

/**
 * Initializes the state for graph @p g in @p dimension dims.
 * @p epsilon is the convergence threshold (max L2 displacement to mark converged).
 * Pass 0 for a default of 1e-10 (effectively single-shot convergence).
 * Returns 0 on success, -1 on allocation failure.
 */
int gvizTutteSolveEmbeddingInit(gvizTutteSolveState *s, gvizGraph *g,
                                size_t dimension, double epsilon);

/** Releases all memory owned by @p s. */
void gvizTutteSolveEmbeddingRelease(gvizTutteSolveState *s);

/**
 * Pins @p count boundary vertices and copies their positions from
 * @p positions (row-major, dim doubles per vertex). Builds and factors L_II.
 * Returns 0 on success, -1 on invalid input or factorization failure.
 */
int gvizTutteSolveEmbeddingSetBoundary(gvizTutteSolveState *s,
                                       const size_t *boundary, size_t count,
                                       const double *positions);

/**
 * Places @p count boundary vertices on a regular convex polygon of @p radius
 * in the first two dimensions, then calls gvizTutteSolveEmbeddingSetBoundary.
 */
void gvizTutteSolveFixConvexPolygon(gvizTutteSolveState *s,
                                    const size_t *boundary, size_t count,
                                    double radius);

/**
 * Builds and Cholesky-factors the interior Laplacian submatrix L_II.
 * Called automatically by SetBoundary. Call again when graph topology changes.
 * Returns 0 on success, -1 on allocation or factorization failure.
 */
int gvizTutteSolveEmbeddingBuildMatrix(gvizTutteSolveState *s);

/**
 * Solves L_II * X = B for all dimensions in one shot using the precomputed
 * Cholesky factor, writing the exact Tutte positions to the embedding.
 * Sets converged = 1 after the first successful solve.
 * The @p dt parameter is unused; it exists for interface consistency.
 * Returns the maximum per-vertex L2 displacement from previous positions.
 */
double gvizTutteSolveEmbeddingStep(gvizTutteSolveState *s, double dt);

/**
 * Runs until converged or @p maxIters steps taken. Since each step is exact,
 * converges after one call. Returns iteration count, or -1 if state is invalid.
 */
int gvizTutteSolveEmbeddingRun(gvizTutteSolveState *s, size_t maxIters);

#endif
