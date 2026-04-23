#ifndef _GVIZ_CORE_SCENE_H_
#define _GVIZ_CORE_SCENE_H_

#include "core/event.h"
#include "core/gvizCamera.h"
#include "dsa/gvizArray.h"
#include "renderer/layers/gvizLayer.h"

typedef enum gvizSceneMode {
  GVIZ_SCENE_2D,
  GVIZ_SCENE_3D,
} gvizSceneMode;

/*
 * A gvizScene owns a list of layers (pointers, z-ordered), a camera, and a
 * pending-destroy list for layers scheduled to be removed after the current
 * frame. Input is polled from raylib, translated into gvizEvents, and
 * dispatched top-down by z (highest z first).
 */
typedef struct gvizScene {
  gvizSceneMode mode;
  gvizCamera camera;

  /* gvizArray of `gvizLayer *` pointers, kept sorted by z ascending. */
  gvizArray layers;

  /* gvizArray of `gvizLayer *` pointers scheduled for removal. */
  gvizArray pendingRemove;

  /* The layer the most recent mouse-down was delivered to (for drag tracking).
   * May be NULL. */
  gvizLayer *focused;

  /* Background clear color (RGBA). */
  unsigned char bg[4];
} gvizScene;

/*
 * Lifecycle.
 */
int gvizSceneInit2D(gvizScene *s);
int gvizSceneInit3D(gvizScene *s);
void gvizSceneRelease(gvizScene *s);

/*
 * Ownership: the scene holds raw `gvizLayer *` pointers but does NOT free
 * them on remove. Callers supply heap-allocated layers and reclaim them
 * either via gvizSceneReleaseAllLayers or by removing then freeing manually.
 *
 * To keep memory simple, gvizSceneRelease will release and free every
 * layer currently in the list (calling vtable->release then GVIZ_DEALLOC).
 * If you need a layer to outlive the scene, remove it before release.
 */
int gvizSceneAddLayer(gvizScene *s, gvizLayer *layer);
int gvizSceneRemoveLayer(gvizScene *s, gvizLayer *layer);
int gvizSceneBringToFront(gvizScene *s, gvizLayer *layer);

/*
 * Per-frame API. Call in order: handleInput -> update(dt) -> draw.
 */
void gvizSceneHandleInput(gvizScene *s);
void gvizSceneUpdate(gvizScene *s, float dt);
void gvizSceneDraw(gvizScene *s);

#endif
