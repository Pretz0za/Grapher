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
                             float wheel, int leftHeld, int interactive) {
  if (cam->kind != GVIZ_CAMERA_2D) return;
  /* Recenter offset to viewport centre so (0,0) world maps to vp middle. */
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
}

void gvizCameraHandleInput3D(gvizCamera *cam, int vx, int vy, int vw, int vh,
                             float mdx, float mdy, float wheel,
                             int leftHeld, int rightHeld, int interactive) {
  (void)vx; (void)vy; (void)vw; (void)vh;
  if (cam->kind != GVIZ_CAMERA_3D || !interactive) return;
  Camera3D *c = &cam->c3d;
  /* Orbit on right-drag */
  if (rightHeld && (mdx != 0.0f || mdy != 0.0f)) {
    Vector3 dir = Vector3Subtract(c->position, c->target);
    float yaw = -mdx * 0.005f, pitch = -mdy * 0.005f;
    Matrix mYaw = MatrixRotate(c->up, yaw);
    dir = Vector3Transform(dir, mYaw);
    Vector3 right = Vector3CrossProduct(c->up, dir);
    right = Vector3Normalize(right);
    Matrix mPitch = MatrixRotate(right, pitch);
    dir = Vector3Transform(dir, mPitch);
    c->position = Vector3Add(c->target, dir);
  }
  /* Pan on left-drag */
  if (leftHeld && (mdx != 0.0f || mdy != 0.0f)) {
    Vector3 dir = Vector3Subtract(c->target, c->position);
    Vector3 right = Vector3CrossProduct(dir, c->up);
    right = Vector3Normalize(right);
    Vector3 up = Vector3Normalize(c->up);
    float dist = Vector3Length(dir) * 0.001f;
    Vector3 pan = Vector3Add(Vector3Scale(right, -mdx * dist),
                             Vector3Scale(up,    mdy * dist));
    c->target   = Vector3Add(c->target,   pan);
    c->position = Vector3Add(c->position, pan);
  }
  /* Dolly on wheel */
  if (wheel != 0.0f) {
    Vector3 dir = Vector3Subtract(c->position, c->target);
    float scale = 1.0f - wheel * 0.1f;
    if (scale < 0.1f) scale = 0.1f;
    dir = Vector3Scale(dir, scale);
    c->position = Vector3Add(c->target, dir);
  }
}
