#include "renderer/layers/gvizLayerMainMenu.h"
#include "raygui.h"
#include "raylib.h"
#include <string.h>

#define PANEL_W 320
#define BTN_H   40
#define BTN_GAP 10
#define BTN_COUNT 6
#define PANEL_H (48 + BTN_COUNT * (BTN_H + BTN_GAP) - BTN_GAP + 16)

void gvizLayerMainMenuInit(gvizLayerMainMenu *layer,
                           const gvizViewport viewport, size_t z) {
  gvizLayerInit((gvizLayer *)layer, viewport, &GVIZ_LAYER_MAIN_MENU_VTABLE, z);
  layer->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE |
                       GVIZ_LAYER_CAPTURES_INPUT;
  layer->requestedAction = GVIZ_MENU_NONE;
  layer->loadPath[0] = '\0';
}

void gvizLayerMainMenuDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizLayerMainMenu *l = (gvizLayerMainMenu *)layerV;
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float px = (sw - PANEL_W) / 2.0f;
  float py = (sh - PANEL_H) / 2.0f;

  GuiPanel((Rectangle){px, py, PANEL_W, PANEL_H}, "Grapher");

  float by = py + 48;
#define MBTN(label, action) \
  if (GuiButton((Rectangle){px + 16, by, PANEL_W - 32, BTN_H}, label)) \
    l->requestedAction = (action); \
  by += BTN_H + BTN_GAP;

  MBTN("Load Scene",              GVIZ_MENU_LOAD_SCENE)
  MBTN("Demo: GRIP Sierpinski",   GVIZ_MENU_DEMO_GRIP_SIERPINSKI)
  MBTN("Demo: GRIP Carpet",       GVIZ_MENU_DEMO_GRIP_CARPET)
  MBTN("Demo: Tutte Spider-Web",  GVIZ_MENU_DEMO_TUTTE)
  MBTN("Demo: Random Tree",       GVIZ_MENU_DEMO_TREE)
  MBTN("Blank Scene",             GVIZ_MENU_BLANK_SCENE)
#undef MBTN
}

void gvizLayerMainMenuUpdate(void *layer, float dt) {
  (void)layer;
  (void)dt;
}

void gvizLayerMainMenuRelease(void *layer) { (void)layer; }

int gvizLayerMainMenuHandleEvent(void *layer, const gvizEvent *event) {
  (void)layer;
  (void)event;
  /* Menu consumes mouse events that fall within the panel via
   * CAPTURES_INPUT flag; the panel rendering is handled by raygui in draw. */
  return 0;
}
