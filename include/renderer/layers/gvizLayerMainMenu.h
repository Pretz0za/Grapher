#ifndef _GVIZ_LAYER_MAIN_MENU_H_
#define _GVIZ_LAYER_MAIN_MENU_H_

#include "renderer/layers/gvizLayer.h"

/*
 * The main menu emits one of these actions when a button is clicked. The
 * scene-owner inspects `requestedAction` after each frame and transitions
 * scenes accordingly. The menu never performs the scene transition itself.
 */
typedef enum gvizMainMenuAction {
  GVIZ_MENU_NONE = 0,
  GVIZ_MENU_LOAD_SCENE,
  GVIZ_MENU_DEMO_GRIP_SIERPINSKI,
  GVIZ_MENU_DEMO_GRIP_CARPET,
  GVIZ_MENU_DEMO_TUTTE,
  GVIZ_MENU_DEMO_TREE,
  GVIZ_MENU_BLANK_SCENE,
} gvizMainMenuAction;

typedef struct gvizLayerMainMenu {
  gvizLayer layer;
  gvizMainMenuAction requestedAction;
  char loadPath[512];
} gvizLayerMainMenu;

/*
 * Initialize @p layer as a screen-space main-menu layer covering the given
 * viewport. The menu is a raygui panel with three buttons.
 */
void gvizLayerMainMenuInit(gvizLayerMainMenu *layer,
                           const gvizViewport viewport, size_t z);

void gvizLayerMainMenuDraw(void *layer, const gvizCamera *camera);
void gvizLayerMainMenuUpdate(void *layer, float dt);
void gvizLayerMainMenuRelease(void *layer);
int gvizLayerMainMenuHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_LAYER_MAIN_MENU_VTABLE = {
    .draw = gvizLayerMainMenuDraw,
    .update = gvizLayerMainMenuUpdate,
    .release = gvizLayerMainMenuRelease,
    .onEvent = gvizLayerMainMenuHandleEvent,
    .hitTest = NULL,
};

#endif
