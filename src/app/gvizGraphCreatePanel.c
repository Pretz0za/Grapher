#include "app/gvizGraphCreatePanel.h"
#include "app/gvizDemoGraphLoad.h"
#include "core/alloc.h"
#include "core/event.h"
#include "raygui.h"
#include "raylib.h"
#include <stdlib.h>

#define PANEL_W 460
#define PANEL_H 280
#define ROW_H   28
#define LABEL_W 100
#define GAP     10

static const char *GRAPH_TYPE_LIST =
    "Sierpinski triangle;Sierpinski tetrahedron;Sierpinski carpet;"
    "Rect mesh;Tetrahedral mesh;Equilateral tri mesh;Knotted rect mesh;"
    "Mobius strip;Klein bottle;Random tree";

static int graphTypeUsesParam2(gvizDemoGraphType t) {
  switch (t) {
  case GVIZ_DEMO_RECT_MESH:
  case GVIZ_DEMO_KNOTTED_RECT:
  case GVIZ_DEMO_MOBIUS:
  case GVIZ_DEMO_KLEIN:
    return 1;
  default:
    return 0;
  }
}

static const char *graphTypeParam1Label(gvizDemoGraphType t) {
  switch (t) {
  case GVIZ_DEMO_SIERPINSKI_TRI:
  case GVIZ_DEMO_SIERPINSKI_TET:
  case GVIZ_DEMO_CARPET:
  case GVIZ_DEMO_TET_MESH:
  case GVIZ_DEMO_EQ_TRI_MESH:
    return "Depth";
  case GVIZ_DEMO_RECT_MESH:
  case GVIZ_DEMO_KNOTTED_RECT:
    return "L";
  case GVIZ_DEMO_MOBIUS:
  case GVIZ_DEMO_KLEIN:
    return "Rows";
  case GVIZ_DEMO_RANDOM_TREE:
    return "Max depth";
  default:
    return "Param";
  }
}

static const char *graphTypeParam2Label(gvizDemoGraphType t) {
  switch (t) {
  case GVIZ_DEMO_RECT_MESH:
  case GVIZ_DEMO_KNOTTED_RECT:
    return "W";
  case GVIZ_DEMO_MOBIUS:
  case GVIZ_DEMO_KLEIN:
    return "Cols";
  default:
    return "";
  }
}

void gvizGraphCreatePanelInit(gvizGraphCreatePanel *p, size_t z) {
  gvizLayerInit((gvizLayer *)p, (gvizViewport){0, 0, 0, 0},
                &GVIZ_GRAPH_CREATE_PANEL_VTABLE, z);
  p->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE |
                   GVIZ_LAYER_CAPTURES_INPUT;
  p->result = GVIZ_GRAPH_PANEL_PENDING;
  p->createdGraph = GVIZ_SCENE_GRAPH_INVALID;
  p->params.graphType = GVIZ_DEMO_SIERPINSKI_TRI;
  p->params.graphParam1 = 3;
  p->params.graphParam2 = 3;
  p->graphTypeDropdownEdit = 0;
  p->param1Edit = 0;
  p->param2Edit = 0;
}

void gvizGraphCreatePanelDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizGraphCreatePanel *p = (gvizGraphCreatePanel *)layerV;
  if (p->result != GVIZ_GRAPH_PANEL_PENDING) return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float px = (sw - PANEL_W) / 2.0f;
  float py = (sh - PANEL_H) / 2.0f;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 96});
  if (GuiWindowBox((Rectangle){px, py, PANEL_W, PANEL_H}, "New Graph"))
    p->result = GVIZ_GRAPH_PANEL_CANCELLED;

  float y = py + 36;

  GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "Graph type");
  Rectangle graphTypeRect = {px + GAP + LABEL_W, y,
                             PANEL_W - LABEL_W - GAP * 2, ROW_H};
  y += ROW_H + GAP;

  GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H},
           graphTypeParam1Label(p->params.graphType));
  if (GuiSpinner((Rectangle){px + GAP + LABEL_W, y,
                             PANEL_W - LABEL_W - GAP * 2, ROW_H},
                 NULL, &p->params.graphParam1, 1, 64, p->param1Edit))
    p->param1Edit = !p->param1Edit;
  y += ROW_H + GAP;

  if (graphTypeUsesParam2(p->params.graphType)) {
    GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H},
             graphTypeParam2Label(p->params.graphType));
    if (GuiSpinner((Rectangle){px + GAP + LABEL_W, y,
                               PANEL_W - LABEL_W - GAP * 2, ROW_H},
                   NULL, &p->params.graphParam2, 1, 64, p->param2Edit))
      p->param2Edit = !p->param2Edit;
    y += ROW_H + GAP;
  }

  Rectangle okRect = {px + PANEL_W - GAP - 110, py + PANEL_H - GAP - ROW_H,
                      110, ROW_H};
  Rectangle cancelRect = {px + PANEL_W - GAP - 230, py + PANEL_H - GAP - ROW_H,
                          110, ROW_H};
  if (GuiButton(cancelRect, "Cancel"))
    p->result = GVIZ_GRAPH_PANEL_CANCELLED;
  if (GuiButton(okRect, "Create"))
    p->result = GVIZ_GRAPH_PANEL_CONFIRMED;

  int gt = (int)p->params.graphType;
  if (GuiDropdownBox(graphTypeRect, GRAPH_TYPE_LIST, &gt,
                     p->graphTypeDropdownEdit))
    p->graphTypeDropdownEdit = !p->graphTypeDropdownEdit;
  p->params.graphType = (gvizDemoGraphType)gt;
}

void gvizGraphCreatePanelRelease(void *layer) { (void)layer; }

int gvizGraphCreatePanelHandleEvent(void *layerV, const gvizEvent *event) {
  gvizGraphCreatePanel *p = (gvizGraphCreatePanel *)layerV;
  if (event->type == GVIZ_EVENT_KEY_DOWN && event->key.key == KEY_ESCAPE) {
    p->result = GVIZ_GRAPH_PANEL_CANCELLED;
    return 1;
  }
  return 0;
}

gvizSceneGraphHandle gvizApplyGraphCreate(gvizScene *scene,
                                          const gvizGraphCreateParams *params) {
  if (!scene || !params) return GVIZ_SCENE_GRAPH_INVALID;
  gvizLayerCreateParams shim = {0};
  shim.graphType = params->graphType;
  shim.graphParam1 = params->graphParam1;
  shim.graphParam2 = params->graphParam2;
  gvizGraph stack;
  if (gvizLoadDemoGraph(&shim, &stack, NULL, NULL, NULL) != 0)
    return GVIZ_SCENE_GRAPH_INVALID;
  gvizGraph *g = gvizGraphToHeap(&stack);
  if (!g) return GVIZ_SCENE_GRAPH_INVALID;
  gvizSceneGraphHandle h = gvizSceneRegisterGraph(scene, g);
  if (h == GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphRelease(g);
    GVIZ_DEALLOC(g);
    return GVIZ_SCENE_GRAPH_INVALID;
  }
  return h;
}
