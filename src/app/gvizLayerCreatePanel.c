#include "app/gvizLayerCreatePanel.h"
#include "core/event.h"
#include "raygui.h"
#include "raylib.h"
#include <string.h>

#define PANEL_W 460
#define PANEL_H 460
#define ROW_H   28
#define LABEL_W 100
#define GAP     10

void gvizLayerCreatePanelInit(gvizLayerCreatePanel *p,
                              gvizCreateSlotKind slotKind, size_t z) {
  gvizLayerInit((gvizLayer *)p, (gvizViewport){0, 0, 0, 0},
                &GVIZ_LAYER_CREATE_PANEL_VTABLE, z);
  p->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE |
                   GVIZ_LAYER_CAPTURES_INPUT;
  p->result = GVIZ_CREATE_PANEL_PENDING;
  p->params.slotKind = slotKind;
  p->params.mode = GVIZ_SCENE_2D;
  p->params.algo = GVIZ_CREATE_TUTTE;
  p->params.source = GVIZ_SRC_DEMO;
  p->params.filepath[0] = '\0';
  p->params.targetLayer = NULL;
  p->params.graphType = GVIZ_DEMO_SIERPINSKI_TRI;
  p->params.graphParam1 = 3;
  p->params.graphParam2 = 3;
  p->params.embDim = 2;
  p->algoDropdownEdit = 0;
  p->sourceDropdownEdit = 0;
  p->graphTypeDropdownEdit = 0;
  p->filepathEdit = 0;
  p->param1Edit = 0;
  p->param2Edit = 0;
  p->embDimEdit = 0;
}

static const char *slotTitle(gvizCreateSlotKind k) {
  switch (k) {
  case GVIZ_SLOT_SPLIT_H: return "Create Layer (Split Horizontal)";
  case GVIZ_SLOT_SPLIT_V: return "Create Layer (Split Vertical)";
  default:                return "Create Layer";
  }
}

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

void gvizLayerCreatePanelDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerCreatePanel *p = (gvizLayerCreatePanel *)layerV;
  if (p->result != GVIZ_CREATE_PANEL_PENDING) return;

  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float px = (sw - PANEL_W) / 2.0f;
  float py = (sh - PANEL_H) / 2.0f;

  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 96});
  if (GuiWindowBox((Rectangle){px, py, PANEL_W, PANEL_H}, slotTitle(p->params.slotKind)))
    p->result = GVIZ_CREATE_PANEL_CANCELLED;

  float y = py + 36;

  /* Mode */
  GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "Mode");
  int mode = (p->params.mode == GVIZ_SCENE_3D) ? 1 : 0;
  GuiToggleGroup((Rectangle){px + GAP + LABEL_W, y,
                             (PANEL_W - LABEL_W - GAP * 2 - GAP) / 2.0f,
                             ROW_H},
                 "2D;3D", &mode);
  p->params.mode = (mode == 1) ? GVIZ_SCENE_3D : GVIZ_SCENE_2D;
  y += ROW_H + GAP;

  /* Algo (rect captured for deferred draw) */
  GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "Algorithm");
  int algo = (int)p->params.algo;
  Rectangle algoRect = {px + GAP + LABEL_W, y, PANEL_W - LABEL_W - GAP * 2, ROW_H};
  y += ROW_H + GAP;

  /* Source (rect captured for deferred draw) */
  GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "Source");
  int src = (int)p->params.source;
  Rectangle srcRect = {px + GAP + LABEL_W, y, PANEL_W - LABEL_W - GAP * 2, ROW_H};
  y += ROW_H + GAP;

  Rectangle graphTypeRect = {0};
  int hasGraphType = 0;

  if (p->params.source == GVIZ_SRC_DEMO) {
    /* Graph-type dropdown — also deferred. */
    GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "Graph type");
    graphTypeRect = (Rectangle){px + GAP + LABEL_W, y,
                                PANEL_W - LABEL_W - GAP * 2, ROW_H};
    hasGraphType = 1;
    y += ROW_H + GAP;

    /* Param spinners (relevant to chosen type). */
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

    /* Embedding dim (only meaningful for GRIP, but always shown). */
    GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "Embed dim");
    if (GuiSpinner((Rectangle){px + GAP + LABEL_W, y,
                               PANEL_W - LABEL_W - GAP * 2, ROW_H},
                   NULL, &p->params.embDim, 2, 8, p->embDimEdit))
      p->embDimEdit = !p->embDimEdit;
    y += ROW_H + GAP;
  } else {
    /* From-file: show path text box. */
    GuiLabel((Rectangle){px + GAP, y, LABEL_W, ROW_H}, "File path");
    Rectangle pathRect = {px + GAP + LABEL_W, y,
                          PANEL_W - LABEL_W - GAP * 2, ROW_H};
    if (GuiTextBox(pathRect, p->params.filepath, sizeof(p->params.filepath),
                   p->filepathEdit))
      p->filepathEdit = !p->filepathEdit;
    y += ROW_H + GAP;
  }

  /* Buttons. */
  Rectangle okRect = {px + PANEL_W - GAP - 110, py + PANEL_H - GAP - ROW_H,
                      110, ROW_H};
  Rectangle cancelRect = {px + PANEL_W - GAP - 230, py + PANEL_H - GAP - ROW_H,
                          110, ROW_H};
  if (GuiButton(cancelRect, "Cancel"))
    p->result = GVIZ_CREATE_PANEL_CANCELLED;
  if (GuiButton(okRect, "Create"))
    p->result = GVIZ_CREATE_PANEL_CONFIRMED;

  /* Dropdowns drawn last so their open list overlays everything. */
  if (hasGraphType) {
    int gt = (int)p->params.graphType;
    if (GuiDropdownBox(graphTypeRect, GRAPH_TYPE_LIST, &gt,
                       p->graphTypeDropdownEdit))
      p->graphTypeDropdownEdit = !p->graphTypeDropdownEdit;
    p->params.graphType = (gvizDemoGraphType)gt;
  }
  if (GuiDropdownBox(srcRect, "Demos;From file", &src, p->sourceDropdownEdit))
    p->sourceDropdownEdit = !p->sourceDropdownEdit;
  p->params.source = (gvizCreateSource)src;

  if (GuiDropdownBox(algoRect, "Tutte;GRIP;PolyTutte;Reingold-Tilford;Empty",
                     &algo, p->algoDropdownEdit))
    p->algoDropdownEdit = !p->algoDropdownEdit;
  p->params.algo = (gvizCreateAlgo)algo;
}

void gvizLayerCreatePanelRelease(void *layer) { (void)layer; }

int gvizLayerCreatePanelHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerCreatePanel *p = (gvizLayerCreatePanel *)layerV;
  if (event->type == GVIZ_EVENT_KEY_DOWN && event->key.key == KEY_ESCAPE) {
    p->result = GVIZ_CREATE_PANEL_CANCELLED;
    return 1;
  }
  return 0;
}
