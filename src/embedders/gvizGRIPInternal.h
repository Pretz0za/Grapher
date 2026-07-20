#ifndef GVIZ_GRIP_INTERNAL_H
#define GVIZ_GRIP_INTERNAL_H

/*
 * Internal GRIP pipeline stages, exposed for white-box tests and benchmarks
 * only. Not part of the public interface: front-ends drive GRIP through
 * gvizGRIPEmbedder.h (Embed, NextStage, RefineRound) or the registered
 * actions.
 */

#include "embedders/gvizGRIPEmbedder.h"

/** Swaps *a and *b in place. */
static inline void gvizGRIPSwap(size_t *a, size_t *b) {
  if (a != b) {
    size_t tmp = *a;
    *a = *b;
    *b = tmp;
  }
}

/** Builds the finest MIS layer and appends its border to state->misBorder. */
void makeFirstMISPartition(gvizGRIPState *state, gvizVertexSubset out);

/**
 * Coarsens @p vertices into MIS layer @p i (spacing radius 2^(i-1)).
 * Returns 1 while further coarsening is possible, 0 at the final layer.
 */
int iterMISFiltration(gvizGRIPState *state, size_t i,
                      gvizVertexSubset vertices);

/**
 * Moves the graph-distance-farthest candidate from a shallower layer into the
 * final layer. Returns 0 on success, -1 when every pool is exhausted.
 */
int migrateOneToFinalLayer(gvizGRIPState *state, GVIZ_BIT_ARRAY finalLayer,
                           size_t count);

/** Builds the MIS filtration and returns the number of layers created. */
size_t createMISFiltration(gvizGRIPState *state);

#endif
