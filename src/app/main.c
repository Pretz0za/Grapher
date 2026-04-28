#include "app/gvizContextMenu.h"
#include "app/gvizLayerCreate.h"
#include "app/gvizLayerCreatePanel.h"
#include "app/gvizOBJLoadModal.h"
#include "app/gvizSceneBuilders.h"
#include "core/alloc.h"
#include "core/gvizScene.h"
#include "platform/macos_menu.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_W 1280
#define WINDOW_H 800

/* Action ids for context-menu entries. */
enum {
  ACT_CREATE_NEW = 1,
  ACT_SPLIT_H,
  ACT_SPLIT_V,
};

typedef struct AppState {
  gvizContextMenu      *menu;        /* heap-allocated, owned by scene */
  gvizLayerCreatePanel *panel;       /* heap-allocated, owned by scene */
  gvizCreateSlotKind    pendingSlot; /* what kind of slot the panel will fill */
} AppState;

static gvizOBJLoadModal *installOBJModal(gvizScene *scene, const char *path) {
  gvizOBJLoadModal *m = GVIZ_ALLOC(sizeof(gvizOBJLoadModal));
  if (!m) return NULL;
  gvizOBJLoadModalInit(m, path, 2000);
  gvizSceneAddLayer(scene, (gvizLayer *)m);
  return m;
}

static void buildFromOBJChoice(gvizScene *scene, gvizOBJModalChoice choice,
                               const char *path) {
  gvizSceneRelease(scene);
  switch (choice) {
  case GVIZ_OBJ_MODAL_OBJ_ONLY:
    if (gvizBuildOBJSceneFromFile(scene, path) != 0)
      gvizSceneInitEmpty(scene);
    break;
  case GVIZ_OBJ_MODAL_POLYTUTTE_ONLY:
    if (gvizBuildPolyTutteFromOBJScene(scene, path) != 0)
      gvizSceneInitEmpty(scene);
    break;
  case GVIZ_OBJ_MODAL_BOTH:
    if (gvizBuildOBJAndPolyTutteSceneFromFile(scene, path) != 0)
      gvizSceneInitEmpty(scene);
    break;
  case GVIZ_OBJ_MODAL_CANCELLED:
  default:
    gvizSceneInitEmpty(scene);
    break;
  }
}

static void onEmptyAreaContextMenu(gvizScene *s, int sx, int sy, void *ud) {
  AppState *app = (AppState *)ud;
  if (app->menu || app->panel) return;
  gvizContextMenu *m = GVIZ_ALLOC(sizeof(gvizContextMenu));
  if (!m) return;
  gvizContextMenuInit(m, sx, sy, 1500);
  gvizContextMenuAddEntry(m, "Create new layer", ACT_CREATE_NEW);
  m->targetLayer = NULL;
  m->clickSx = sx; m->clickSy = sy;
  app->menu = m;
  gvizSceneAddLayer(s, (gvizLayer *)m);
}

static void onLayerContextMenu(gvizScene *s, gvizLayer *layer, int sx, int sy,
                               void *ud) {
  AppState *app = (AppState *)ud;
  if (app->menu || app->panel) return;
  gvizContextMenu *m = GVIZ_ALLOC(sizeof(gvizContextMenu));
  if (!m) return;
  gvizContextMenuInit(m, sx, sy, 1500);
  gvizContextMenuAddEntry(m, "Split Horizontal", ACT_SPLIT_H);
  gvizContextMenuAddEntry(m, "Split Vertical",   ACT_SPLIT_V);
  m->targetLayer = layer;
  m->clickSx = sx; m->clickSy = sy;
  app->menu = m;
  gvizSceneAddLayer(s, (gvizLayer *)m);
}

static void openCreatePanel(gvizScene *scene, AppState *app,
                            gvizCreateSlotKind slotKind) {
  gvizLayerCreatePanel *p = GVIZ_ALLOC(sizeof(gvizLayerCreatePanel));
  if (!p) return;
  gvizLayerCreatePanelInit(p, slotKind, 1800);
  app->panel = p;
  app->pendingSlot = slotKind;
  gvizSceneAddLayer(scene, (gvizLayer *)p);
}

static void drainContextMenu(gvizScene *scene, AppState *app) {
  if (!app->menu) return;
  if (app->menu->result == 0) return;
  int result = app->menu->result;
  gvizSceneRemoveLayer(scene, (gvizLayer *)app->menu);
  app->menu = NULL;
  if (result == ACT_CREATE_NEW)
    openCreatePanel(scene, app, GVIZ_SLOT_NEW_EMPTY_SCENE);
  else if (result == ACT_SPLIT_H)
    openCreatePanel(scene, app, GVIZ_SLOT_SPLIT_H);
  else if (result == ACT_SPLIT_V)
    openCreatePanel(scene, app, GVIZ_SLOT_SPLIT_V);
}

static void drainCreatePanel(gvizScene *scene, AppState *app) {
  if (!app->panel) return;
  if (app->panel->result == GVIZ_CREATE_PANEL_PENDING) return;
  if (app->panel->result == GVIZ_CREATE_PANEL_CONFIRMED)
    gvizApplyLayerCreate(scene, &app->panel->params);
  gvizSceneRemoveLayer(scene, (gvizLayer *)app->panel);
  app->panel = NULL;
}

int main(void) {
  srand((unsigned)time(NULL));
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(WINDOW_W, WINDOW_H, "Grapher");
  SetTargetFPS(60);

  gvizPlatformMenuInit();

  gvizScene scene;
  gvizSceneInitEmpty(&scene);

  AppState app = {0};
  scene.contextMenuUserdata    = &app;
  scene.onEmptyAreaContextMenu = onEmptyAreaContextMenu;
  scene.onLayerContextMenu     = onLayerContextMenu;

  gvizOBJLoadModal *objModal = NULL;

  while (!WindowShouldClose()) {
    gvizSceneHandleInput(&scene);
    gvizSceneUpdate(&scene, GetFrameTime());
    gvizSceneDraw(&scene);

    drainContextMenu(&scene, &app);
    drainCreatePanel(&scene, &app);

    /* Drain a freshly picked .obj path: present the layout-choice modal. */
    if (!objModal) {
      char *objPath = gvizPlatformMenuPollPendingOBJPath();
      if (objPath) {
        objModal = installOBJModal(&scene, objPath);
        gvizPlatformMenuFreePath(objPath);
      }
    }

    if (objModal && objModal->result != GVIZ_OBJ_MODAL_PENDING) {
      gvizOBJModalChoice choice = objModal->result;
      char path[1024];
      strncpy(path, objModal->objPath, sizeof(path));
      path[sizeof(path) - 1] = '\0';
      buildFromOBJChoice(&scene, choice, path);
      objModal = NULL;
      app.menu = NULL;
      app.panel = NULL;
      scene.contextMenuUserdata    = &app;
      scene.onEmptyAreaContextMenu = onEmptyAreaContextMenu;
      scene.onLayerContextMenu     = onLayerContextMenu;
    }
  }

  gvizSceneRelease(&scene);
  CloseWindow();
  return 0;
}
