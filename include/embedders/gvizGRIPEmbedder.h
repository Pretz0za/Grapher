#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

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

/** Displacement statistics from the most recent refinement round. */
typedef struct gvizGRIPRoundStats {
  double maxDisplacement;
  double meanDisplacement;
} gvizGRIPRoundStats;

typedef struct gvizGRIPDecorators {
  gvizArray knn;
  double *disp;
  double *oldDisp;
  double oldCos;
  double heat;
} gvizGRIPDecorators;

typedef struct gvizGRIPState {
  gvizEmbeddedGraph graph;
  size_t layerCount;
  size_t currLayer;
  size_t currRound;
  size_t *misFiltration;
  gvizArray misBorder;
  size_t *rounds;
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
} gvizGRIPState;

/**
 * Initializes GRIP embedder state for @p graph in @p dimension dimensions.
 * @p diameter may be 0 if unknown; it sizes internal MIS-filtration buffers.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizGRIPEmbedderInit(gvizGRIPState *state, gvizSubgraph subgraph,
                         size_t diameter, size_t dimension);

/**
 * Configures neighbor counts for placement and refinement. @p knnCapacity sizes
 * per-vertex KNN storage and must be >= both max values. Defaults after init:
 * placement/refinement max 128, capacity 256, @ref GVIZ_GRIP_K_CONSTANT.
 */
void gvizGRIPEmbedderConfigureK(gvizGRIPState *state, size_t placementKMax,
                                size_t refinementKMax, size_t knnCapacity,
                                gvizGRIPKPolicy policy);

/** Returns stats collected by the last call to runRefinementRound. */
gvizGRIPRoundStats gvizGRIPEmbedderLastRoundStats(const gvizGRIPState *state);

/** Runs the full GRIP embedding pipeline on @p state. */
int gvizGRIPEmbedderEmbed(gvizGRIPState *state);

/** Releases all memory owned by @p state. */
void gvizGRIPEmbedderRelease(gvizGRIPState *state);

/** Builds the MIS filtration and returns the number of layers created. */
size_t createMISFiltration(gvizGRIPState *state);

/**
 * Starts GRIP embedding: builds MIS filtration, places the final simplex,
 * and prepares the coarsest active layer.
 */
void gvizGRIPEmbedderBegin(gvizGRIPState *state);

/** Advances to the next coarser layer and places its vertices. No-op at layer 0. */
void beginNewStage(gvizGRIPState *state);

/** Runs one force-directed refinement round on the current layer. */
void runRefinementRound(gvizGRIPState *state);

#endif
