#include "app/gvizSceneBuilders.h"
#include "core/alloc.h"
#include "core/gvizScene.h"
#include "raylib.h"
#include "renderer/layers/gvizLayerMainMenu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WINDOW_W 1280
#define WINDOW_H 800

/* #define GVIZ_OBJ_TEST_PATH "/Users/abdulazizalahmadi/Desktop/COMPSCI 163/Grapher/build/face.obj" */

static char *gvizOpenFileDialog(void) {
  const char *cmd =
      "osascript -e 'POSIX path of (choose file with prompt \"Select OBJ mesh\""
      " of type {\"obj\", \"OBJ\"})' 2>/dev/null";
  FILE *fp = popen(cmd, "r");
  if (!fp)
    return NULL;
  char buf[1024];
  if (!fgets(buf, sizeof(buf), fp)) {
    pclose(fp);
    return NULL;
  }
  pclose(fp);
  size_t len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r' ||
                     buf[len - 1] == ' ' || buf[len - 1] == '\t'))
    buf[--len] = '\0';
  if (len == 0)
    return NULL;
  char *out = (char *)GVIZ_ALLOC(len + 1);
  if (!out)
    return NULL;
  memcpy(out, buf, len + 1);
  return out;
}

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
        /* TODO: raygui text-input file picker. */
        gvizBuildBlankScene(&scene);
        break;
      case GVIZ_MENU_LOAD_OBJ_TUTTE: {
#ifdef GVIZ_OBJ_TEST_PATH
        if (gvizBuildPolyTutteFromOBJScene(&scene, GVIZ_OBJ_TEST_PATH) != 0)
          gvizBuildBlankScene(&scene);
#else
        char *path = gvizOpenFileDialog();
        if (!path || gvizBuildPolyTutteFromOBJScene(&scene, path) != 0)
          gvizBuildBlankScene(&scene);
        if (path)
          GVIZ_DEALLOC(path);
#endif
        break;
      }
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
