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

/*
 * Reusable input helpers — handle pan/zoom/rotation for a camera bound to a
 * specific viewport rect. Caller supplies current mouse position (screen px),
 * mouse delta, wheel delta, and whether left-button is held. The viewport
 * (vx,vy,vw,vh) lets the helper set up a sensible 2D offset and reject input
 * that originated outside the layer.
 *
 * `interactive` should be 0 when the layer is not under cursor — the helper
 * still updates aspect-ratio-bound state (for 2D, the offset on resize) but
 * skips pan/zoom.
 */
void gvizCameraHandleInput2D(gvizCamera *cam, int vx, int vy, int vw, int vh,
                             float mx, float my, float mdx, float mdy,
                             float wheel, int leftHeld, int interactive);
/*
 * 3D camera helper. Orbit on left-drag without Cmd; pan on left-drag with
 * Cmd held. Right-button is unused (reserved for the scene context menu).
 */
void gvizCameraHandleInput3D(gvizCamera *cam, int vx, int vy, int vw, int vh,
                             float mdx, float mdy, float wheel,
                             int leftHeld, int superHeld, int interactive);

#endif
