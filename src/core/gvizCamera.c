#include "core/gvizCamera.h"

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
