#ifndef _GVIZ_FR_PAIRWISE_EMBEDDER_H_
#define _GVIZ_FR_PAIRWISE_EMBEDDER_H_

#include "core/gvizThreadPool.h"
#include "embedders/gvizEmbeddedGraph.h"
#include <stddef.h>

#define GVIZ_FR_PAIRWISE_EDGE_LENGTH_DEFAULT 10.0
#define GVIZ_FR_PAIRWISE_COOLING_FACTOR_DEFAULT 0.97
#define GVIZ_FR_PAIRWISE_MIN_TEMP_FACTOR 0.01
#define GVIZ_FR_PAIRWISE_INITIAL_TEMP_FACTOR 3.0

/**
 * State for a brute-force pairwise Fruchterman-Reingold embedder. Every
 * round, every vertex pair exerts a force on each other: attractive if an
 * edge connects them, repulsive otherwise. O(V^2) per round; a simple
 * reference layout rather than a scalable one (contrast with GRIP).
 *
 * The first field MUST remain gvizEmbeddedGraph so that a pointer to this
 * struct may be safely cast to gvizEmbeddedGraph *.
 */
typedef struct gvizFRPairwiseState {
  gvizEmbeddedGraph graph; /* MUST be first */
  size_t *vertices;        /* owned; active subgraph vertex ids */
  size_t vertexCount;
  double *disp; /* owned; vertexCount * dim scratch accumulator */
  double *attForceMag; /* owned; vertexCount scratch, per-vertex attractive
                        * force magnitude for this round's stats */
  double *repForceMag; /* owned; vertexCount scratch, per-vertex repulsive
                        * force magnitude for this round's stats */
  double edgeLength;
  double boxExtent;
  double temperature;    /* current per-round displacement cap; cools each step */
  double coolingFactor;  /* temperature *= coolingFactor each step, floored at
                          * edgeLength * GVIZ_FR_PAIRWISE_MIN_TEMP_FACTOR */
  size_t iteration;
  double lastMaxDisplacement;
  int begun;
  /**
   * Worker pool for future data-parallel force evaluation. Always NULL for
   * now, which makes gvizThreadPoolForRange run the force pass serially;
   * force computation is already split into disjoint per-vertex ranges so
   * that assigning a real pool here is the only change needed later.
   */
  gvizThreadPool *pool;
} gvizFRPairwiseState;

/**
 * Initializes @p state for @p subgraph in @p dimension dimensions.
 * Registers action "frPairwise.step" and stat series "frPairwise.maxDisp",
 * "frPairwise.temperature", "frPairwise.attractiveForce", and
 * "frPairwise.repulsiveForce" (the last two are means over active vertices of
 * each vertex's attractive/repulsive force magnitude). Edge length and box
 * extent are set to sensible defaults; override with
 * gvizFRPairwiseEmbedderConfigure before calling Begin.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizFRPairwiseEmbedderInit(gvizFRPairwiseState *state,
                               gvizSubgraph subgraph, size_t dimension);

/** Releases all memory owned by @p state. */
void gvizFRPairwiseEmbedderRelease(gvizFRPairwiseState *state);

/**
 * Overrides the target edge length and the half-width of the random initial
 * placement box. Pass 0 for either to keep its current/default value. Call
 * before gvizFRPairwiseEmbedderBegin.
 */
void gvizFRPairwiseEmbedderConfigure(gvizFRPairwiseState *state,
                                     double edgeLength, double boxExtent);

/**
 * Overrides the per-step cooling factor applied to the temperature (the
 * displacement cap). Pass 0 to keep the current/default value. A value closer
 * to 1 cools more slowly; must be in (0, 1]. Call before
 * gvizFRPairwiseEmbedderBegin.
 */
void gvizFRPairwiseEmbedderConfigureCooling(gvizFRPairwiseState *state,
                                            double coolingFactor);

/**
 * Places every active vertex uniformly at random inside the
 * [-boxExtent, boxExtent]^dim box, so the layout starts compact rather than
 * scattering vertices arbitrarily far apart. @p seed seeds the generator;
 * pass 0 for a time-based seed. Resets the temperature to edgeLength. Safe to
 * call again to restart the layout.
 */
void gvizFRPairwiseEmbedderBegin(gvizFRPairwiseState *state,
                                 unsigned int seed);

/**
 * Runs one round: accumulates the pairwise attractive/repulsive force on
 * every active vertex against every other active vertex, then applies the
 * resulting displacement, clamped to at most the current temperature per
 * vertex (the standard Fruchterman-Reingold temperature cap). The temperature
 * cools by coolingFactor every round, floored at
 * edgeLength * GVIZ_FR_PAIRWISE_MIN_TEMP_FACTOR, which damps the back-and-forth
 * oscillation an uncooled fixed cap can fall into.
 *
 * @return the maximum per-vertex displacement actually applied this round.
 */
double gvizFRPairwiseEmbedderStep(gvizFRPairwiseState *state);

/**
 * Runs gvizFRPairwiseEmbedderStep until the max displacement drops below
 * @p epsilon or @p maxIters rounds have run.
 *
 * @return the number of rounds run, or -1 if Begin has not been called.
 */
int gvizFRPairwiseEmbedderRun(gvizFRPairwiseState *state, size_t maxIters,
                              double epsilon);

#endif
