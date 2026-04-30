#ifndef _GVIZ_LAYER_H_
#define _GVIZ_LAYER_H_

#include "core/object.h"
#include "raylib.h"
#include <stddef.h>

typedef struct gvizViewport {
  int x, y;
  int width, height;
} gvizViewport;

/* Layer flags. */
enum {
  GVIZ_LAYER_VISIBLE = 1 << 0,
  /*
   * Screen-space layers render outside BeginMode2D/3D — their draw callback
   * receives the camera for reference but draws in raw screen coordinates.
   */
  GVIZ_LAYER_SCREEN_SPACE = 1 << 1,
  /*
   * If set, the layer eats input events that fall inside its viewport even
   * if it does not consume them (useful for modal menus).
   */
  GVIZ_LAYER_CAPTURES_INPUT = 1 << 2,
};

/*
 * hitTest returns 1 if the world-space point (wx, wy) lies over the layer's
 * interactive content, 0 otherwise. May be NULL on the vtable — the scene
 * treats a NULL hitTest as "does not participate in hit-testing" (the layer
 * still receives events in z-order).
 */
typedef int(HitTestFunction)(void *self, float wx, float wy);

/* Component layers expose their per-layer camera through this accessor.
 * Screen-space (menu/HUD/modal) layers leave it NULL — they draw in raw
 * screen coordinates and receive NULL as the camera argument to draw(). */
struct gvizCamera;
typedef struct gvizCamera *(GetCameraFunction)(void *self);

/*
 * Optional: return the world-space centroid and approximate radius of the
 * layer's content. Used by the camera input helpers for T/R/L controls.
 * Layers that don't implement this get NULL — those controls become no-ops.
 * For 2D layers: fill centroid.x/.y, leave centroid.z = 0.
 */
typedef void (GetContentBoundsFunction)(void *self,
                                        Vector3 *centroid, float *radius);

typedef struct gvizLayerVTable {
  DrawFunction *draw;
  UpdateFunction *update;
  EventHandler *onEvent;
  ReleaseFunction *release;
  HitTestFunction *hitTest;
  GetCameraFunction *getCamera;
  GetContentBoundsFunction *getContentBounds;
} gvizLayerVTable;

typedef struct gvizLayer {
  const gvizLayerVTable *vtable;
  gvizViewport viewport;
  size_t z;
  unsigned int flags;
} gvizLayer;

void gvizLayerInit(gvizLayer *layer, const gvizViewport viewport,
                   const gvizLayerVTable *vtable, size_t z);

#endif
