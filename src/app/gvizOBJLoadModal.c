#include "app/gvizOBJLoadModal.h"
#include "raygui.h"
#include "raylib.h"
#include <string.h>

#define MODAL_W 380
#define MODAL_H 220
#define BTN_H   36
#define BTN_GAP 10

void gvizOBJLoadModalInit(gvizOBJLoadModal *m, const char *objPath, size_t z) {
  gvizLayerInit((gvizLayer *)m, (gvizViewport){0, 0, 0, 0},
                &GVIZ_OBJ_LOAD_MODAL_VTABLE, z);
  m->layer.flags = GVIZ_LAYER_VISIBLE | GVIZ_LAYER_SCREEN_SPACE |
                   GVIZ_LAYER_CAPTURES_INPUT;
  m->result = GVIZ_OBJ_MODAL_PENDING;
  m->objPath[0] = '\0';
  if (objPath) {
    strncpy(m->objPath, objPath, sizeof(m->objPath) - 1);
    m->objPath[sizeof(m->objPath) - 1] = '\0';
  }
}

void gvizOBJLoadModalDraw(void *layerV, const gvizCamera *camera) {
  (void)camera;
  gvizOBJLoadModal *m = (gvizOBJLoadModal *)layerV;
  int sw = GetScreenWidth();
  int sh = GetScreenHeight();
  float px = (sw - MODAL_W) / 2.0f;
  float py = (sh - MODAL_H) / 2.0f;

  /* Dim backdrop. */
  DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 96});

  if (GuiWindowBox((Rectangle){px, py, MODAL_W, MODAL_H}, "Load OBJ — choose layout") )
    m->result = GVIZ_OBJ_MODAL_CANCELLED;

  const char *base = m->objPath;
  for (const char *p = m->objPath; *p; p++)
    if (*p == '/') base = p + 1;
  GuiLabel((Rectangle){px + 16, py + 32, MODAL_W - 32, 18}, base);

  float by = py + 60;
  if (GuiButton((Rectangle){px + 16, by, MODAL_W - 32, BTN_H}, "OBJ mesh only (3D)"))
    m->result = GVIZ_OBJ_MODAL_OBJ_ONLY;
  by += BTN_H + BTN_GAP;
  if (GuiButton((Rectangle){px + 16, by, MODAL_W - 32, BTN_H}, "PolyTutte embedding only (2D)"))
    m->result = GVIZ_OBJ_MODAL_POLYTUTTE_ONLY;
  by += BTN_H + BTN_GAP;
  if (GuiButton((Rectangle){px + 16, by, MODAL_W - 32, BTN_H}, "Both side-by-side"))
    m->result = GVIZ_OBJ_MODAL_BOTH;
}

void gvizOBJLoadModalRelease(void *layer) { (void)layer; }

int gvizOBJLoadModalHandleEvent(void *layer, const gvizEvent *event) {
  (void)layer;
  (void)event;
  return 0;
}
