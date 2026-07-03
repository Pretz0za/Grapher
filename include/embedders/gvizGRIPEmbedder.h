#ifndef _GVIZ_GRIP_H_
#define _GVIZ_GRIP_H_

#include "algorithms/search/gvizKNearest.h"
#include "ds/gvizArray.h"
#include "ds/gvizDeque.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"

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
  gvizVertexSubset placed;
  size_t *misFiltration;
  size_t *misBorder;
  size_t *rounds;
  gvizGRIPDecorators *dec;
  GVIZ_BIT_ARRAY dispCalculated;
  size_t *radiusBfsStamp;
  size_t radiusBfsEpoch;
  gvizDeque radiusBfsQueue;
} gvizGRIPState;

/**
 * Initializes GRIP embedder state for @p graph in @p dimension dimensions.
 * @p diameter may be 0 if unknown; it sizes internal MIS-filtration buffers.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizGRIPEmbedderInit(gvizGRIPState *state, gvizSubgraph subgraph,
                         size_t diameter, size_t dimension);

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
