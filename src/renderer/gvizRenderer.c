#include "renderer/gvizRenderer.h"
#include "raymath.h"

void gvizRenderer3DCameraUpdate(Camera3D *camera, Vector3 centroid) {
  Vector3 forward =
      Vector3Normalize(Vector3Subtract(camera->target, camera->position));
  Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera->up));

  float speed = 50.0f;
  float fb = (IsKeyDown(KEY_W) - IsKeyDown(KEY_S)) * speed;
  float lr = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A)) * speed;
  float ud = (IsKeyDown(KEY_E) - IsKeyDown(KEY_Q)) * speed;

  camera->position = Vector3Add(camera->position, Vector3Scale(forward, fb));
  camera->position = Vector3Add(camera->position, Vector3Scale(right, lr));
  camera->position = Vector3Add(camera->position, Vector3Scale(camera->up, ud));
  camera->target = Vector3Add(camera->target, Vector3Scale(forward, fb));
  camera->target = Vector3Add(camera->target, Vector3Scale(right, lr));
  camera->target = Vector3Add(camera->target, Vector3Scale(camera->up, ud));

  // yaw/pitch via UpdateCameraPro
  UpdateCameraPro(
      camera, (Vector3){0, 0, 0},
      (Vector3){
          IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? GetMouseDelta().x * 0.3f : 0,
          IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? GetMouseDelta().y * 0.3f : 0,
          0},
      0);

  // roll (neck twist) with Z/X
  {
    float rollSpeed = 1.5f; // radians per second
    float rollDelta = 0.0f;

    if (IsKeyDown(KEY_Z))
      rollDelta += rollSpeed * GetFrameTime();
    if (IsKeyDown(KEY_X))
      rollDelta -= rollSpeed * GetFrameTime();

    if (rollDelta != 0.0f) {
      Vector3 dir = Vector3Normalize(
          Vector3Subtract(camera->target, camera->position)); // forward
      camera->up = Vector3RotateByAxisAngle(camera->up, dir, rollDelta);
      camera->up = Vector3Normalize(camera->up);
    }
  }

  // scroll to change FOV (zoom feel without moving)
  float wheel = GetMouseWheelMove();
  if (wheel != 0) {
    camera->fovy -= wheel * 2.0f;
    if (camera->fovy < 5.0f)
      camera->fovy = 5.0f;
    if (camera->fovy > 120.0f)
      camera->fovy = 120.0f;
  }

  // rotate camera & target around centroid using *current up* axis
  if (IsKeyDown(KEY_R)) {
    float rotSpeed = 1.0f * GetFrameTime();
    Vector3 axis = Vector3Normalize(camera->up);

    Vector3 toCamera = Vector3Subtract(camera->position, centroid);
    toCamera = Vector3RotateByAxisAngle(toCamera, axis, rotSpeed);
    camera->position = Vector3Add(centroid, toCamera);

    Vector3 toTarget = Vector3Subtract(camera->target, centroid);
    toTarget = Vector3RotateByAxisAngle(toTarget, axis, rotSpeed);
    camera->target = Vector3Add(centroid, toTarget);
  }

  if (IsKeyPressed(KEY_T)) {
    camera->target = centroid;
    camera->position = (Vector3){
        centroid.x, centroid.y,
        centroid.z + 5000.0f // pull back along Z
    };
  }

  if (IsKeyPressed(KEY_L)) {
    camera->target = centroid;
  }
}

void gvizRenderer2DCameraUpdate(Camera2D *camera) {

  // Drag to move camera
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 delta = GetMouseDelta();
    camera->target.x -= delta.x / camera->zoom;
    camera->target.y -= delta.y / camera->zoom;
  }

  // Zoom with mouse wheel, centered on cursor
  float wheelMove = GetMouseWheelMove();
  if (wheelMove != 0) {
    Vector2 mousePos = GetMousePosition();

    // Get world position before zoom
    Vector2 worldPosBefore = GetScreenToWorld2D(mousePos, *camera);

    // Apply zoom
    float zoomFactor = 1.0f + (wheelMove * 0.1f);
    camera->zoom *= zoomFactor;

    // Get world position after zoom
    Vector2 worldPosAfter = GetScreenToWorld2D(mousePos, *camera);

    // Adjust camera target to keep cursor on same world position
    camera->target.x += (worldPosBefore.x - worldPosAfter.x);
    camera->target.y += (worldPosBefore.y - worldPosAfter.y);
  }
}
