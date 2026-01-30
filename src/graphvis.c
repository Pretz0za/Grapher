#include "../include/graphvis.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void drawVertex(vComponent2 vComponent, unsigned char opacity) {
  DrawRing(vComponent.pos, 22, 25, 0, 360, 1,
           (Color){0x00, 0x00, 0x00, opacity});
  return;
}

void drawEdge(vComponent2 from, vComponent2 to, int directed,
              unsigned char opacity) {

  Vector2 tmp = Vector2Subtract(to.pos, from.pos);

  Vector2 delta1 = {-7.5, 5};
  Vector2 delta2 = {-7.5, -5};
  float theta = atan2(tmp.y, tmp.x);
  delta1 = Vector2Rotate(delta1, theta);
  delta2 = Vector2Rotate(delta2, theta);
  float dist = Vector2Length(tmp);
  float tBegin = from.r / dist;
  float tEnd = 1 - to.r / dist;
  float tArrow = tEnd - 10.0 / dist;

  Vector2 v0 = Vector2Lerp(from.pos, to.pos, tEnd - 7.5 / dist);
  Vector2 v1 = Vector2Lerp(from.pos, to.pos, tEnd);

  DrawLineEx(Vector2Lerp(from.pos, to.pos, tBegin), directed ? v0 : v1, 2,
             (Color){0x00, 0x00, 0x00, opacity}); // Draw a line

  if (directed)
    DrawTriangle(v1, Vector2Add(v1, delta2), Vector2Add(v1, delta1),
                 (Color){0x00, 0x00, 0x00, opacity}); // Draw Arrow
}

void runReingoldTilfordAlgorithm(RTTree *tree, size_t root, size_t level) {
  // TODO: keep track of the minimum seperation along the contours of each pair.
  // This will allow to center siblings between eachother
  Vector *children = neighbors(tree->tree, root);
  const int minsep = 100;
  int noThreadSep;
  int currsep;
  int rootsep;
  int lOffSum, rOffSum;
  int lStop, rStop;
  int threadUsed = 0;
  size_t lr, ll, rl, rr;
  size_t l, r; // l -> left subtree along right contour. r -> right subtree
               // along left contour.

  if (children->count == 0) {
    tree->vertexData[root].rMost = root;
    tree->vertexData[root].lMost = root;
    tree->vertexData[root].depth = level;
    tree->offsets[root] = malloc(sizeof(float));
    if (level > tree->height)
      tree->height = level;
    memset(tree->offsets[root], 0, sizeof(float));
    return;
  }

  for (int i = 0; i < children->count; i++) {
    runReingoldTilfordAlgorithm(tree, children->arr[i], level + 1);
  }

  int seperations[children->count - 1];
  float totalDist = 0;

  tree->vertexData[root].depth = level;
  // Initialize blob to be the left child
  tree->vertexData[root].lMost = tree->vertexData[children->arr[0]].lMost;
  tree->vertexData[root].rMost = tree->vertexData[children->arr[0]].rMost;

  if (children->count == 1) {
    tree->offsets[root] = malloc(sizeof(float));
    memset(tree->offsets[root], 0, sizeof(float));
    return;
  }

  // Add children to the blob one at a time
  for (int i = 1; i < children->count; i++) {
    currsep = minsep;
    noThreadSep = currsep;
    rootsep = currsep;
    threadUsed = 0;
    lOffSum = 0;
    rOffSum = 0;
    l = children->arr[i - 1]; // right-most child in the blob
    r = children->arr[i];     // child to be added to blob

    lStop = 0;
    rStop = 0;
    // Calculate seperation between blob and child by comparing contours.
    while (!lStop && !rStop) {
      if (currsep < minsep) {
        rootsep += (minsep - currsep);
        if (threadUsed) {
          noThreadSep += (minsep - currsep);
        }
        currsep = minsep;
      }
      if (neighbors(tree->tree, r)->count == 0) {
        rStop = 1;
      } else {
        currsep += tree->offsets[r][0];
        rOffSum += tree->offsets[r][0];
        r = neighbors(tree->tree, r)->arr[0];
      }

      if (neighbors(tree->tree, l)->count == 0) {
        lStop = 1;
      } else {
        currsep -= tree->offsets[l][neighbors(tree->tree, l)->count - 1];
        lOffSum += tree->offsets[l][neighbors(tree->tree, l)->count - 1];
        if (l == tree->vertexData[children->arr[i - 1]].rMost) {
          threadUsed = 1;
        }
        l = neighbors(tree->tree, l)->arr[neighbors(tree->tree, l)->count - 1];
      }
    }

    lr = tree->vertexData[root].rMost;
    ll = tree->vertexData[root].lMost;
    rr = tree->vertexData[r].rMost;
    rl = tree->vertexData[r].lMost;

    // Store calculated seperation
    seperations[i - 1] = rootsep;
    totalDist += rootsep;

    // Centers the rightmost subtree of the blob, if able to move towards the
    // right.
    if (noThreadSep > minsep) {
      seperations[i - 2] += noThreadSep / 2;
      seperations[i - 1] -= noThreadSep / 2;
      rootsep -= noThreadSep / 2;
    }

    // Set lMost and rMost of root
    if (tree->vertexData[rl].depth > tree->vertexData[ll].depth) {
      tree->vertexData[root].lMost = rl;
      tree->offsets[rl][0] += (float)totalDist / 2.0;
    } else {
      tree->vertexData[root].lMost = ll;
      tree->offsets[ll][0] -= (float)rootsep / 2.0;
    }

    if (tree->vertexData[lr].depth > tree->vertexData[rr].depth) {
      tree->vertexData[root].rMost = lr;
      tree->offsets[lr][0] -= (float)rootsep / 2.0;
    } else {
      tree->vertexData[root].rMost = rr;
      tree->offsets[rr][0] += (float)totalDist / 2.0;
    }

    // Create threads in the contours
    if (!lStop && rStop) {
      addEdge(tree->tree, rr, l);
      tree->thread[rr] = 1;
      tree->offsets[rr][0] = -(tree->offsets[rr][0] + rootsep) + lOffSum;
    } else if (lStop && !rStop) {
      addEdge(tree->tree, ll, r);
      tree->thread[ll] = 1;
      tree->offsets[ll][0] = -(tree->offsets[ll][0]) + rootsep + rOffSum;
    }
  }

  // Set offsets for roots children
  tree->offsets[root] = malloc(sizeof(float) * children->count);
  tree->offsets[root][0] = -totalDist / 2.0;
  for (int i = 1; i < children->count; i++) {
    tree->offsets[root][i] = tree->offsets[root][i - 1] + seperations[i - 1];
  }

  // DONE!
}

