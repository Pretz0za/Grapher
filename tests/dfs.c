#include "../include/graph.h"
#include "../include/graphvis.h"
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>

[[nodiscard]] void *copyChar(const void *data) { return NULL; }

void freeChar(void *data) { return; }

const int HEIGHT = 600;
const int WIDTH = 800;

void iterateByNgonAngle(Vector2 *prev, const Vector2 center, int n) {
  prev->x -= center.x;
  prev->y -= center.y;
  double theta = 2 * PI / n;
  double c = cos(theta);
  double s = sin(theta);

  float tmp = prev->x;

  prev->x = prev->x * c - prev->y * s;
  prev->y = tmp * s + prev->y * c;
  prev->x += center.x;
  prev->y += center.y;

  return;
}

int main() {

  DataCopyFn copyFn = copyChar;
  DataFreeFn freeFn = freeChar;
  Graph *g = createGraph(5, 0, copyFn, freeFn);
  char *string[] = {"A", "B", "C", "D", "E"};
  for (int i = 0; i < 5; i++) {
    printf("Creating vertex %d\n", i);
    addVertex(g, NULL);
  }
  printf("Adding Edges\n");
  addEdge(g, 0, 1);
  addEdge(g, 0, 2);
  addEdge(g, 1, 3);
  addEdge(g, 1, 4);
  addEdge(g, 2, 3);

  vComponent2 vComponents[g->count];

  Vector2 center = {WIDTH / 2, HEIGHT / 2};
  Vector2 currPos = {WIDTH / 2 - 100, HEIGHT / 2};

  float r = 25;

  for (size_t i = 0; i < g->count; i++) {
    vComponents[i].pos = currPos;
    printf("Set circle at (%f, %f)\n", currPos.x, currPos.y);

    iterateByNgonAngle(&currPos, center, g->count);
    vComponents[i].v = g->vertices[i];
    vComponents[i].r = r;
  }

  InitWindow(WIDTH, HEIGHT, "graphvis");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      for (size_t i = 0; i < g->count; i++) {
        drawVertex(vComponents[i]);
        for (int j = 0; j < vComponents[i].v->neighbors->count; j++) {
          drawEdge(vComponents[i],
                   vComponents[vComponents[i].v->neighbors->arr[j]],
                   g->directed);
        }
      }

      DrawText("Hello World", 0, 0, 20, GREEN);
    }

    EndDrawing();
  }

  CloseWindow();
  return 0;
}

//
// int main() {
//   DataCopyFn copyFn = copyChar;
//   DataFreeFn freeFn = freeChar;
//   Graph *g = createGraph(5, 0, copyFn, freeFn);
//   char *string[] = {"A", "B", "C", "D", "E"};
//   for (int i = 0; i < 5; i++) {
//     printf("Creating vertex %d\n", i);
//     addVertex(g, NULL);
//   }
//   printf("Adding Edges\n");
//   addEdge(g, 0, 1);
//   addEdge(g, 0, 2);
//   addEdge(g, 1, 3);
//   addEdge(g, 1, 4);
//   addEdge(g, 2, 3);
//   printf("Running DFS\n");
//   Vector *expansionOrder = DepthFirstSearch(g, 0);
//   printDFSTree(g, expansionOrder, string, stdout);
//   printf("\n");
//   destroyGraph(g);
//   destroyVec(expansionOrder);
//
//
//
//
// }
