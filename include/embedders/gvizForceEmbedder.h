#ifndef _GVIZ_FORCE_EMBEDDER_H_
#define _GVIZ_FORCE_EMBEDDER_H_

#include "core/gvizThreadPool.h"
#include "ds/gvizQuadtree.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizForceModel.h"
#include <stddef.h>

#define GVIZ_FORCE_EMBEDDER_EDGE_LENGTH_DEFAULT 10.0
#define GVIZ_FORCE_EMBEDDER_THETA_DEFAULT 1.0

/* Per-vertex adaptive heat (step length), mirroring gvizGRIPEmbedder's model:
 * heat grows while a vertex keeps displacing in a consistent direction across
 * rounds, and shrinks when it reverses (oscillating around a resting point).
 * Unlike a global cooling schedule, this lets vertices still migrating toward
 * their equilibrium (e.g. a subtree still settling on a side of the root)
 * keep accelerating instead of being throttled by a round-count-based cap. */
#define GVIZ_FORCE_EMBEDDER_HEAT_INITIAL_FACTOR (1.0 / 6.0)
#define GVIZ_FORCE_EMBEDDER_HEAT_MIN_FACTOR 1e-4
#define GVIZ_FORCE_EMBEDDER_HEAT_MAX_FACTOR 10.0
#define GVIZ_FORCE_EMBEDDER_HEAT_R_DEFAULT 0.15
#define GVIZ_FORCE_EMBEDDER_HEAT_S_DEFAULT 3.0

/**
 * State for a Barnes-Hut force-directed embedder: repulsion applies between
 * every pair of vertices and is approximated by walking a quadtree over the
 * current positions and treating distant subtrees as a single pseudo-body at
 * their center of mass; attraction is computed additionally, on top of that
 * repulsion, exactly along real graph edges (O(E) per round). The actual
 * force math (attraction, repulsion, and the mass a vertex contributes to
 * quadtree aggregation) is pluggable via a gvizForceModel selected at Init;
 * everything else (heat, Barnes-Hut traversal, actions, stat series) is
 * shared across models. gvizForceEmbedderSetBarnesHutEnabled can disable the
 * quadtree approximation in favor of exact O(V^2) all-pairs repulsion when a
 * brute-force reference is wanted instead of the scalable default.
 *
 * The first field MUST remain gvizEmbeddedGraph so that a pointer to this
 * struct may be safely cast to gvizEmbeddedGraph *.
 */
typedef struct gvizForceEmbedderState {
  gvizEmbeddedGraph graph; /* MUST be first */
  size_t *vertices;        /* owned; active subgraph vertex ids, compact index
                            * i -> real vertex id */
  size_t vertexCount;
  const gvizForceModel *model; /* force computation strategy, fixed at Init */
  double *mass;   /* owned; vertexCount, model->vertexMass(degree[i]) per
                   * vertex, computed once at Init */
  size_t *degree; /* owned; vertexCount, raw subgraph degree per vertex,
                   * independent of model->vertexMass; used by gravity */
  double meanDegreePlusOne; /* mean over vertices of (degree + 1), computed
                            * once at Init since degree is fixed for the
                            * embedder's lifetime; avoids an O(vertexCount)
                            * reduction every Step just for the gravity stat */
  double *disp;             /* owned; vertexCount * dim scratch accumulator */
  double *positionsScratch; /* owned; vertexCount * 2 doubles, gathered from
                             * vPos each round in state->vertices[] order; fed
                             * to the quadtree */
  double *attForceMag; /* owned; vertexCount scratch, per-vertex attractive
                        * force magnitude for this round's stats */
  double *repForceMag; /* owned; vertexCount scratch, per-vertex repulsive
                        * force magnitude for this round's stats */
  double *heat;    /* owned; vertexCount, per-vertex adaptive step length that
                    * the raw force for that vertex is rescaled to each round */
  double *oldDisp; /* owned; vertexCount * 2, previous round's applied
                    * (heat-scaled) displacement, for the direction-consistency
                    * check in the next round's heat update */
  double *oldCos;  /* owned; vertexCount, previous round's direction-
                    * consistency cosine, so two consecutive agreeing rounds
                    * accelerate harder than one */
  double heatR; /* base heat growth/decay rate */
  double heatS; /* extra multiplier applied to heatR on a second consecutive
                * agreeing round */
  double edgeLength;
  double boxExtent;
  double gravityK; /* constant-magnitude-per-(deg+1) pull toward the origin;
                    * 0.0 (the default) disables gravity */
  size_t iteration;
  double lastMaxDisplacement;
  int begun;
  double theta; /* Barnes-Hut opening-angle threshold; embedder-owned, not a
                * field of gvizQuadtree */
  size_t nodesPerCell;
  int barnesHutEnabled; /* whether Step approximates repulsion via a quadtree
                        * walk (1, the default) or exact all-pairs (0) */
  gvizQuadtree quadtree;
  int quadtreeReady; /* whether gvizQuadtreeInit has run yet (vs needing
                      * gvizQuadtreeRebuild); stays 0 when barnesHutEnabled
                      * is 0, since the quadtree is never built in that mode */
  /**
   * Worker pool for future data-parallel force evaluation. Always NULL for
   * now.
   */
  gvizThreadPool *pool;
} gvizForceEmbedderState;

