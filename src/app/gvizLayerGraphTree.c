#include "app/gvizLayerGraphTree.h"
#include "core/event.h"
#include "core/gvizScene.h"
#include "dsa/gvizGraph.h"
#include "raygui.h"
#include "raylib.h"
#include <stdio.h>
#include <string.h>

#define GT_PANEL_PAD 8
#define GT_ROW_H 22
#define GT_INDENT 14

static int panelWidth(void) { return GVIZ_SCENE_MARGIN_L; }

void gvizLayerGraphTreeInit(gvizLayerGraphTree *self, gvizScene *s, size_t z) {
  gvizLayerInit((gvizLayer *)self,
                (gvizViewport){0, 0, panelWidth(), GetScreenHeight()},
                &GVIZ_LAYER_GRAPH_TREE_VTABLE, z);
  self->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE;
  self->scene = s;
  self->scrollY = 0.0f;
}

void gvizLayerGraphTreeUpdate(void *layer, float dt) {
  (void)dt;
  gvizLayerGraphTree *self = (gvizLayerGraphTree *)layer;
  self->layer.viewport.height = GetScreenHeight();
  self->layer.viewport.width = panelWidth();
}

void gvizLayerGraphTreeRelease(void *layer) { (void)layer; }

static void rowText(int x, int y, int w, const char *text, Color color) {
  Rectangle rb = {(float)x, (float)y, (float)w, (float)GT_ROW_H};
  DrawRectangleLinesEx(rb, 1.0f, (Color){220, 220, 220, 255});
  DrawText(text, x + 4, y + 4, 12, color);
}

void gvizLayerGraphTreeDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerGraphTree *self = (gvizLayerGraphTree *)layerV;
  if (!self->scene) return;
  int W = panelWidth();
  int H = GetScreenHeight();

  Rectangle bg = {0, 0, (float)W, (float)H};
  DrawRectangleRec(bg, (Color){240, 240, 240, 255});
  DrawLine(W - 1, 0, W - 1, H, (Color){180, 180, 180, 255});

  DrawText("Graphs / Views", GT_PANEL_PAD, GT_PANEL_PAD, 14,
           (Color){40, 40, 40, 255});

  int y = GT_PANEL_PAD + 22;
  gvizArray *graphs = &self->scene->graphs;
  if (!graphs->arr) return;

  char buf[128];
  for (size_t i = 1; i < graphs->count; i++) {
    gvizSceneGraphEntry *e =
        (gvizSceneGraphEntry *)gvizArrayAtIndex(graphs, i);
    if (!e->graph) continue;

    snprintf(buf, sizeof(buf), "Graph #%zu (%zu v)", i,
             e->graph->vertices.count);
    rowText(GT_PANEL_PAD, y, W - 2 * GT_PANEL_PAD, buf,
            (Color){20, 20, 20, 255});
    y += GT_ROW_H + 2;

    if (!e->views.arr) continue;
    for (size_t v = 0; v < e->views.count; v++) {
      gvizSceneViewEntry *ve =
          (gvizSceneViewEntry *)gvizArrayAtIndex(&e->views, v);
      const char *name = ve->name ? ve->name : "view";
      snprintf(buf, sizeof(buf), "  %s (%zu v)", name,
               ve->view ? ve->view->count : 0);
      Color c = (self->scene->activeLayer == ve->layer)
                    ? (Color){10, 80, 200, 255}
                    : (Color){60, 60, 60, 255};
      Rectangle rb = {GT_PANEL_PAD + GT_INDENT, (float)y,
                      (float)(W - 2 * GT_PANEL_PAD - GT_INDENT),
                      (float)GT_ROW_H};
      if (GuiButton(rb, buf)) {
        if (ve->layer)
          gvizSceneSetActiveLayer(self->scene, ve->layer);
      }
      (void)c;
      y += GT_ROW_H + 2;
    }
  }
}

int gvizLayerGraphTreeHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerGraphTree *self = (gvizLayerGraphTree *)layerV;
  if (!self->scene || !event) return 0;
  if (event->type == GVIZ_EVENT_MOUSE_DOWN) {
    int sx = (int)event->mouse.sx;
    if (sx >= 0 && sx < panelWidth())
      return 1;
  }
  return 0;
}
