#include "app/gvizOBJLoadModal.h"
#include "app/gvizSceneBuilders.h"
#include "core/alloc.h"
#include "core/gvizScene.h"
#include "platform/macos_menu.h"
#include "raylib.h"
#include "renderer/layers/gvizLayerMainMenu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_W 1280
#define WINDOW_H 800

static gvizLayerMainMenu *installMainMenu(gvizScene *scene) {
  gvizLayerMainMenu *menu = GVIZ_ALLOC(sizeof(gvizLayerMainMenu));
  if (!menu)
    return NULL;
  gvizViewport vp = {0, 0, GetScreenWidth(), GetScreenHeight()};
  gvizLayerMainMenuInit(menu, vp, 1000);
  gvizSceneAddLayer(scene, (gvizLayer *)menu);
  return menu;
}

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
      gvizBuildBlankScene(scene);
    break;
  case GVIZ_OBJ_MODAL_POLYTUTTE_ONLY:
    if (gvizBuildPolyTutteFromOBJScene(scene, path) != 0)
      gvizBuildBlankScene(scene);
    break;
  case GVIZ_OBJ_MODAL_BOTH:
    if (gvizBuildOBJAndPolyTutteSceneFromFile(scene, path) != 0)
      gvizBuildBlankScene(scene);
    break;
  case GVIZ_OBJ_MODAL_CANCELLED:
  default:
    gvizBuildBlankScene(scene);
    break;
  }
}

int main(void) {
  srand((unsigned)time(NULL));
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(WINDOW_W, WINDOW_H, "Grapher");
  SetTargetFPS(60);

  gvizPlatformMenuInit();

  gvizScene scene;
  gvizBuildBlankScene(&scene);
  gvizLayerMainMenu *menu = installMainMenu(&scene);
  gvizOBJLoadModal *objModal = NULL;

  while (!WindowShouldClose()) {
    gvizSceneHandleInput(&scene);
    gvizSceneUpdate(&scene, GetFrameTime());
    gvizSceneDraw(&scene);

    /* Drain a freshly picked .obj path: present the layout-choice modal. */
    if (!objModal) {
      char *objPath = gvizPlatformMenuPollPendingOBJPath();
      if (objPath) {
        objModal = installOBJModal(&scene, objPath);
        gvizPlatformMenuFreePath(objPath);
      }
    }

    /* If the modal has resolved, dispatch to the right builder. */
    if (objModal && objModal->result != GVIZ_OBJ_MODAL_PENDING) {
      gvizOBJModalChoice choice = objModal->result;
      char path[1024];
      strncpy(path, objModal->objPath, sizeof(path));
      path[sizeof(path) - 1] = '\0';
      buildFromOBJChoice(&scene, choice, path);
      objModal = NULL;
      menu = NULL;
    }

    if (menu && menu->requestedAction != GVIZ_MENU_NONE) {
      gvizMainMenuAction act = menu->requestedAction;
      menu->requestedAction = GVIZ_MENU_NONE;
      gvizSceneRelease(&scene);
      switch (act) {
      case GVIZ_MENU_BLANK_SCENE:
        gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_DEMO_GRIP_SIERPINSKI:
        if (gvizBuildGRIPSierpinskiScene(&scene, 13) != 0)
          gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_DEMO_GRIP_CARPET:
        if (gvizBuildGRIPCarpetScene(&scene, 4) != 0)
          gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_DEMO_TUTTE:
        if (gvizBuildTutteDemoScene(&scene) != 0)
          gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_DEMO_POLY_TUTTE:
        if (gvizBuildPolyTutteDemoScene(&scene) != 0)
          gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_DEMO_TREE:
        if (gvizBuildTreeDemoScene(&scene) != 0)
          gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_LOAD_SCENE:
        gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_LOAD_OBJ_TUTTE:
        /* In-app entry now defers to the macOS File menu. */
        gvizBuildBlankScene(&scene);
        break;
      default:
        gvizBuildBlankScene(&scene);
        break;
      }
      menu = NULL;
    }
  }

  gvizSceneRelease(&scene);
  CloseWindow();
  return 0;
}
