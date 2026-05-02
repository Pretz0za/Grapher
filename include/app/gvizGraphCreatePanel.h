#ifndef _GVIZ_APP_GRAPH_CREATE_PANEL_H_
#define _GVIZ_APP_GRAPH_CREATE_PANEL_H_

#include "app/gvizLayerCreatePanel.h"
#include "core/gvizScene.h"
#include "renderer/layers/gvizLayer.h"

/*
 * "New Graph" modal — picks a demo graph type + parameters and registers
 * the graph with the scene. No layer is created. The orchestrator polls
 * `result` after each frame; on confirm the chosen handle is in
 * `createdGraph` (also returned by gvizApplyGraphCreate).
 */

typedef enum gvizGraphCreatePanelResult {
  GVIZ_GRAPH_PANEL_PENDING = 0,
  GVIZ_GRAPH_PANEL_CONFIRMED,
  GVIZ_GRAPH_PANEL_CANCELLED,
} gvizGraphCreatePanelResult;

typedef struct gvizGraphCreateParams {
  gvizDemoGraphType graphType;
  int               graphParam1;
  int               graphParam2;
} gvizGraphCreateParams;

typedef struct gvizGraphCreatePanel {
  gvizLayer layer;                        /* MUST be first */
  gvizGraphCreateParams params;
  gvizGraphCreatePanelResult result;
  gvizSceneGraphHandle createdGraph;      /* set on confirm; INVALID otherwise */

  int graphTypeDropdownEdit;
  int param1Edit;
  int param2Edit;
} gvizGraphCreatePanel;

void gvizGraphCreatePanelInit(gvizGraphCreatePanel *p, size_t z);

void gvizGraphCreatePanelDraw(void *layer, const gvizCamera *camera);
void gvizGraphCreatePanelRelease(void *layer);
int  gvizGraphCreatePanelHandleEvent(void *layer, const gvizEvent *event);

/*
 * Build a graph from @p params, register it with @p scene, return its
 * handle. Returns GVIZ_SCENE_GRAPH_INVALID on failure.
 */
gvizSceneGraphHandle gvizApplyGraphCreate(gvizScene *scene,
                                          const gvizGraphCreateParams *params);

static const gvizLayerVTable GVIZ_GRAPH_CREATE_PANEL_VTABLE = {
    .draw      = gvizGraphCreatePanelDraw,
    .update    = NULL,
    .release   = gvizGraphCreatePanelRelease,
    .onEvent   = gvizGraphCreatePanelHandleEvent,
    .hitTest   = NULL,
    .getCamera = NULL,
};

#endif
