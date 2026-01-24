#include "../include/graph.h"
#include "../include/graphvis.h"
#include <math.h>
#include <raylib.h>
#include <raymath.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

[[nodiscard]] void *copyChar(const void *data) {
  char *out = malloc(sizeof(char));
  *out = *(char *)data;
  return out;
}

void freeChar(void *data) {
  if (data) {
    free((char *)data);
  }
}

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
  Graph *g = createGraph(13, 1, copyFn, freeFn);
  char string[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G',
                   'H', 'I', 'J', 'K', 'L', 'M'};
  for (int i = 0; i < 13; i++) {
    printf("Creating vertex %d\n", i);
    addVertex(g, copyChar(&string[i]));
  }
  printf("Adding Edges\n");
  int res = 0;

  res = addEdge(g, 0, 1);
  res = addEdge(g, 0, 2);
  res = addEdge(g, 0, 3);
  res = addEdge(g, 1, 4);
  res = addEdge(g, 2, 5);
  res = addEdge(g, 2, 6);
  res = addEdge(g, 2, 7);
  res = addEdge(g, 3, 8);
  res = addEdge(g, 3, 9);
  res = addEdge(g, 5, 10);
  res = addEdge(g, 5, 11);
  res = addEdge(g, 7, 12);
  if (res != 0) {
    printf("nonzero res %d\n", res);
  }

  Vector2 center = {WIDTH / 2.0, HEIGHT / 2.0};
  Vector2 currPos = {WIDTH / 2.0 - 100, HEIGHT / 2.0};

  float r = 25;

  vComponent2 vComponents[g->count];
  vComponents[0] = (vComponent2){g->vertices[0], (Vector2){WIDTH / 2.0, 0}, 25};
  RTVertexData vertexData[g->count];
  int thread[g->count];
  memset(thread, 0, sizeof(thread));
  RTTree rtTree = {g, vertexData, thread};

  printf("running reinglodtilford\n");
  runReingoldTilfordAlgorithm(&rtTree, 0, 1);

  for (int i = 0; i < g->count; i++) {
    if (thread[i]) {
      removeEdge(g, i, neighbors(g, i)->arr[0]);
    }
  }

  printf("running makeTree\n");
  makeTreeComponents(&rtTree, vComponents, 0, (Vector2){WIDTH / 2.0, 0});

  InitWindow(WIDTH, HEIGHT, "graphvis");
  SetTargetFPS(60);

  printf("running draw\n");

  Camera2D camera = {(Vector2){-WIDTH / 2.0, 100}, (Vector2){0, 0}, 0, 1.0f};
  BeginMode2D(camera);

  while (!WindowShouldClose()) {
    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      for (size_t i = 0; i < g->count; i++) {
        drawVertex(vComponents[i], camera);
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
