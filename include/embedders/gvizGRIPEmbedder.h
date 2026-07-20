#ifndef GVIZ_GRIP_EMBEDDER_H
#define GVIZ_GRIP_EMBEDDER_H

#include "algorithms/search/gvizKNearest.h"
#include "core/gvizThreadPool.h"
#include "ds/gvizArray.h"
#include "ds/gvizDeque.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"

/** How placement/refinement neighbor counts are chosen per layer. */
typedef enum gvizGRIPKPolicy {
  /** Same @c placementKMax / @c refinementKMax at every layer. */
  GVIZ_GRIP_K_CONSTANT = 0,
  /** @c maxK >> currLayer — smaller on coarse layers (legacy fast path). */
  GVIZ_GRIP_K_LAYER_DECAY,
  /** @c maxK >> depthFromCoarse — larger on coarse layers. */
  GVIZ_GRIP_K_LAYER_GROW,
  /** Placement uses @ref GVIZ_GRIP_K_LAYER_DECAY; refinement stays constant. */
  GVIZ_GRIP_K_PLACEMENT_DECAY,
  /**
   * Refinement k scales inversely with the active-layer vertex count so the
   * per-round cost stays roughly constant: k = refinementKMax * N / active,
   * capped at @c knnCapacity. Sparse (coarse/mid) layers get long-range
   * springs, which keeps articulated substructures rigid — without them a
   * subgraph attached through a single vertex can slowly fold onto the rest
   * of the layout. Placement uses @ref GVIZ_GRIP_K_LAYER_DECAY.
   */
  GVIZ_GRIP_K_BUDGET,
} gvizGRIPKPolicy;

/** Displacement and force statistics from the most recent refinement round. */
typedef struct gvizGRIPRoundStats {
  double maxDisplacement;
  double meanDisplacement;
  /** Mean raw spring-force magnitude before heat scaling. */
  double meanForce;
} gvizGRIPRoundStats;

typedef struct gvizGRIPDecorators {
  gvizArray knn;
  double *disp;
  double *oldDisp;
  double oldCos;
  double heat;
  /** Raw force magnitude from the latest force evaluation (pre heat scaling). */
  double lastForceMag;
} gvizGRIPDecorators;

typedef struct gvizGRIPState {
  gvizEmbeddedGraph graph;
  size_t layerCount;
  size_t currLayer;
  size_t currRound;
  size_t *misFiltration;
  gvizArray misBorder;
  gvizGRIPDecorators *dec;
  GVIZ_BIT_ARRAY dispCalculated;
  size_t *radiusBfsStamp;
  size_t radiusBfsEpoch;
  gvizDeque radiusBfsQueue;
  /** One KNN scratch buffer per pool worker plus one for the caller thread. */
  gvizKNearestScratch *knnScratch;
  size_t knnScratchCount;
  /** Worker pool for data-parallel phases. NULL => serial fallback. */
  gvizThreadPool *pool;
  /** Upper bounds for KNN search; arrays are sized to @c knnCapacity. */
  size_t placementKMax;
  size_t refinementKMax;
  size_t knnCapacity;
  gvizGRIPKPolicy kPolicy;
  gvizGRIPRoundStats lastRoundStats;
  /** 0 = stats on (default for zero-initialized state), 1 = disabled via
   *  gvizGRIPEmbedderConfigureStats before init. */
  uint8_t statsRegisterMode;
  /** Resolved at init: whether stat series are registered and appended. */
  bool statsEnabled;
} gvizGRIPState;

/**
 * Initializes GRIP embedder state for @p graph in @p dimension dimensions (2, 3,
 * or 4). In 4D, net rotation is removed by projecting it out of all six
 * coordinate-plane rotations each refinement round.
 * @p diameter may be 0 if unknown; it sizes internal MIS-filtration buffers.
 *
 * @return 0 on success, -1 on allocation failure or if @p subgraph has fewer
 * than @p dimension + 1 active vertices (too few to place the coarsest
 * simplex).
 */
int gvizGRIPEmbedderInit(gvizGRIPState *state, gvizSubgraph subgraph,
                         size_t diameter, size_t dimension);

/**
 * Sets the per-vertex KNN storage capacity used by gvizGRIPEmbedderInit
 * (default 256). Storage is allocated once at init, so this must be called
 * on a zero-initialized @p state before gvizGRIPEmbedderInit; it has no
 * effect afterward.
 */
void gvizGRIPEmbedderConfigureKnnCapacity(gvizGRIPState *state,
                                          size_t knnCapacity);

/**
 * Configures neighbor counts for placement and refinement; both are clamped
 * to the KNN capacity set at init (see gvizGRIPEmbedderConfigureKnnCapacity).
 * A value of 0 leaves that max unchanged. Defaults after init: placement/
 * refinement max 128, @ref GVIZ_GRIP_K_CONSTANT.
 */
void gvizGRIPEmbedderConfigureK(gvizGRIPState *state, size_t placementKMax,
                                size_t refinementKMax,
                                gvizGRIPKPolicy policy);

/**
 * Enables or disables live stat series on the embedded graph. When disabled,
 * no gvizStatSeries are registered and refinement rounds skip
 * gvizEmbeddedGraphStatAppend (zero chart overhead). Default is enabled.
 * Call before gvizGRIPEmbedderInit.
 */
void gvizGRIPEmbedderConfigureStats(gvizGRIPState *state, bool enable);

/** Returns stats collected by the last call to gvizGRIPEmbedderRefineRound. */
gvizGRIPRoundStats gvizGRIPEmbedderLastRoundStats(const gvizGRIPState *state);

/**
 * Runs the full GRIP embedding pipeline on @p state: Begin, then per layer a
 * bounded number of refinement rounds (stopping early once the max
 * displacement settles), advancing until the finest layer is refined.
 * One-shot equivalent of driving Begin / RefineRound / NextStage manually.
 *
 * @return 0 on success, -1 if @p state is NULL or Begin produced no layers.
 */
int gvizGRIPEmbedderEmbed(gvizGRIPState *state);

/** Releases all memory owned by @p state. Safe after a failed Init. */
void gvizGRIPEmbedderRelease(gvizGRIPState *state);

/**
 * Starts GRIP embedding: builds MIS filtration, places the final simplex,
 * and prepares the coarsest active layer.
 */
void gvizGRIPEmbedderBegin(gvizGRIPState *state);

/**
 * Advances to the next finer layer and places its vertices. No-op at layer 0.
 * Also exposed as the "grip.nextStage" action.
 */
void gvizGRIPEmbedderNextStage(gvizGRIPState *state);

/**
 * Runs one force-directed refinement round on the current layer.
 * Also exposed as the "grip.refineRound" action.
 */
void gvizGRIPEmbedderRefineRound(gvizGRIPState *state);

#endif
