#include "../../include/graph.h"
#include "../../include/graphvis.h"
#include "./include/helpers.h"
#include "./include/parser.h"
#include <math.h>
#include <raylib.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *copyNote(const void *data) {
  Note *src = (Note *)data;
  Note *dest = malloc(sizeof(Note));
  dest->references = createVec();
  copyVec(dest->references, src->references);
  dest->line = src->line;

  return dest;
}

void freeNote(void *data) {
  Note *tmp = (Note *)data;
  destroyVec(tmp->references);
  free(tmp);
}

DataCopyFn copyFn = copyNote;
DataFreeFn freeFn = freeNote;

Graph *makeNotesGraph(Note **notes, size_t size, size_t *filter,
                      size_t filterSize) {

  Graph *g = createGraph(filterSize ? filterSize : size, 1, copyFn, freeFn);

  for (int i = 0; i < (filterSize ? filterSize : size); i++) {
    addVertex(g, copyFn(notes[i]));
  }

  size_t refIdx;

  for (int i = 0; i < (filterSize ? filterSize : size); i++) {
    for (int j = 0; j < notes[i]->references->count; j++) {
      if (filterSize == 0 || (refIdx = findInArr(notes[i]->references->arr[j],
                                                 filter, filterSize)) != -1) {
        if (filterSize) {
          addEdge(g, i, refIdx);
          printf("Added edge from %d to %zu\n", i, refIdx);
        } else {
          addEdge(g, i, notes[i]->references->arr[j]);
          printf("Added edge from %d to %zu\n", i,
                 notes[i]->references->arr[j]);
        }
      }
    }
  }

  return g;
}

void destroyNotes(Note **notes, size_t count) {
  for (size_t i = 0; i < count; i++) {
    destroyVec(notes[i]->references);
    free(notes[i]);
  }
  free(notes);
}

[[nodiscard]] size_t *makeFilter(char *notes[], size_t count) {
  size_t *output = malloc(sizeof(size_t) * count);
  for (size_t i = 0; i < count; i++) {
    output[i] = strToLine(notes[i], strlen(notes[i]));
  }
  return output;
}

const int HEIGHT = 800;
const int WIDTH = 1480;

int main(int argc, char *argv[]) {
  if (argc < 2)
    exit(1);
  char *subset[] = {"m",  "r",  "s",  "t",  "ab", "ad", "ai", "al",
                    "am", "ar", "bz", "ca", "cf", "ci", "cj", "ck",
                    "cl", "cm", "ct", "cu", "cv", "cy", "cz", "dd"};
  size_t subsetSize = 24;
  size_t *filter = makeFilter(subset, subsetSize);

  printf("parsing file\n");
  size_t lineCount = strToLine("fs", 2);
  Note **notes = parseFile(argv[1], lineCount, NULL, 0);

  printf("Creating graph\n");
  Graph *g = makeNotesGraph(notes, lineCount, NULL, 0);
  for (int i = 0; i < g->count; i++) {
    // strings[i] = lineToStr(((Note *)getVertexData(DFSTree, i))->line);
    printf("vertex idx: %d, , children: \n", i);
    printVec(neighbors(g, i), stdout);
  }

  Graph *reverse = createGraph(g->size, 1, copyFn, freeFn);
  copyReversedGraph(reverse, g);

  char *strings[subsetSize];

  printf("running Depth First Search\n");
  Graph *DFSTree = DepthFirstSearch(g, 43);

  // for (int i = 0; i < DFSTree->count; i++) {
  //   strings[i] = lineToStr(((Note *)getVertexData(DFSTree, i))->line);
  //   // printf("vertex idx: %d, str: %s, children: \n", i, strings[i]);
  //   // printVec(neighbors(DFSTree, i), stdout);
  // }
  //
  printf("\n\n DFS of graph completed. Reached %zu/%zu vertices\n",
         DFSTree->count, g->count);
  //  printDFSTree(DFSTree, strings, stdout);

  printf("\n");

  Graph *rDFSTree = DepthFirstSearch(reverse, 43);
  for (int i = 0; i < g->count; i++) {
    // strings[i] = lineToStr(((Note *)getVertexData(DFSTree, i))->line);
    printf("vertex idx: %d, , children: \n", i);
    printVec(neighbors(g, i), stdout);
  }

  printf("\n\n DFS of reverse graph completed. Reached %zu/%zu vertices\n",
         DFSTree->count, reverse->count);
  // printDFSTree(rDFSTree, strings, stdout);

  Vector2 center = {WIDTH / 2.0, HEIGHT / 2.0};
  Vector2 currPos = {WIDTH / 2.0 - 100, HEIGHT / 2.0};

  float r = 25;

  vComponent2 vComponents[DFSTree->count];
  vComponents[0] = (vComponent2){DFSTree->vertices[0], (Vector2){0, 0}, 25};
  RTVertexData vertexData[DFSTree->count];
  int thread[DFSTree->count];
  memset(thread, 0, sizeof(thread));
  RTTree rtTree = {DFSTree, vertexData, thread};

  printf("running reinglodtilford\n");
  runReingoldTilfordAlgorithm(&rtTree, 0, 1);

  for (int i = 0; i < DFSTree->count; i++) {
    if (thread[i]) {
      printf("removed thread from %d\n", i);
      removeEdge(DFSTree, i, neighbors(DFSTree, i)->arr[0]);
    }
  }

  printf("running makeTree\n");
  makeTreeComponents(&rtTree, vComponents, 0, (Vector2){0, 0});

  InitWindow(WIDTH, HEIGHT, "graphvis");
  SetTargetFPS(60);

  Camera2D camera = {(Vector2){WIDTH / 2.0, 100}, (Vector2){0, 0}, 0, 1.0f};

  printf("running draw\n");
  while (!WindowShouldClose()) {
    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      if (IsKeyDown(KEY_RIGHT))
        camera.offset.x -= 4;
      else if (IsKeyDown(KEY_LEFT))
        camera.offset.x += 4;

      if (IsKeyDown(KEY_UP))
        camera.offset.y += 4;
      else if (IsKeyDown(KEY_DOWN))
        camera.offset.y -= 4;

      camera.zoom =
          expf(logf(camera.zoom) + ((float)GetMouseWheelMove() * 0.1f));

      if (camera.zoom > 3.0f)
        camera.zoom = 3.0f;
      else if (camera.zoom < 0.1f)
        camera.zoom = 0.1f;

      BeginMode2D(camera);
      for (size_t i = 0; i < DFSTree->count; i++) {
        drawVertex(vComponents[i], camera);
        for (int j = 0; j < vComponents[i].v->neighbors->count; j++) {
          drawEdge(vComponents[i],
                   vComponents[vComponents[i].v->neighbors->arr[j]],
                   DFSTree->directed);
        }
      }
      EndMode2D();

      DrawText("Hello World", 0, 0, 20, GREEN);
    }

    EndDrawing();
  }

  printf("destroying...\n");
  CloseWindow();
  destroyGraph(g);
  destroyGraph(reverse);
  destroyGraph(DFSTree);
  destroyGraph(rDFSTree);
  destroyNotes(notes, subsetSize ? subsetSize : lineCount);
}
