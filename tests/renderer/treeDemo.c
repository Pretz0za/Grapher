#include "dsa/gvizGraph.h"
#include "raylib.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizEmbeddedTree.h"
#include "renderer/layers/gvizLayerGraph.h"
#include <_stdlib.h>

#define MAXDEPTH 5

void randomTree(gvizGraph *tree, size_t root, int depth, int k, int exact) {
  if (depth >= MAXDEPTH) {
    return;
  }
  if (exact) {
    for (int i = 0; i < k; i++) {
      gvizGraphAddVertex(tree, NULL, NULL, NULL);
      gvizGraphAddEdge(tree, root, tree->vertices.count - 1);
      randomTree(tree, tree->vertices.count - 1, depth + 1, k, exact);
    }
  } else {
    int random = arc4random_uniform(k + 1);
    while (random < 2) {
      random = arc4random_uniform(k + 1);
    }
    for (int i = 0; i < random; i++) {
      gvizGraphAddVertex(tree, NULL, NULL, NULL);
      gvizGraphAddEdge(tree, root, tree->vertices.count - 1);
      randomTree(tree, tree->vertices.count - 1, depth + 1, k, exact);
    }
  }
  return;
}

int main() {
  int WIDTH = 800;
  int HEIGHT = 600;
  gvizGraph graph;
  gvizGraphInit(&graph, 1);
  gvizGraphAddVertex(&graph, NULL, NULL, NULL);
  randomTree(&graph, 0, 0, 5, 0);

  gvizEmbeddedTree rtTreeEmbedding;
  gvizEmbeddedTreeRTInit(&rtTreeEmbedding, &graph, 0);
  gvizEmbeddedTreeCalculateOffsets(&rtTreeEmbedding, 0, 0);

  double init[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(&rtTreeEmbedding, 0, init);

  InitWindow(WIDTH, HEIGHT, "graphvis");
  SetTargetFPS(60);
  Camera2D camera = {(Vector2){0, 0}, (Vector2){0, 0}, 0, 1.0f};

  gvizLayerGraph layer;
  gvizViewport viewport = {0, 0, 800, 600};
  gvizLayerGraphInit(&layer, (gvizEmbeddedGraph *)&rtTreeEmbedding, viewport,
                     999);

  unsigned char currOpacity = 0xFF;

  while (!WindowShouldClose()) {
    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      // Drag to move camera
      if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        Vector2 delta = GetMouseDelta();
        camera.target.x -= delta.x / camera.zoom;
        camera.target.y -= delta.y / camera.zoom;
      }

      // Zoom with mouse wheel, centered on cursor
      float wheelMove = GetMouseWheelMove();
      if (wheelMove != 0) {
        Vector2 mousePos = GetMousePosition();

        // Get world position before zoom
        Vector2 worldPosBefore = GetScreenToWorld2D(mousePos, camera);

        // Apply zoom
        float zoomFactor = 1.0f + (wheelMove * 0.1f);
        camera.zoom *= zoomFactor;

        // Clamp zoom
        // if (camera.zoom > 3.0f)
        //   camera.zoom = 3.0f;
        // else if (camera.zoom < 0.1f)
        //   camera.zoom = 0.1f;

        // Get world position after zoom
        Vector2 worldPosAfter = GetScreenToWorld2D(mousePos, camera);

        // Adjust camera target to keep cursor on same world position
        camera.target.x += (worldPosBefore.x - worldPosAfter.x);
        camera.target.y += (worldPosBefore.y - worldPosAfter.y);
      }

      BeginMode2D(camera);

      // draw

      ((gvizLayer *)&layer)->vtable->draw(&layer, &camera);

      EndMode2D();
      DrawText("Hello World", 0, 0, 20, GREEN);
      currOpacity = 0xFF;
    }

    EndDrawing();
  }

  gvizEmbeddedTreeRTRelease(&rtTreeEmbedding);
  gvizGraphRelease(&graph);
  CloseWindow();
  return 0;
}
