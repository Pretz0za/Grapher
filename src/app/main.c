#include "core/gvizScene.h"
#include "raylib.h"

#define WINDOW_W 1280
#define WINDOW_H 800

int main(void) {
  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
  InitWindow(WINDOW_W, WINDOW_H, "Grapher");
  SetTargetFPS(60);

  gvizScene scene;
  gvizSceneInit2D(&scene);

  while (!WindowShouldClose()) {
    gvizSceneHandleInput(&scene);
    gvizSceneUpdate(&scene, GetFrameTime());
    gvizSceneDraw(&scene);
  }

  gvizSceneRelease(&scene);
  CloseWindow();
  return 0;
}
