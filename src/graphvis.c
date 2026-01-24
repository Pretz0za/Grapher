#include "../include/graphvis.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void drawVertex(vComponent2 vComponent, Camera2D camera) {
  DrawRing(GetWorldToScreen2D(vComponent.pos, camera), 22, 25, 0, 360, 1,
           BLACK);
  //  DrawCircleLines(vComponent.pos.x, vComponent.pos.y, vComponent.r, BLACK);
  // DrawText(str, vComponent.pos.x - 5, vComponent.pos.y - 5, 20, BLACK);
  return;
}

void drawEdge(vComponent2 from, vComponent2 to, int directed) {

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

  Vector2 v1 = Vector2Lerp(from.pos, to.pos, tEnd);
  Vector2 v2 = Vector2Add(v1, delta1);
  Vector2 v3 = Vector2Add(v1, delta2);

  DrawLineEx(Vector2Lerp(from.pos, to.pos, tBegin), v1, 2,
             BLACK); // Draw a line

  if (directed)
    DrawTriangle(v1, v3, v2, BLACK);
}

const int minsep = 100;

void runReingoldTilfordAlgorithm(RTTree *tree, size_t root, size_t level) {
  Vector *children = neighbors(tree->tree, root);
  int currsep;
  int rootsep;
  int lOffSum, rOffSum;
  int lStop, rStop;
  size_t lr, ll, rl, rr;
  size_t l, r; // l -> left subtree along right contour. r -> right subtree
               // along left contour.

  char str[32];
  snprintf(str, sizeof(str), "%c", *(char *)getVertexData(tree->tree, root));
  printf("solving vertex %s\n", str);

  if (children->count == 0) {
    printf("reingoldtilford: reached base case\n");
    tree->vertexData[root].rMost = root;
    tree->vertexData[root].lMost = root;
    tree->vertexData[root].depth = level;
    tree->vertexData[root].offsets = malloc(sizeof(float));
    memset(tree->vertexData[root].offsets, 0, sizeof(float));
    return;
  }

  for (int i = 0; i < children->count; i++) {
    printf("reingoldtilford: dividing on\n");
    printVec(children, stdout);
    runReingoldTilfordAlgorithm(tree, children->arr[i], level + 1);
  }

  int seperations[children->count - 1];
  float totalDist = 0;

  tree->vertexData[root].depth = level;
  // Initialize blob to be the left child
  tree->vertexData[root].lMost = tree->vertexData[children->arr[0]].lMost;
  tree->vertexData[root].rMost = tree->vertexData[children->arr[0]].rMost;

  if (children->count == 1) {
    tree->vertexData[root].offsets = malloc(sizeof(float));
    memset(tree->vertexData[root].offsets, 0, sizeof(float));
    return;
  }

  // Add children to the blob one at a time
  for (int i = 1; i < children->count; i++) {
    currsep = minsep;
    rootsep = currsep;
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
        currsep = minsep;
      }

      if (neighbors(tree->tree, r)->count == 0) {
        rStop = 1;
      } else {
        currsep += tree->vertexData[r].offsets[0];
        rOffSum += tree->vertexData[r].offsets[0];
        r = neighbors(tree->tree, r)->arr[0];
      }

      if (neighbors(tree->tree, l)->count == 0) {
        lStop = 1;
      } else {
        currsep -=
            tree->vertexData[l].offsets[neighbors(tree->tree, l)->count - 1];
        lOffSum +=
            tree->vertexData[l].offsets[neighbors(tree->tree, l)->count - 1];
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

    // Set lMost and rMost of root
    if (tree->vertexData[rl].depth > tree->vertexData[ll].depth) {
      tree->vertexData[root].lMost = rl;
      tree->vertexData[rl].offsets[0] += (float)totalDist / 2.0;
    } else {
      tree->vertexData[root].lMost = ll;
      tree->vertexData[ll].offsets[0] -= (float)rootsep / 2.0;
    }

    if (tree->vertexData[lr].depth > tree->vertexData[rr].depth) {
      tree->vertexData[root].rMost = lr;
      tree->vertexData[lr].offsets[0] -= (float)rootsep / 2.0;
    } else {
      tree->vertexData[root].rMost = rr;
      tree->vertexData[rr].offsets[0] += (float)totalDist / 2.0;
    }

    // Create threads in the contours
    if (!lStop && rStop) {
      addEdge(tree->tree, rr, l);
      tree->thread[rr] = 1;
      tree->vertexData[rr].offsets[0] =
          -(tree->vertexData[rr].offsets[0] + rootsep) + lOffSum;
      if (rl != rr) {
        free(tree->vertexData[rl].offsets);
        tree->vertexData[rl].offsets = NULL;
      }
    } else if (lStop && !rStop) {
      addEdge(tree->tree, ll, r);
      tree->thread[ll] = 1;
      tree->vertexData[ll].offsets[0] =
          -(tree->vertexData[ll].offsets[0]) + rootsep + rOffSum;
      if (lr != ll) {
        free(tree->vertexData[lr].offsets);
        tree->vertexData[lr].offsets = NULL;
      }
    } else {
      if (lr != ll) {
        free(tree->vertexData[lr].offsets);
        tree->vertexData[lr].offsets = NULL;
      }
      if (rl != rr) {
        free(tree->vertexData[rl].offsets);
        tree->vertexData[rl].offsets = NULL;
      }
    }
  }

  // Set offsets for roots children
  tree->vertexData[root].offsets = malloc(sizeof(float) * children->count);
  tree->vertexData[root].offsets[0] = -totalDist / 2.0;
  for (int i = 1; i < children->count; i++) {
    tree->vertexData[root].offsets[i] =
        tree->vertexData[root].offsets[i - 1] + seperations[i - 1];
  }

  // DONE!
}

void makeTreeComponents(RTTree *tree, vComponent2 *components, size_t root,
                        Vector2 rootPos) {
  Vector *children = neighbors(tree->tree, root);
  Vector2 pos;

  if (children->count == 0) {
    return;
  }

  for (int i = 0; i < children->count; i++) {
    pos = Vector2Add(rootPos,
                     (Vector2){tree->vertexData[root].offsets[i], 100.0});
    components[children->arr[i]].pos = pos;
    components[children->arr[i]].r = 25;
    components[children->arr[i]].v = tree->tree->vertices[children->arr[i]];

    makeTreeComponents(tree, components, children->arr[i], pos);
  }
}
