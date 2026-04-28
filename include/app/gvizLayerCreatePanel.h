#ifndef _GVIZ_APP_LAYER_CREATE_PANEL_H_
#define _GVIZ_APP_LAYER_CREATE_PANEL_H_

#include "core/gvizScene.h"
#include "renderer/layers/gvizLayer.h"

/*
 * Layer-creation panel — modal form for picking a new layer's mode,
 * algorithm, and source. Driven by a gvizLayerCreateParams struct that
 * the caller stashes; the orchestrator reads `result` after each frame.
 */

typedef enum gvizCreateAlgo {
  GVIZ_CREATE_TUTTE = 0,
  GVIZ_CREATE_GRIP,
  GVIZ_CREATE_POLYTUTTE,
  GVIZ_CREATE_RT,
  GVIZ_CREATE_EMPTY,
} gvizCreateAlgo;

typedef enum gvizCreateSource {
  GVIZ_SRC_DEMO_SIERPINSKI = 0,
  GVIZ_SRC_DEMO_OCTAHEDRON,
  GVIZ_SRC_DEMO_RANDOM_TREE,
  GVIZ_SRC_FROM_FILE,
} gvizCreateSource;

typedef enum gvizCreateSlotKind {
  GVIZ_SLOT_NEW_EMPTY_SCENE = 0, /* scene currently has no component layers */
  GVIZ_SLOT_SPLIT_H,             /* split active region horizontally */
  GVIZ_SLOT_SPLIT_V,             /* split active region vertically */
} gvizCreateSlotKind;

typedef enum gvizCreatePanelResult {
  GVIZ_CREATE_PANEL_PENDING = 0,
  GVIZ_CREATE_PANEL_CONFIRMED,
  GVIZ_CREATE_PANEL_CANCELLED,
} gvizCreatePanelResult;

typedef struct gvizLayerCreateParams {
  gvizCreateSlotKind slotKind;
  gvizSceneMode      mode;       /* 2D / 3D */
  gvizCreateAlgo     algo;
  gvizCreateSource   source;
  char               filepath[512];
} gvizLayerCreateParams;

typedef struct gvizLayerCreatePanel {
  gvizLayer layer;                 /* MUST be first */
  gvizLayerCreateParams params;
  gvizCreatePanelResult result;

  /* internal raygui state */
  int  algoDropdownEdit;
  int  sourceDropdownEdit;
  int  filepathEdit;
} gvizLayerCreatePanel;

void gvizLayerCreatePanelInit(gvizLayerCreatePanel *p,
                              gvizCreateSlotKind slotKind, size_t z);

void gvizLayerCreatePanelDraw(void *layer, const gvizCamera *camera);
void gvizLayerCreatePanelRelease(void *layer);
int  gvizLayerCreatePanelHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_LAYER_CREATE_PANEL_VTABLE = {
    .draw      = gvizLayerCreatePanelDraw,
    .update    = NULL,
    .release   = gvizLayerCreatePanelRelease,
    .onEvent   = gvizLayerCreatePanelHandleEvent,
    .hitTest   = NULL,
    .getCamera = NULL,
};

#endif
