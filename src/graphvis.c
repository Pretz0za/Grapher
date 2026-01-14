#include "../include/graphvis.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>

void drawVertex(vComponent2 vComponent) {

  DrawRing(vComponent.pos, 22, 25, 0, 360, 1, BLACK);
  //  DrawCircleLines(vComponent.pos.x, vComponent.pos.y, vComponent.r, BLACK);
  // DrawText("V", vComponent.pos.x - 5, vComponent.pos.y - 5, 20, BLACK);
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
