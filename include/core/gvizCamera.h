#ifndef _GVIZ_CORE_CAMERA_H_
#define _GVIZ_CORE_CAMERA_H_

#include "raylib.h"

/*
 * Tagged-union camera wrapper. Layers receive a const pointer to one of these
 * and inspect `kind` to decide how to interpret the underlying raylib camera.
 */
typedef enum gvizCameraKind {
  GVIZ_CAMERA_2D,
  GVIZ_CAMERA_3D,
} gvizCameraKind;

typedef struct gvizCamera {
  gvizCameraKind kind;
  union {
    Camera2D c2d;
    Camera3D c3d;
  };
} gvizCamera;

/* Factory helpers. */
gvizCamera gvizCameraMake2D(Vector2 target, Vector2 offset, float rotation,
                            float zoom);
gvizCamera gvizCameraMake3D(Vector3 position, Vector3 target, Vector3 up,
                            float fovy, int projection);

/*
 * Convert a screen point to the camera's world plane. For 2D this is the
 * raylib GetScreenToWorld2D. For 3D, a ray is cast into the scene and the
 * point on the z=0 plane is returned (z is returned in out_z if non-null).
 */
Vector2 gvizCameraScreenToWorld2D(const gvizCamera *cam, Vector2 screen);

#endif
