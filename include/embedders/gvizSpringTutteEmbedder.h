#ifndef _GVIZ_SPRING_TUTTE_EMBEDDER_H_
#define _GVIZ_SPRING_TUTTE_EMBEDDER_H_

#include "ds/gvizArray.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizGraph.h"
#include "embedders/gvizEmbeddedGraph.h"

#define GVIZ_SPRING_TUTTE_DEFAULT_EPSILON 1e-5
#define GVIZ_SPRING_TUTTE_DEFAULT_STIFFNESS 30.0
#define GVIZ_SPRING_TUTTE_DEFAULT_DAMPING 6.0

/**
 * State for a second-order, spring-driven variant of the Tutte barycentric
 * embedding. Same fixed point as gvizTutteState (every interior vertex
 * settles at the barycenter of its neighbors, boundary vertices pinned), but
 * reached via a damped harmonic oscillator instead of a direct position
 * blend, so underdamped settings let vertices overshoot their equilibrium
 * and spring back before settling:
 *
 *   a_u = stiffness * (barycenter(N(u)) - P_u) - damping * v_u
 *   v_u += a_u * dt
 *   P_u += v_u * dt
 *
 * The first field MUST remain gvizEmbeddedGraph so that a pointer to this
 * struct may be safely cast to gvizEmbeddedGraph *.
 */
typedef struct gvizSpringTutteState {
    gvizEmbeddedGraph graph;    /* MUST be first */
    size_t *boundary;           /* owned; boundary vertex indices */
    size_t boundaryCount;
    GVIZ_BIT_ARRAY isBoundary;  /* 1 bit per vertex */
    double *scratchPos;         /* N*dim Jacobi double-buffer for positions */
    double *velocity;           /* N*dim owned; zero for boundary vertices */
    size_t iteration;
    double lastMaxDelta;
    int converged;
    double epsilon;             /* converged once max(|P delta|, |v|) < epsilon */
    double stiffness;           /* spring constant pulling toward barycenter */
    double damping;             /* velocity damping; >= 2*sqrt(stiffness) is critically damped */
    int begun;
} gvizSpringTutteState;

/**
 * Initializes the spring-Tutte embedding state for graph @p g in
 * @p dimension dims. @p epsilon is the convergence threshold; pass 0 for
 * GVIZ_SPRING_TUTTE_DEFAULT_EPSILON. Registers action "springTutte.step" and
 * stat series "springTutte.maxDelta".
 * Returns 0 on success, -1 on allocation failure.
 */
int gvizSpringTutteEmbedderInit(gvizSpringTutteState *s, gvizSubgraph subgraph,
                                size_t dimension, double epsilon);

/** Releases all memory owned by @p s. */
void gvizSpringTutteEmbedderRelease(gvizSpringTutteState *s);

/**
 * Overrides stiffness and damping. Pass 0 for either to keep its current/
 * default value. Call before or after Begin; takes effect on the next step.
 */
void gvizSpringTutteEmbedderConfigure(gvizSpringTutteState *s,
                                      double stiffness, double damping);

/**
 * Verifies planarity, applies a combinatorial rotation system, pins the
 * largest combinatorial face as the initial outer boundary on a regular
 * convex polygon, and seeds interior vertices. @p dimension must be 2. Safe
 * to call once per state lifetime.
 *
 * @return 0 on success, -1 on error, -2 if the graph is non-planar.
 */
int gvizSpringTutteEmbedderBegin(gvizSpringTutteState *s);

/**
 * Pins @p count boundary vertices and copies their positions from
 * @p polygonPositions (row-major, dim doubles per vertex). Zeroes velocity
 * for the pinned vertices. @p boundary must contain valid vertex indices;
 * count must be >= 3. May be called again to change the outer face. Returns
 * 0 on success, -1 on invalid input.
 */
int gvizSpringTutteEmbedderSetBoundary(gvizSpringTutteState *s,
                                       const size_t *boundary, size_t count,
                                       const double *polygonPositions);

/**
 * Seeds all interior vertices to the centroid of the pinned boundary polygon
 * and zeroes all velocities. Also resets iteration, lastMaxDelta, and
 * converged.
 */
void gvizSpringTutteEmbedderSeedInterior(gvizSpringTutteState *s);

/**
 * Places @p count boundary vertices on a regular convex polygon of @p radius
 * in the first two dimensions, then calls gvizSpringTutteEmbedderSetBoundary.
 */
void gvizSpringTutteFixConvexPolygon(gvizSpringTutteState *s,
                                     const size_t *boundary, size_t count,
                                     double radius);

/**
 * Advances the embedding by one dt-sized step of damped spring dynamics.
 * Each interior vertex accelerates toward its neighbor barycenter under a
 * Hooke's-law spring, decelerated by a linear damping term on its current
 * velocity, then both velocity and position are integrated with semi-
 * implicit Euler. Reads old positions from a Jacobi double-buffer so all
 * updates within a step are order-independent.
 * Returns the maximum per-vertex L2 position displacement this step.
 */
double gvizSpringTutteEmbedderStep(gvizSpringTutteState *s, double dt);

/**
 * Runs until converged (see epsilon) or @p maxIters steps taken, with
 * @p dt applied per step.
 * Returns iteration count used, or -1 if state is invalid.
 */
int gvizSpringTutteEmbedderRun(gvizSpringTutteState *s, size_t maxIters,
                               double dt);

/**
 * Pins the highlighted face boundary (gvizEmbeddedGraph highlight subgraph)
 * on a regular convex polygon, re-seeds interior vertices, and resets
 * convergence state.
 *
 * @return 0 on success, -1 when no highlight is set or boundary setup fails.
 */
int gvizSpringTutteEmbedderFixOuterFace(gvizSpringTutteState *s);

#endif