/**
 * Initializes @p state for @p subgraph in @p dimension dimensions, using
 * @p model for attraction, repulsion, and vertex mass (see gvizForceModel).
 * Only dimension 2 is supported, since repulsion is approximated with a 2D
 * quadtree. Registers action "forceEmbedder.step" and stat series
 * "forceEmbedder.maxDisp", "forceEmbedder.heat",
 * "forceEmbedder.attractiveForce", "forceEmbedder.repulsiveForce", and
 * "forceEmbedder.gravityForce" (the last four are means over active vertices
 * of each vertex's heat/attractive/repulsive/gravity force magnitude). Edge
 * length and box extent are set to sensible defaults; override with
 * gvizForceEmbedderConfigure, gvizForceEmbedderConfigureHeat,
 * gvizForceEmbedderConfigureBarnesHut, gvizForceEmbedderSetBarnesHutEnabled,
 * and gvizForceEmbedderConfigureGravity before calling Begin. @p model is
 * structural, like @p dimension: fixed for the embedder's lifetime. The
 * quadtree itself is not built until Begin, since positions are all zero
 * right after this call.
 *
 * @return 0 on success, -1 on allocation failure, -2 if @p dimension != 2.
 */
int gvizForceEmbedderInit(gvizForceEmbedderState *state,
                          gvizSubgraph subgraph, size_t dimension,
                          gvizForceModelKind model);

/** Releases all memory owned by @p state. Safe to call even if Init returned
 *  -2 or Begin was never called. */
void gvizForceEmbedderRelease(gvizForceEmbedderState *state);

/**
 * Overrides the target edge length and the half-width of the random initial
 * placement box. Pass 0 for either to keep its current/default value. Call
 * before gvizForceEmbedderBegin.
 */
void gvizForceEmbedderConfigure(gvizForceEmbedderState *state,
                                double edgeLength, double boxExtent);

/**
 * Overrides the per-vertex heat model's growth/decay rate @p r and the extra
 * multiplier @p s applied to it on a second consecutive round in which a
 * vertex's displacement direction agrees with the previous round's. Pass 0
 * for either to keep its current/default value. Call before
 * gvizForceEmbedderBegin.
 */
void gvizForceEmbedderConfigureHeat(gvizForceEmbedderState *state, double r,
                                    double s);

/**
 * Overrides the Barnes-Hut opening-angle threshold @p theta and the
 * quadtree's @p nodesPerCell. Pass 0 for either to keep its current/default
 * value. @p theta is compared against a subtree's side length over its
 * distance to the query point (side/distance, not half-side/distance): a
 * subtree is approximated as one pseudo-body when that ratio is less than
 * @p theta, and recursed into otherwise. Call before gvizForceEmbedderBegin,
 * since @p nodesPerCell affects the quadtree built there.
 */