Vector2 makeTreeComponents(RTTree *tree, vComponent2 *components, size_t root,
                           Vector2 rootPos) {
  Vector *children = neighbors(tree->tree, root);
  Vector2 pos;
  Vector2 currX;
  Vector2 extremeX = {INFINITY, -INFINITY};

  components[root].pos = rootPos;
  components[root].r = 25.0f;

  if (children->count == 0) {
    return (Vector2){rootPos.x, rootPos.x};
  }

  for (int i = 0; i < children->count; i++) {
    pos = Vector2Add(rootPos, (Vector2){tree->offsets[root][i], 100.0});
    components[children->arr[i]].pos = pos;
    components[children->arr[i]].r = 25.0f;

    currX = makeTreeComponents(tree, components, children->arr[i], pos);
    if (currX.x < extremeX.x) {
      extremeX.x = currX.x;
    }
    if (currX.y > extremeX.y) {
      extremeX.y = currX.y;
    }
  }
  return extremeX;
}

void openStateInWindow(VisState state) {
  int WIDTH = 800;
  int HEIGHT = 600;
  InitWindow(WIDTH, HEIGHT, "graphvis");
  SetTargetFPS(60);

  Camera2D camera = {(Vector2){0, 0}, (Vector2){0, 0}, 0, 1.0f};
  vComponent2 curr;
  Graph *currGraph;
  unsigned char currOpacity = 0xFF;
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
      if (state.stack) {
        for (int k = state.stack->count; k > 0; k--) {
          currGraph = state.stack->end - (state.stack->count - k);
          for (size_t i = 0; i < currGraph->count; i++) {
            curr = state.gamma.components[currGraph->map[i]];
            drawVertex(curr, currOpacity);
            for (int j = 0; j < neighbors(currGraph, i)->count; j++) {
              drawEdge(curr,
                       state.gamma.components
                           [currGraph->map[neighbors(currGraph, i)->arr[j]]],
                       currGraph->directed, currOpacity);
            }
          }
          currOpacity /= 8;
        }
      }

      for (size_t i = 0; i < state.g->count; i++) {
        curr = state.gamma.components[i];
        drawVertex(curr, currOpacity);
        for (int j = 0; j < neighbors(state.g, i)->count; j++) {
          drawEdge(curr, state.gamma.components[neighbors(state.g, i)->arr[j]],
                   state.g->directed, currOpacity);
        }
      }
      EndMode2D();
      DrawText("Hello World", 0, 0, 20, GREEN);
      currOpacity = 0xFF;
    }

    EndDrawing();
  }

  CloseWindow();
}

void cleanUpReingoldTilford(RTTree *rtTree, size_t root) {
  if (rtTree->offsets[root])
    free(rtTree->offsets[root]);
  if (neighbors(rtTree->tree, root)->count > 0) {
    for (int i = 0; i < neighbors(rtTree->tree, root)->count; i++) {
      cleanUpReingoldTilford(rtTree, neighbors(rtTree->tree, root)->arr[i]);
    }
  }
}

Embedding generateTreeEmbedding(Graph *tree) {

  // TODO: If not tree return NULL

  int minsep = 100;
  RTVertexData vertexData[tree->count];
  int thread[tree->count];
  float *offsets[tree->count];
  memset(thread, 0, sizeof(thread));
  memset(offsets, 0, sizeof(offsets));
  RTTree rtTree = {tree, vertexData, thread, offsets, 0};

  runReingoldTilfordAlgorithm(&rtTree, 0, 1);
  vComponent2 *components = malloc(tree->count * sizeof(vComponent2));

  // Clean up threads
  for (int i = 0; i < tree->count; i++) {
    if (thread[i])
      removeEdge(tree, i, neighbors(tree, i)->arr[0]);
  }

  // Calculate final positions
  Vector2 extremes =
      makeTreeComponents(&rtTree, components, 0, (Vector2){0, 0});

  cleanUpReingoldTilford(&rtTree, 0);
  return (Embedding){components, extremes.y - extremes.x,
                     rtTree.height * minsep};
}
