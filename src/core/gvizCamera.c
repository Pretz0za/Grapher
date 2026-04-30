#include "core/gvizCamera.h"
#include "raymath.h"

gvizCamera gvizCameraMake2D(Vector2 target, Vector2 offset, float rotation,
                            float zoom) {
  gvizCamera cam;
  cam.kind = GVIZ_CAMERA_2D;
  cam.c2d.target = target;
  cam.c2d.offset = offset;
  cam.c2d.rotation = rotation;
  cam.c2d.zoom = zoom;
  return cam;
}

gvizCamera gvizCameraMake3D(Vector3 position, Vector3 target, Vector3 up,
                            float fovy, int projection) {
  gvizCamera cam;
  cam.kind = GVIZ_CAMERA_3D;
  cam.c3d.position = position;
  cam.c3d.target = target;
  cam.c3d.up = up;
  cam.c3d.fovy = fovy;
  cam.c3d.projection = projection;
  return cam;
}

Vector2 gvizCameraScreenToWorld2D(const gvizCamera *cam, Vector2 screen) {
  if (cam->kind == GVIZ_CAMERA_2D) {
    return GetScreenToWorld2D(screen, cam->c2d);
  }
  Ray ray = GetScreenToWorldRay(screen, cam->c3d);
  float t = -ray.position.z / (ray.direction.z == 0.0f ? 1e-9f : ray.direction.z);
  Vector2 out = {ray.position.x + ray.direction.x * t,
                 ray.position.y + ray.direction.y * t};
  return out;
}

void gvizCameraHandleInput2D(gvizCamera *cam, int vx, int vy, int vw, int vh,
                             float mx, float my, float mdx, float mdy,
                             float wheel, int leftHeld, int interactive,
                             Vector2 centroid, float radius) {
  if (cam->kind != GVIZ_CAMERA_2D) return;
  cam->c2d.offset = (Vector2){vx + vw * 0.5f, vy + vh * 0.5f};
  if (!interactive) return;

  Camera2D *c = &cam->c2d;
  if (leftHeld) {
    c->target.x -= mdx / c->zoom;
    c->target.y -= mdy / c->zoom;
  }
  if (wheel != 0.0f) {
    Vector2 mp = {mx, my};
    Vector2 before = GetScreenToWorld2D(mp, *c);
    c->zoom *= (1.0f + wheel * 0.1f);
    Vector2 after = GetScreenToWorld2D(mp, *c);
    c->target.x += (before.x - after.x);
    c->target.y += (before.y - after.y);
  }
  /* T: pan to centroid */
  if (radius > 0.0f && IsKeyPressed(KEY_T)) {
    c->target = centroid;
  }
  /* R: rotate about centroid (set target so centroid stays centred, then spin) */
  if (radius > 0.0f && IsKeyDown(KEY_R)) {
    c->target   = centroid;
    c->rotation += 60.0f * GetFrameTime();
  }
}

void gvizCameraHandleInput3D(gvizCamera *cam, int vx, int vy, int vw, int vh,
                             int interactive, Vector3 centroid, float radius) {
  (void)vx; (void)vy; (void)vw; (void)vh;
  if (cam->kind != GVIZ_CAMERA_3D || !interactive) return;
  Camera3D *c = &cam->c3d;
  float dt = GetFrameTime();

  Vector3 forward = Vector3Normalize(Vector3Subtract(c->target, c->position));
  Vector3 right   = Vector3Normalize(Vector3CrossProduct(forward, c->up));

  /* WASD + Space translation */
  float speed = 200.0f * dt;
  if (IsKeyDown(KEY_W)) {
    c->position = Vector3Add(c->position, Vector3Scale(forward,  speed));
    c->target   = Vector3Add(c->target,   Vector3Scale(forward,  speed));
  }
  if (IsKeyDown(KEY_S)) {
    c->position = Vector3Add(c->position, Vector3Scale(forward, -speed));
    c->target   = Vector3Add(c->target,   Vector3Scale(forward, -speed));
  }
  if (IsKeyDown(KEY_A)) {
    c->position = Vector3Add(c->position, Vector3Scale(right, -speed));
    c->target   = Vector3Add(c->target,   Vector3Scale(right, -speed));
  }
  if (IsKeyDown(KEY_D)) {
    c->position = Vector3Add(c->position, Vector3Scale(right,  speed));
    c->target   = Vector3Add(c->target,   Vector3Scale(right,  speed));
  }
  if (IsKeyDown(KEY_SPACE)) {
    Vector3 up = Vector3Normalize(c->up);
    c->position = Vector3Add(c->position, Vector3Scale(up,  speed));
    c->target   = Vector3Add(c->target,   Vector3Scale(up,  speed));
  }

  /* Left-drag: yaw + pitch */
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    Vector2 md = GetMouseDelta();
    if (md.x != 0.0f || md.y != 0.0f) {
      forward = Vector3Normalize(Vector3Subtract(c->target, c->position));
      right   = Vector3Normalize(Vector3CrossProduct(forward, c->up));
      forward = Vector3Normalize(Vector3Transform(forward, MatrixRotate(c->up,    -md.x * 0.003f)));
      forward = Vector3Normalize(Vector3Transform(forward, MatrixRotate(right,    -md.y * 0.003f)));
      float dist = Vector3Distance(c->position, c->target);
      c->target  = Vector3Add(c->position, Vector3Scale(forward, dist));
    }
  }

  /* Z / X: roll */
  if (IsKeyDown(KEY_Z) || IsKeyDown(KEY_X)) {
    float angle = (IsKeyDown(KEY_Z) ? -1.5f : 1.5f) * dt;
    forward = Vector3Normalize(Vector3Subtract(c->target, c->position));
    c->up   = Vector3Normalize(Vector3Transform(c->up, MatrixRotate(forward, angle)));
  }

  /* Scroll: vertical = pitch, horizontal = yaw. Cmd+scroll-Y = FOV. */
  Vector2 sv = GetMouseWheelMoveV();
  if (sv.x != 0.0f || sv.y != 0.0f) {
    int cmd = IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
    if (cmd) {
      c->fovy -= sv.y * 2.0f;
      if (c->fovy <  10.0f) c->fovy =  10.0f;
      if (c->fovy > 120.0f) c->fovy = 120.0f;
    } else {
      forward = Vector3Normalize(Vector3Subtract(c->target, c->position));
      right   = Vector3Normalize(Vector3CrossProduct(forward, c->up));
      float sens = 0.04f;
      forward = Vector3Normalize(Vector3Transform(forward, MatrixRotate(right,  -sv.y * sens)));
      forward = Vector3Normalize(Vector3Transform(forward, MatrixRotate(c->up,  -sv.x * sens)));
      float dist = Vector3Distance(c->position, c->target);
      c->target  = Vector3Add(c->position, Vector3Scale(forward, dist));
    }
  }

  if (radius <= 0.0f) return;

  /* T: teleport to a view-distance proportional to content size, aim at centroid */
  if (IsKeyPressed(KEY_T)) {
    Vector3 dir = Vector3Subtract(c->position, centroid);
    float   len = Vector3Length(dir);
    float   L   = radius * 2.5f;
    if (L < 1.0f) L = 1.0f;
    Vector3 unit = (len > 1e-6f)
                   ? Vector3Scale(dir, 1.0f / len)
                   : (Vector3){1.0f, 0.0f, 0.0f};
    c->position = Vector3Add(centroid, Vector3Scale(unit, L));
    c->target   = centroid;
  }

  /* L: look at centroid without moving */
  if (IsKeyPressed(KEY_L)) {
    c->target = centroid;
  }
}
