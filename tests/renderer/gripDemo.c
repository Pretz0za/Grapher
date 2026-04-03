#include "cblas.h"
#include "dsa/gvizGraph.h"
#include "raylib.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/layers/gvizLayerGraph.h"

typedef struct {
  gvizGraph *g;
  size_t t, l, r;

} SierpinskiTriangle;
void createSierpinski() {}

void scalePositions(size_t n, double *positions, size_t count) {
  for (double *curr = positions; curr < positions + count * n; curr += n) {
    cblas_dscal(n, 10, curr, 1);
  }
}

gvizGraph build_rect_mesh(size_t L, size_t W) {
  gvizGraph g;
  gvizGraphInitAtCapacity(&g, 0, L * W);

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++)
      gvizGraphAddVertex(&g, NULL, NULL, NULL);

  size_t idx, idx_right, idx_down;

  for (size_t i = 0; i < L; i++)
    for (size_t j = 0; j < W; j++) {
      idx = i * W + j;

      // right neighbor
      if (j + 1 < W) {
        idx_right = i * W + (j + 1);
        gvizGraphAddEdge(&g, idx, idx_right);
      }

      // down neighbor
      if (i + 1 < L) {
        idx_down = (i + 1) * W + j;
        gvizGraphAddEdge(&g, idx, idx_down);
      }
    }

  return g;
}

int main() {

  size_t WIDTH = 100, HEIGHT = 100;

  printf("initializing state\n");
  gvizGRIPState state;
  gvizGraph mesh = build_rect_mesh(WIDTH, HEIGHT);
  gvizGRIPEmbeddingInit(&state, &mesh, (WIDTH - 1) + (HEIGHT - 1), 2);

  printf("creating filtration\n");
  gvizGRIPEmbeddingEmbed(&state);

  scalePositions(2, state.graph.embedding.vertexPositions, mesh.vertices.count);

  size_t N = state.graph.graph->vertices.count;

  double minx = 1e30, maxx = -1e30;
  double miny = 1e30, maxy = -1e30;

  for (size_t i = 0; i < N; ++i) {
    double *p = gvizEmbeddedGraphGetVPosition(&state.graph, i);
    if (p[0] < minx)
      minx = p[0];
    if (p[0] > maxx)
      maxx = p[0];
    if (p[1] < miny)
      miny = p[1];
    if (p[1] > maxy)
      maxy = p[1];
  }

  printf("layout bbox: x=[%f,%f], y=[%f,%f]\n", minx, maxx, miny, maxy);

  InitWindow(1000, 1000, "graphvis");
  SetTargetFPS(60);
  Camera2D camera = {(Vector2){0, 0}, (Vector2){0, 0}, 0, 1.0f};

  gvizLayerGraph layer;
  gvizViewport viewport = {0, 0, 800, 600};
  gvizLayerGraphInit(&layer, (gvizEmbeddedGraph *)&state, viewport, 999);

  unsigned char currOpacity = 0xFF;

  printf("directed: %d\n", mesh.directed);

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

  gvizGRIPEmbeddingRelease(&state);
  gvizGraphRelease(&mesh);
  CloseWindow();
  return 0;

  gvizGraphRelease(&mesh);
  gvizGRIPEmbeddingRelease(&state);
}

//
// int main() {
//   int WIDTH = 800;
//   int HEIGHT = 600;
//   gvizGraph graph;
//   gvizGraphInit(&graph, 1);
//   gvizGraphAddVertex(&graph, NULL, NULL, NULL);
//   randomTree(&graph, 0, 0, 5, 0);
//
//   gvizEmbeddedTree rtTreeEmbedding;
//   gvizEmbeddedTreeRTInit(&rtTreeEmbedding, &graph, 0);
//
//   InitWindow(WIDTH, HEIGHT, "graphvis");
//   SetTargetFPS(60);
//   Camera2D camera = {(Vector2){0, 0}, (Vector2){0, 0}, 0, 1.0f};
//
//   gvizEmbeddedTreeCalculateOffsets(&rtTreeEmbedding, 0, 0);
//
//   gvizEmbeddedTreeEmbed(&rtTreeEmbedding, 0, (Vector2){0, 0});
//
//   gvizLayerGraph layer;
//   gvizViewport viewport = {0, 0, 800, 600};
//   gvizLayerGraphInit(&layer, (gvizEmbeddedGraph *)&rtTreeEmbedding,
//   viewport,
//                      999);
//
//   unsigned char currOpacity = 0xFF;
//
//   while (!WindowShouldClose()) {
//     BeginDrawing();
//
//     {
//       ClearBackground(RAYWHITE);
//
//       // Drag to move camera
//       if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
//         Vector2 delta = GetMouseDelta();
//         camera.target.x -= delta.x / camera.zoom;
//         camera.target.y -= delta.y / camera.zoom;
//       }
//
//       // Zoom with mouse wheel, centered on cursor
//       float wheelMove = GetMouseWheelMove();
//       if (wheelMove != 0) {
//         Vector2 mousePos = GetMousePosition();
//
//         // Get world position before zoom
//         Vector2 worldPosBefore = GetScreenToWorld2D(mousePos, camera);
//
//         // Apply zoom
//         float zoomFactor = 1.0f + (wheelMove * 0.1f);
//         camera.zoom *= zoomFactor;
//
//         // Clamp zoom
//         // if (camera.zoom > 3.0f)
//         //   camera.zoom = 3.0f;
//         // else if (camera.zoom < 0.1f)
//         //   camera.zoom = 0.1f;
//
//         // Get world position after zoom
//         Vector2 worldPosAfter = GetScreenToWorld2D(mousePos, camera);
//
//         // Adjust camera target to keep cursor on same world position
//         camera.target.x += (worldPosBefore.x - worldPosAfter.x);
//         camera.target.y += (worldPosBefore.y - worldPosAfter.y);
//       }
//
//       BeginMode2D(camera);
//
//       // draw
//
//       ((gvizLayer *)&layer)->vtable->draw(&layer, &camera);
//
//       EndMode2D();
//       DrawText("Hello World", 0, 0, 20, GREEN);
//       currOpacity = 0xFF;
//     }
//
//     EndDrawing();
//   }
//
//   gvizEmbeddedTreeRTRelease(&rtTreeEmbedding);
//   gvizGraphRelease(&graph);
//   CloseWindow();
//   return 0;
// }
