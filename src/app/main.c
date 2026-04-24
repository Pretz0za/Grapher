#include "app/gvizSceneBuilders.h"
#include "core/alloc.h"
#include "core/gvizScene.h"
#include "raylib.h"
#include "renderer/layers/gvizLayerMainMenu.h"
#include <stdlib.h>

#define WINDOW_W 1280
#define WINDOW_H 800

/*
 * Build a scene that contains only the main menu layer (screen-space).
 * The menu's `requestedAction` is checked each frame by the main loop.
 */
static gvizLayerMainMenu *installMainMenu(gvizScene *scene) {
  gvizLayerMainMenu *menu = GVIZ_ALLOC(sizeof(gvizLayerMainMenu));
  if (!menu)
    return NULL;
  gvizViewport vp = {0, 0, GetScreenWidth(), GetScreenHeight()};
  gvizLayerMainMenuInit(menu, vp, 1000);
  gvizSceneAddLayer(scene, (gvizLayer *)menu);
  return menu;
}

int main(void) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(WINDOW_W, WINDOW_H, "Grapher");
  SetTargetFPS(60);

  gvizScene scene;
  gvizBuildBlankScene(&scene);
  gvizLayerMainMenu *menu = installMainMenu(&scene);

  while (!WindowShouldClose()) {
    gvizSceneHandleInput(&scene);
    gvizSceneUpdate(&scene, GetFrameTime());
    gvizSceneDraw(&scene);

    if (menu && menu->requestedAction != GVIZ_MENU_NONE) {
      gvizMainMenuAction act = menu->requestedAction;
      menu->requestedAction = GVIZ_MENU_NONE;
      gvizSceneRelease(&scene);
      switch (act) {
      case GVIZ_MENU_BLANK_SCENE:
        gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_DEMO_GRIP_SIERPINSKI:
        if (gvizBuildGRIPSierpinskiScene(&scene, 8) != 0)
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
      case GVIZ_MENU_DEMO_TREE:
        if (gvizBuildTreeDemoScene(&scene) != 0)
          gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_LOAD_SCENE:
        /* TODO: raygui text-input file picker. */
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