void gvizForceEmbedderConfigureBarnesHut(gvizForceEmbedderState *state,
                                         double theta, size_t nodesPerCell);

/**
 * Enables or disables the Barnes-Hut quadtree approximation of repulsion.
 * Enabled (the default) by gvizForceEmbedderInit. When disabled, Step
 * computes repulsion by exact all-pairs evaluation instead of a quadtree
 * walk; no allocation occurs either way, since the quadtree fields simply
 * stay unused/uninitialized in that case (gvizQuadtreeRelease on a zeroed
 * struct is already known-safe per that function's contract). Call before
 * gvizForceEmbedderBegin.
 */
void gvizForceEmbedderSetBarnesHutEnabled(gvizForceEmbedderState *state,
                                          int enabled);

/**
 * Sets the gravity constant @p k: every vertex feels a constant-magnitude
 * force of k * (deg(v) + 1) pulling it toward the origin, using the
 * vertex's raw subgraph degree regardless of which force model's mass
 * abstraction is active. @p k = 0 (the default) disables gravity. Unlike
 * the other Configure* functions, there is no "0 means keep current" special
 * case: @p k is always assigned unconditionally, since 0 is gravity's
 * legitimate default/off value.
 */
void gvizForceEmbedderConfigureGravity(gvizForceEmbedderState *state,
                                       double k);

/**
 * Places every active vertex uniformly at random inside the
 * [-boxExtent, boxExtent]^2 box, so the layout starts compact rather than
 * scattering vertices arbitrarily far apart. @p seed seeds the generator;
 * pass 0 for a time-based seed. Resets every vertex's heat to
 * edgeLength * GVIZ_FORCE_EMBEDDER_HEAT_INITIAL_FACTOR and clears the
 * direction-consistency history, and (re)builds the quadtree over the
 * freshly randomized positions. Safe to call again to restart the layout.
 *
 * @return 0 on success, -1 on allocation failure while building the
 * quadtree.
 */
int gvizForceEmbedderBegin(gvizForceEmbedderState *state, unsigned int seed);

/**
 * Runs one round: accumulates the model's repulsive force, approximated by
 * a Barnes-Hut quadtree walk over the current positions when
 * barnesHutEnabled (the default), or by exact all-pairs evaluation
 * otherwise, unconditionally between every active vertex and every other
 * active vertex, plus the model's exact attractive force along every real
 * edge of every active vertex, additionally on top of that vertex's
 * repulsion, plus (if
 * gvizForceEmbedderConfigureGravity set a nonzero k) a constant-magnitude
 * force pulling the vertex toward the origin. Each vertex's raw force is
 * then rescaled to that vertex's current heat (a per-vertex adaptive step
 * length, not a single global temperature): heat grows when this round's
 * displacement direction agrees with last round's (accelerating a vertex
 * that is still consistently migrating toward its equilibrium), and shrinks
 * when it reverses (damping a vertex that is oscillating around a resting
 * point). Heat is clamped to
 * [edgeLength * GVIZ_FORCE_EMBEDDER_HEAT_MIN_FACTOR,
 *  edgeLength * GVIZ_FORCE_EMBEDDER_HEAT_MAX_FACTOR]. The mean of all
 * heat-scaled displacements is then subtracted from every vertex before
 * applying them, since nothing else pins the layout's center of mass and the
 * heat model rewards coherent motion, which would otherwise let any residual
 * net force drift or spin the whole embedding indefinitely.
 *
 * @return the maximum per-vertex displacement actually applied this round.
 */
double gvizForceEmbedderStep(gvizForceEmbedderState *state);

/**
 * Runs gvizForceEmbedderStep until the max displacement drops below
 * @p epsilon or @p maxIters rounds have run.
 *
 * @return the number of rounds run, or -1 if Begin has not been called.
 */
int gvizForceEmbedderRun(gvizForceEmbedderState *state, size_t maxIters,
                         double epsilon);

#endif
