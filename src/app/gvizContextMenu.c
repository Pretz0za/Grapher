#include "app/gvizContextMenu.h"
#include "core/event.h"
#include "raygui.h"
#include "raylib.h"
#include <string.h>

#define CTX_MENU_W 180
#define CTX_ENTRY_H 28
#define CTX_PAD 4

void gvizContextMenuInit(gvizContextMenu *m, int anchorX, int anchorY,
                         size_t z) {
  gvizLayerInit((gvizLayer *)m, (gvizViewport){0, 0, 0, 0},
                &GVIZ_CONTEXT_MENU_VTABLE, z);
  m->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE |
                   GVIZ_LAYER_CAPTURES_INPUT;
  m->count = 0;
  m->anchorX = anchorX;
  m->anchorY = anchorY;
  m->result = 0;
  m->targetLayer = NULL;
  m->clickSx = anchorX;
  m->clickSy = anchorY;
}

int gvizContextMenuAddEntry(gvizContextMenu *m, const char *label,
                            int actionId) {
  if (!m || !label || actionId <= 0) return -1;
  if (m->count >= GVIZ_CONTEXT_MENU_MAX_ENTRIES) return -1;
  gvizContextMenuEntry *e = &m->entries[m->count];
  strncpy(e->label, label, GVIZ_CONTEXT_MENU_MAX_LABEL - 1);
  e->label[GVIZ_CONTEXT_MENU_MAX_LABEL - 1] = '\0';
  e->actionId = actionId;
  m->count++;
  return 0;
}

static Rectangle menuRect(const gvizContextMenu *m) {
  int h = CTX_PAD * 2 + m->count * CTX_ENTRY_H;
  return (Rectangle){(float)m->anchorX, (float)m->anchorY,
                     (float)CTX_MENU_W, (float)h};
}

void gvizContextMenuDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizContextMenu *m = (gvizContextMenu *)layerV;
  if (m->result != 0) return;
  Rectangle r = menuRect(m);
  /* Clamp to screen. */
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  if (r.x + r.width > sw)  r.x = sw - r.width;
  if (r.y + r.height > sh) r.y = sh - r.height;
  if (r.x < 0) r.x = 0;
  if (r.y < 0) r.y = 0;
  m->anchorX = (int)r.x;
  m->anchorY = (int)r.y;

  DrawRectangleRec(r, (Color){250, 250, 250, 245});
  DrawRectangleLinesEx(r, 1.0f, (Color){120, 120, 120, 255});

  for (int i = 0; i < m->count; i++) {
    Rectangle eb = {r.x + CTX_PAD, r.y + CTX_PAD + i * CTX_ENTRY_H,
                    r.width - CTX_PAD * 2, CTX_ENTRY_H};
    if (GuiButton(eb, m->entries[i].label))
      m->result = m->entries[i].actionId;
  }
}

void gvizContextMenuRelease(void *layer) { (void)layer; }

int gvizContextMenuHandleEvent(void *layerV, const gvizEvent *event) {
  gvizContextMenu *m = (gvizContextMenu *)layerV;
  if (event->type == GVIZ_EVENT_KEY_DOWN && event->key.key == KEY_ESCAPE) {
    m->result = -1;
    return 1;
  }
  if (event->type == GVIZ_EVENT_MOUSE_DOWN) {
    Rectangle r = menuRect(m);
    int sx = (int)event->mouse.sx;
    int sy = (int)event->mouse.sy;
    int inside = sx >= r.x && sx < r.x + r.width &&
                 sy >= r.y && sy < r.y + r.height;
    if (!inside) {
      m->result = -1;
    }
    /* CAPTURES_INPUT swallows the click either way. */
  }
  return 0;
}
