#ifndef _GVIZ_APP_CONTEXT_MENU_H_
#define _GVIZ_APP_CONTEXT_MENU_H_

#include "renderer/layers/gvizLayer.h"

#define GVIZ_CONTEXT_MENU_MAX_ENTRIES 8
#define GVIZ_CONTEXT_MENU_MAX_LABEL   48

/*
 * Right-click context menu — a small screen-space list drawn at a click
 * location. The owner reads `result`:
 *   - 0  = pending (still open)
 *   - -1 = cancelled (clicked outside or pressed Esc)
 *   - >0 = selected actionId.
 */
typedef struct gvizContextMenuEntry {
  char label[GVIZ_CONTEXT_MENU_MAX_LABEL];
  int  actionId;       /* must be > 0 */
} gvizContextMenuEntry;

typedef struct gvizContextMenu {
  gvizLayer layer;          /* MUST be first */
  gvizContextMenuEntry entries[GVIZ_CONTEXT_MENU_MAX_ENTRIES];
  int  count;
  int  anchorX, anchorY;    /* screen-space top-left of the menu */
  int  result;              /* 0 pending, -1 cancelled, >0 selected actionId */

  /* Caller-stashed click context (so the orchestrator knows which layer +
   * where the user right-clicked when the menu resolves). */
  void *targetLayer;        /* gvizLayer * — NULL for empty-area menus */
  int  clickSx, clickSy;
} gvizContextMenu;

void gvizContextMenuInit(gvizContextMenu *m, int anchorX, int anchorY,
                         size_t z);
int  gvizContextMenuAddEntry(gvizContextMenu *m, const char *label,
                             int actionId);

void gvizContextMenuDraw(void *layer, const gvizCamera *camera);
void gvizContextMenuRelease(void *layer);
int  gvizContextMenuHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_CONTEXT_MENU_VTABLE = {
    .draw      = gvizContextMenuDraw,
    .update    = NULL,
    .release   = gvizContextMenuRelease,
    .onEvent   = gvizContextMenuHandleEvent,
    .hitTest   = NULL,
    .getCamera = NULL,
};

#endif
