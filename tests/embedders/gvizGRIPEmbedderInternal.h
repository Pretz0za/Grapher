#ifndef GVIZ_GRIP_EMBEDDER_INTERNAL_H
#define GVIZ_GRIP_EMBEDDER_INTERNAL_H

#include "embedders/gvizGRIPEmbedder.h"

void makeFirstMISPartition(gvizGRIPState *state, gvizVertexSubset out);
int iterMISFiltration(gvizGRIPState *state, size_t i, gvizVertexSubset vertices);
void migrateOneToFinalLayer(gvizGRIPState *state, GVIZ_BIT_ARRAY finalLayer,
                            size_t count);

#endif
