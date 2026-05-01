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
#define GT_CHEVRON_W 16

static int panelWidth(void) { return GVIZ_SCENE_MARGIN_L; }

void gvizLayerGraphTreeInit(gvizLayerGraphTree *self, gvizScene *s, size_t z) {
  gvizLayerInit((gvizLayer *)self,
                (gvizViewport){0, 0, panelWidth(), GetScreenHeight()},
                &GVIZ_LAYER_GRAPH_TREE_VTABLE, z);
  self->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE;
  self->scene = s;
  self->scrollY = 0.0f;
  gvizArrayInit(&self->collapsed, sizeof(unsigned char));
}

void gvizLayerGraphTreeUpdate(void *layer, float dt) {
  (void)dt;
  gvizLayerGraphTree *self = (gvizLayerGraphTree *)layer;
  self->layer.viewport.height = GetScreenHeight();
  self->layer.viewport.width = panelWidth();
}

void gvizLayerGraphTreeRelease(void *layer) {
  gvizLayerGraphTree *self = (gvizLayerGraphTree *)layer;
  gvizArrayRelease(&self->collapsed);
}

static void ensureCollapsedCapacity(gvizLayerGraphTree *self, size_t need) {
  unsigned char zero = 0;
  while (self->collapsed.count < need) {
    gvizArrayPush(&self->collapsed, &zero);
  }
}

static void syncCollapsedToRegistry(gvizLayerGraphTree *self) {
  gvizArray *graphs = &self->scene->graphs;
  if (!graphs->arr) return;
  ensureCollapsedCapacity(self, graphs->count);
  for (size_t i = 1; i < graphs->count && i < self->collapsed.count; i++) {
    gvizSceneGraphEntry *e =
        (gvizSceneGraphEntry *)gvizArrayAtIndex(graphs, i);
    if (!e->graph) {
      unsigned char *slot =
          (unsigned char *)gvizArrayAtIndex(&self->collapsed, i);
      *slot = 0;
    }
  }
}

static int contentHeightPx(gvizLayerGraphTree *self) {
  gvizArray *graphs = &self->scene->graphs;
  if (!graphs->arr) return 0;
  int rows = 0;
  for (size_t i = 1; i < graphs->count; i++) {
    gvizSceneGraphEntry *e =
        (gvizSceneGraphEntry *)gvizArrayAtIndex(graphs, i);
    if (!e->graph) continue;
    rows++;
    int collapsed = 0;
    if (i < self->collapsed.count) {
      collapsed = *(unsigned char *)gvizArrayAtIndex(&self->collapsed, i);
    }
    if (!collapsed && e->views.arr) rows += (int)e->views.count;
  }
  return rows * (GT_ROW_H + 2);
}

