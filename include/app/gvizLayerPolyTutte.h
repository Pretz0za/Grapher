#ifndef _GVIZ_APP_LAYER_POLY_TUTTE_H_
#define _GVIZ_APP_LAYER_POLY_TUTTE_H_

#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizTutteSolveEmbedding.h"
#include "renderer/layers/gvizGraphVBO.h"
#include "renderer/layers/gvizLayer.h"

typedef enum gvizLayerPolyTuttePhase {
    GVIZ_POLY_TUTTE_INITIAL = 0,
    GVIZ_POLY_TUTTE_SCANNING,
    GVIZ_POLY_TUTTE_FINAL,
} gvizLayerPolyTuttePhase;

/*
 * A world-space layer that runs Tutte on a 3-connected planar mesh.
 * On SPACE in the INITIAL phase, scans every face (highlighting each in
 * sequence), tracks the largest-area face, then in FINAL re-pins that face
 * as the outer boundary and re-runs Tutte.
 */
typedef struct gvizLayerPolyTutte {
    gvizLayer layer;          /* MUST be first */
    gvizGraph graph;
    gvizTutteSolveState tutte;
    gvizGraphVBO vbo;
    int gpuDirty;             /* 2=topology, 1=positions, 0=clean */
    int highlightDirty;
    int hasTutte;

    /* Highlight buffers — same shape as gvizLayerGraph */
    GVIZ_BIT_UNIT *vertexHighlight;
    size_t vertexHighlightBits;
    GVIZ_BIT_UNIT *edgeHighlight;
    size_t edgeHighlightBits;
    size_t *edgeStartIdx;

    /* Scan state */
    gvizLayerPolyTuttePhase phase;
    size_t scanFaceIdx;
    size_t bestFaceIdx;
    double bestFaceArea;
    gvizArray faces;          /* gvizArray of gvizArray<size_t> */

    /* Cached boundary radius from initial init */
    double boundaryRadius;

    /* Per-face dwell timer (seconds) used during SCANNING. */
    float scanTimer;
} gvizLayerPolyTutte;

int gvizLayerPolyTutteInit(gvizLayerPolyTutte *layer, gvizGraph *mesh,
                           const size_t *outerFace, size_t outerFaceLen,
                           size_t z);

void gvizLayerPolyTutteDraw(void *layer, const gvizCamera *camera);
void gvizLayerPolyTutteUpdate(void *layer, float dt);
void gvizLayerPolyTutteRelease(void *layer);
int gvizLayerPolyTutteHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_LAYER_POLY_TUTTE_VTABLE = {
    .draw    = gvizLayerPolyTutteDraw,
    .update  = gvizLayerPolyTutteUpdate,
    .release = gvizLayerPolyTutteRelease,
    .onEvent = gvizLayerPolyTutteHandleEvent,
    .hitTest = NULL,
};

#endif