static void clampScroll(gvizLayerGraphTree *self, int panelInner) {
  int content = contentHeightPx(self);
  float maxScroll = (float)(content - panelInner);
  if (maxScroll < 0) maxScroll = 0;
  if (self->scrollY < 0) self->scrollY = 0;
  if (self->scrollY > maxScroll) self->scrollY = maxScroll;
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

  int headerBottom = GT_PANEL_PAD + 22;
  int panelInner = H - headerBottom;
  syncCollapsedToRegistry(self);
  clampScroll(self, panelInner);

  gvizArray *graphs = &self->scene->graphs;
  if (!graphs->arr) return;

  BeginScissorMode(0, headerBottom, W, panelInner);

  Color guideCol   = (Color){170, 170, 170, 255};
  Color textCol    = (Color){20, 20, 20, 255};
  Color folderCol  = (Color){210, 175, 90, 255};
  Color leafCol    = (Color){90, 140, 210, 255};
  Color activeBg   = (Color){200, 222, 250, 255};
  Color activeOut  = (Color){90, 140, 210, 255};

  char buf[128];
  int y = headerBottom - (int)self->scrollY;
  for (size_t i = 1; i < graphs->count; i++) {
    gvizSceneGraphEntry *e =
        (gvizSceneGraphEntry *)gvizArrayAtIndex(graphs, i);
    if (!e->graph) continue;

    unsigned char *collapsedSlot =
        (i < self->collapsed.count)
            ? (unsigned char *)gvizArrayAtIndex(&self->collapsed, i)
            : NULL;
    int collapsed = collapsedSlot ? *collapsedSlot : 0;

    int graphRowY = y;
    if (y + GT_ROW_H >= headerBottom && y < H) {
      Rectangle rb = {(float)GT_PANEL_PAD, (float)y,
                      (float)(W - 2 * GT_PANEL_PAD), (float)GT_ROW_H};
      const char *chev = collapsed ? ">" : "v";
      DrawText(chev, GT_PANEL_PAD + 4, y + 4, 12, textCol);
      Rectangle iconR = {(float)(GT_PANEL_PAD + 4 + GT_CHEVRON_W),
                         (float)(y + 5), 12.0f, 12.0f};
      DrawRectangleRec(iconR, folderCol);
      DrawRectangleLinesEx(iconR, 1.0f, (Color){140, 110, 50, 255});
      snprintf(buf, sizeof(buf), "Graph #%zu (%zu v)", i,
               e->graph->vertices.count);
      DrawText(buf, GT_PANEL_PAD + 4 + GT_CHEVRON_W + 16, y + 4, 12, textCol);
      if (collapsedSlot && CheckCollisionPointRec(GetMousePosition(), rb) &&
          IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *collapsedSlot = (unsigned char)!*collapsedSlot;
      }
    }
    y += GT_ROW_H + 2;

    if (collapsed || !e->views.arr) continue;

    int guideX = GT_PANEL_PAD + GT_CHEVRON_W / 2 + 4;
    int guideTop = graphRowY + GT_ROW_H;
    int guideBottom = guideTop;
    int childCount = (int)e->views.count;
    if (childCount > 0)
      guideBottom = y + (childCount - 1) * (GT_ROW_H + 2) + GT_ROW_H / 2;
    DrawLine(guideX, guideTop, guideX, guideBottom, guideCol);

    for (size_t v = 0; v < e->views.count; v++) {
      gvizSceneViewEntry *ve =
          (gvizSceneViewEntry *)gvizArrayAtIndex(&e->views, v);
      if (y + GT_ROW_H < headerBottom || y >= H) {
        y += GT_ROW_H + 2;
        continue;
      }
      int rowMid = y + GT_ROW_H / 2;
      DrawLine(guideX, rowMid, GT_PANEL_PAD + GT_INDENT, rowMid, guideCol);

      const char *name = ve->name ? ve->name : "view";
      size_t vcount = ve->view ? ve->view->count : 0;
      if (ve->layer)
        snprintf(buf, sizeof(buf), "%s (%zu v)", name, vcount);
      else
        snprintf(buf, sizeof(buf), "%s (%zu v) (no layer)", name, vcount);

      Rectangle rb = {GT_PANEL_PAD + GT_INDENT, (float)y,
                      (float)(W - 2 * GT_PANEL_PAD - GT_INDENT),
                      (float)GT_ROW_H};
      int isActive = ve->layer && ve->layer == self->scene->activeLayer;
      if (isActive) {
        DrawRectangleRec(rb, activeBg);
        DrawRectangleLinesEx(rb, 1.0f, activeOut);
      }
      Rectangle iconR = {rb.x + 4, (float)(y + 5), 12.0f, 12.0f};
      DrawRectangleRec(iconR, leafCol);
      DrawRectangleLinesEx(iconR, 1.0f, (Color){50, 90, 160, 255});
      DrawText(buf, (int)rb.x + 4 + 16, y + 4, 12, textCol);

      if (CheckCollisionPointRec(GetMousePosition(), rb) &&
          IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (ve->layer)
          gvizSceneSetActiveLayer(self->scene, ve->layer);
      }
      y += GT_ROW_H + 2;
    }
  }

  EndScissorMode();
}

int gvizLayerGraphTreeHandleEvent(void *layerV, const gvizEvent *event) {
  gvizLayerGraphTree *self = (gvizLayerGraphTree *)layerV;
  if (!self->scene || !event) return 0;
  if (event->type == GVIZ_EVENT_MOUSE_DOWN) {
    int sx = (int)event->mouse.sx;
    if (sx >= 0 && sx < panelWidth())
      return 1;
  }
  if (event->type == GVIZ_EVENT_MOUSE_WHEEL) {
    int sx = (int)event->wheel.sx;
    if (sx >= 0 && sx < panelWidth()) {
      self->scrollY -= event->wheel.dy * (float)GT_ROW_H * 3.0f;
      int H = GetScreenHeight();
      int headerBottom = GT_PANEL_PAD + 22;
      clampScroll(self, H - headerBottom);
      return 1;
    }
  }
  return 0;
}
