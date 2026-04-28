#include "core/gvizScene.h"
#include "core/alloc.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmpLayerPtrByZ(const void *a, const void *b) {
  const gvizLayer *la = *(const gvizLayer *const *)a;
  const gvizLayer *lb = *(const gvizLayer *const *)b;
  if (la->z < lb->z)
    return -1;
  if (la->z > lb->z)
    return 1;
  return 0;
}

static void sortLayers(gvizScene *s) {
  if (s->layers.count < 2)
    return;
  qsort(s->layers.arr, s->layers.count, s->layers.elementSize,
        cmpLayerPtrByZ);
}

static unsigned int currentModMask(void) {
  unsigned int m = 0;
  if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
    m |= GVIZ_MOD_SHIFT;
  if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL))
    m |= GVIZ_MOD_CTRL;
  if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT))
    m |= GVIZ_MOD_ALT;
  if (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER))
    m |= GVIZ_MOD_SUPER;
  return m;
}

void gvizSceneComputeRegion(int sw, int sh, gvizViewport *out) {
  int top = GVIZ_SCENE_MARGIN_TOP;
  int bottomLimit = (int)(sh * 2.0f / 3.0f) - GVIZ_SCENE_MARGIN_BOTTOM;
  int left = GVIZ_SCENE_MARGIN_L;
  int right = sw - GVIZ_SCENE_MARGIN_R;
  if (right < left) right = left;
  if (bottomLimit < top) bottomLimit = top;
  out->x = left;
  out->y = top;
  out->width = right - left;
  out->height = bottomLimit - top;
}

static int commonInit(gvizScene *s) {
  if (gvizArrayInit(&s->layers, sizeof(gvizLayer *)) != 0)
    return -1;
  if (gvizArrayInit(&s->pendingRemove, sizeof(gvizLayer *)) != 0) {
    gvizArrayRelease(&s->layers);
    return -1;
  }
  s->focused = NULL;
  s->bg[0] = 245;
  s->bg[1] = 245;
  s->bg[2] = 245;
  s->bg[3] = 255;
  s->layout.split = GVIZ_SPLIT_NONE;
  s->layout.splitRatio = 0.5f;
  gvizSceneComputeRegion(GetScreenWidth(), GetScreenHeight(), &s->layout.region);
  s->dividerDragging = 0;
  return 0;
}

int gvizSceneInit2D(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_2D;
  Vector2 offset = {(float)GetScreenWidth() / 2.0f,
                    (float)GetScreenHeight() / 2.0f};
  s->defaultCamera = gvizCameraMake2D((Vector2){0, 0}, offset, 0.0f, 1.0f);
  return 0;
}

int gvizSceneInit3D(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_3D;
  s->defaultCamera =
      gvizCameraMake3D((Vector3){0, 0, 2000}, (Vector3){0, 0, 0},
                       (Vector3){0, 1, 0}, 45.0f, CAMERA_PERSPECTIVE);
  return 0;
}

void gvizSceneRelease(gvizScene *s) {
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (l && l->vtable && l->vtable->release)
      l->vtable->release(l);
    GVIZ_DEALLOC(l);
  }
  gvizArrayRelease(&s->layers);
  gvizArrayRelease(&s->pendingRemove);
  s->focused = NULL;
}

int gvizSceneAddLayer(gvizScene *s, gvizLayer *layer) {
  if (!layer)
    return -1;
  if (gvizArrayPush(&s->layers, &layer) != 0)
    return -1;
  sortLayers(s);
  gvizSceneRecomputeSlots(s);
  return 0;
}

static int findLayerIndex(const gvizArray *arr, const gvizLayer *layer) {
  for (size_t i = 0; i < arr->count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(arr, i);
    if (l == layer)
      return (int)i;
  }
  return -1;
}

int gvizSceneRemoveLayer(gvizScene *s, gvizLayer *layer) {
  return gvizArrayPush(&s->pendingRemove, &layer);
}

int gvizSceneBringToFront(gvizScene *s, gvizLayer *layer) {
  if (!layer || s->layers.count == 0)
    return -1;
  gvizLayer *top =
      *(gvizLayer **)gvizArrayAtIndex(&s->layers, s->layers.count - 1);
  if (top == layer)
    return 0;
  layer->z = top->z + 1;
  sortLayers(s);
  return 0;
}

static void flushPendingRemoves(gvizScene *s) {
  int removed = 0;
  for (size_t i = 0; i < s->pendingRemove.count; i++) {
    gvizLayer *victim =
        *(gvizLayer **)gvizArrayAtIndex(&s->pendingRemove, i);
    int idx = findLayerIndex(&s->layers, victim);
    if (idx < 0)
      continue;
    gvizArrayDeleteAtIndex(&s->layers, (size_t)idx);
    if (s->focused == victim)
      s->focused = NULL;
    if (victim->vtable && victim->vtable->release)
      victim->vtable->release(victim);
    GVIZ_DEALLOC(victim);
    removed = 1;
  }
  s->pendingRemove.count = 0;
  if (removed) gvizSceneRecomputeSlots(s);
}

static int isComponentLayer(const gvizLayer *l) {
  return !(l->flags & GVIZ_LAYER_SCREEN_SPACE);
}

static size_t countComponentLayers(const gvizScene *s) {
  size_t n = 0;
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (isComponentLayer(l)) n++;
  }
  return n;
}

void gvizSceneRecomputeSlots(gvizScene *s) {
  size_t n = countComponentLayers(s);
  /* default split policy: 1 layer = none; 2 layers = horizontal */
  if (n <= 1) s->layout.split = GVIZ_SPLIT_NONE;
  else        s->layout.split = GVIZ_SPLIT_H;

  gvizViewport r = s->layout.region;
  size_t assigned = 0;
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!isComponentLayer(l)) continue;
    if (assigned >= 2 && n > 2) {
      l->viewport = (gvizViewport){0, 0, 0, 0};
      assigned++;
      fprintf(stderr, "[scene] >2 component layers; extras hidden\n");
      continue;
    }
    if (n <= 1) {
      l->viewport = r;
    } else if (s->layout.split == GVIZ_SPLIT_H) {
      int firstW = (int)(r.width * s->layout.splitRatio);
      if (assigned == 0)
        l->viewport = (gvizViewport){r.x, r.y, firstW - GVIZ_SCENE_DIVIDER_GUTTER/2, r.height};
      else
        l->viewport = (gvizViewport){r.x + firstW + GVIZ_SCENE_DIVIDER_GUTTER/2,
                                     r.y,
                                     r.width - firstW - GVIZ_SCENE_DIVIDER_GUTTER/2,
                                     r.height};
    } else { /* SPLIT_V */
      int firstH = (int)(r.height * s->layout.splitRatio);
      if (assigned == 0)
        l->viewport = (gvizViewport){r.x, r.y, r.width, firstH - GVIZ_SCENE_DIVIDER_GUTTER/2};
      else
        l->viewport = (gvizViewport){r.x,
                                     r.y + firstH + GVIZ_SCENE_DIVIDER_GUTTER/2,
                                     r.width,
                                     r.height - firstH - GVIZ_SCENE_DIVIDER_GUTTER/2};
    }
    assigned++;
  }
}

static int viewportContains(const gvizViewport *vp, int sx, int sy) {
  return sx >= vp->x && sx < vp->x + vp->width &&
         sy >= vp->y && sy < vp->y + vp->height;
}

gvizLayer *gvizSceneFindLayerAt(gvizScene *s, int sx, int sy) {
  for (size_t i = s->layers.count; i-- > 0;) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!isComponentLayer(l)) continue;
    if (!(l->flags & GVIZ_LAYER_VISIBLE)) continue;
    if (viewportContains(&l->viewport, sx, sy)) return l;
  }
  return NULL;
}

/* ---- Divider gutter ----------------------------------------------------- */

static int dividerGutterContains(const gvizScene *s, int sx, int sy,
                                 int *cursorOut) {
  if (s->layout.split == GVIZ_SPLIT_NONE) return 0;
  gvizViewport r = s->layout.region;
  if (s->layout.split == GVIZ_SPLIT_H) {
    int gx = r.x + (int)(r.width * s->layout.splitRatio);
    if (sx >= gx - GVIZ_SCENE_DIVIDER_GUTTER &&
        sx <= gx + GVIZ_SCENE_DIVIDER_GUTTER &&
        sy >= r.y && sy < r.y + r.height) {
      if (cursorOut) *cursorOut = MOUSE_CURSOR_RESIZE_EW;
      return 1;
    }
  } else {
    int gy = r.y + (int)(r.height * s->layout.splitRatio);
    if (sy >= gy - GVIZ_SCENE_DIVIDER_GUTTER &&
        sy <= gy + GVIZ_SCENE_DIVIDER_GUTTER &&
        sx >= r.x && sx < r.x + r.width) {
      if (cursorOut) *cursorOut = MOUSE_CURSOR_RESIZE_NS;
      return 1;
    }
  }
  return 0;
}

static void dragDivider(gvizScene *s, int sx, int sy) {
  gvizViewport r = s->layout.region;
  float ratio;
  if (s->layout.split == GVIZ_SPLIT_H) {
    if (r.width <= 0) return;
    ratio = (float)(sx - r.x) / (float)r.width;
  } else {
    if (r.height <= 0) return;
    ratio = (float)(sy - r.y) / (float)r.height;
  }
  if (ratio < 0.1f) ratio = 0.1f;
  if (ratio > 0.9f) ratio = 0.9f;
  s->layout.splitRatio = ratio;
  gvizSceneRecomputeSlots(s);
}

/*
 * Dispatch an event to layers in top-down z order. Stops at the first
 * handler that returns 1.
 */
static int dispatchEvent(gvizScene *s, const gvizEvent *ev) {
  for (size_t i = s->layers.count; i-- > 0;) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE))
      continue;
    if (l->vtable && l->vtable->onEvent) {
      if (l->vtable->onEvent(l, ev))
        return 1;
    }
    if (l->flags & GVIZ_LAYER_CAPTURES_INPUT)
      return 1;
  }
  return 0;
}

static gvizCamera *layerCamera(gvizLayer *l) {
  if (!l || !l->vtable || !l->vtable->getCamera) return NULL;
  return l->vtable->getCamera(l);
}

static gvizCamera *cameraAt(gvizScene *s, int sx, int sy) {
  gvizLayer *l = gvizSceneFindLayerAt(s, sx, sy);
  gvizCamera *cam = layerCamera(l);
  return cam ? cam : &s->defaultCamera;
}

static void fillMouseWorld(gvizScene *s, float sx, float sy, float *wx,
                           float *wy) {
  gvizCamera *cam = cameraAt(s, (int)sx, (int)sy);
  Vector2 w = gvizCameraScreenToWorld2D(cam, (Vector2){sx, sy});
  *wx = w.x;
  *wy = w.y;
}

void gvizSceneHandleInput(gvizScene *s) {
  Vector2 mp = GetMousePosition();
  Vector2 md = GetMouseDelta();
  unsigned int mods = currentModMask();

  /* Resize */
  if (IsWindowResized()) {
    gvizEvent ev = {0};
    ev.type = GVIZ_EVENT_RESIZE;
    ev.resize.width = GetScreenWidth();
    ev.resize.height = GetScreenHeight();
    dispatchEvent(s, &ev);
    if (s->mode == GVIZ_SCENE_2D) {
      s->defaultCamera.c2d.offset =
          (Vector2){(float)ev.resize.width / 2.0f,
                    (float)ev.resize.height / 2.0f};
    }
    gvizSceneComputeRegion(ev.resize.width, ev.resize.height, &s->layout.region);
    gvizSceneRecomputeSlots(s);
  }

  /* Cursor hint over divider */
  int cursorKind = MOUSE_CURSOR_DEFAULT;
  if (dividerGutterContains(s, (int)mp.x, (int)mp.y, &cursorKind))
    SetMouseCursor(cursorKind);
  else if (!s->dividerDragging)
    SetMouseCursor(MOUSE_CURSOR_DEFAULT);

  /* Divider drag */
  if (s->dividerDragging) {
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
      dragDivider(s, (int)mp.x, (int)mp.y);
    } else {
      s->dividerDragging = 0;
    }
  }

  /* Mouse move */
  if (md.x != 0.0f || md.y != 0.0f) {
    gvizEvent ev = {0};
    ev.type = GVIZ_EVENT_MOUSE_MOVE;
    ev.mouse.sx = mp.x;
    ev.mouse.sy = mp.y;
    fillMouseWorld(s, mp.x, mp.y, &ev.mouse.wx, &ev.mouse.wy);
    ev.mouse.button = -1;
    ev.mouse.mods = mods;
    dispatchEvent(s, &ev);
  }

  /* Mouse buttons */
  static const int btns[3] = {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT,
                              MOUSE_BUTTON_MIDDLE};
  static const int gbtns[3] = {GVIZ_MOUSE_LEFT, GVIZ_MOUSE_RIGHT,
                               GVIZ_MOUSE_MIDDLE};
  for (int i = 0; i < 3; i++) {
    if (IsMouseButtonPressed(btns[i])) {
      /* Begin divider drag if applicable */
      if (i == 0 && !s->dividerDragging &&
          dividerGutterContains(s, (int)mp.x, (int)mp.y, NULL)) {
        s->dividerDragging = 1;
        continue;
      }
      gvizEvent ev = {0};
      ev.type = GVIZ_EVENT_MOUSE_DOWN;
      ev.mouse.sx = mp.x;
      ev.mouse.sy = mp.y;
      fillMouseWorld(s, mp.x, mp.y, &ev.mouse.wx, &ev.mouse.wy);
      ev.mouse.button = gbtns[i];
      ev.mouse.mods = mods;
      dispatchEvent(s, &ev);
    }
    if (IsMouseButtonReleased(btns[i])) {
      gvizEvent ev = {0};
      ev.type = GVIZ_EVENT_MOUSE_UP;
      ev.mouse.sx = mp.x;
      ev.mouse.sy = mp.y;
      fillMouseWorld(s, mp.x, mp.y, &ev.mouse.wx, &ev.mouse.wy);
      ev.mouse.button = gbtns[i];
      ev.mouse.mods = mods;
      dispatchEvent(s, &ev);
    }
  }

  /* Mouse wheel + per-layer camera pan/zoom (2D) */
  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    gvizEvent ev = {0};
    ev.type = GVIZ_EVENT_MOUSE_WHEEL;
    ev.wheel.sx = mp.x;
    ev.wheel.sy = mp.y;
    fillMouseWorld(s, mp.x, mp.y, &ev.wheel.wx, &ev.wheel.wy);
    ev.wheel.dy = wheel;
    ev.wheel.mods = mods;
    if (!dispatchEvent(s, &ev) && s->mode == GVIZ_SCENE_2D) {
      gvizLayer *l = gvizSceneFindLayerAt(s, (int)mp.x, (int)mp.y);
      gvizCamera *cam = layerCamera(l);
      if (cam && cam->kind == GVIZ_CAMERA_2D)
        gvizCameraHandleInput2D(cam, l->viewport.x, l->viewport.y,
                                l->viewport.width, l->viewport.height,
                                mp.x, mp.y, md.x, md.y, wheel,
                                IsMouseButtonDown(MOUSE_BUTTON_LEFT), 1);
    }
  } else if (s->mode == GVIZ_SCENE_2D && !s->dividerDragging) {
    /* No wheel: only pan if left-button is held */
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !s->focused) {
      gvizLayer *l = gvizSceneFindLayerAt(s, (int)mp.x, (int)mp.y);
      gvizCamera *cam = layerCamera(l);
      if (cam && cam->kind == GVIZ_CAMERA_2D)
        gvizCameraHandleInput2D(cam, l->viewport.x, l->viewport.y,
                                l->viewport.width, l->viewport.height,
                                mp.x, mp.y, md.x, md.y, 0.0f, 1, 1);
    }
  }

  /* Text input */
  int cp;
  while ((cp = GetCharPressed()) != 0) {
    gvizEvent ev = {0};
    ev.type = GVIZ_EVENT_TEXT_INPUT;
    ev.text.codepoint = cp;
    dispatchEvent(s, &ev);
  }

  /* Keys */
  int key;
  while ((key = GetKeyPressed()) != 0) {
    gvizEvent ev = {0};
    ev.type = GVIZ_EVENT_KEY_DOWN;
    ev.key.key = key;
    ev.key.mods = mods;
    dispatchEvent(s, &ev);
  }
}

void gvizSceneUpdate(gvizScene *s, float dt) {
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE))
      continue;
    if (l->vtable && l->vtable->update)
      l->vtable->update(l, dt);
  }
  flushPendingRemoves(s);
}

void gvizSceneDraw(gvizScene *s) {
  BeginDrawing();
  ClearBackground((Color){s->bg[0], s->bg[1], s->bg[2], s->bg[3]});

  /* Component layers: each in its own viewport scissor + per-layer camera. */
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE)) continue;
    if (l->flags & GVIZ_LAYER_SCREEN_SPACE) continue;
    if (!l->vtable || !l->vtable->draw) continue;

    gvizCamera *cam = layerCamera(l);
    if (!cam) cam = &s->defaultCamera;
    /* Recenter 2D camera offset to the viewport centre so world-(0,0)
     * lands at the slot's middle. */
    if (cam->kind == GVIZ_CAMERA_2D) {
      cam->c2d.offset = (Vector2){l->viewport.x + l->viewport.width * 0.5f,
                                  l->viewport.y + l->viewport.height * 0.5f};
    }

    if (l->viewport.width > 0 && l->viewport.height > 0)
      BeginScissorMode(l->viewport.x, l->viewport.y,
                       l->viewport.width, l->viewport.height);
    if (cam->kind == GVIZ_CAMERA_2D) BeginMode2D(cam->c2d);
    else                              BeginMode3D(cam->c3d);
    l->vtable->draw(l, cam);
    if (cam->kind == GVIZ_CAMERA_2D) EndMode2D();
    else                              EndMode3D();
    if (l->viewport.width > 0 && l->viewport.height > 0)
      EndScissorMode();
  }

  /* Screen-space layers (menus, HUD) drawn last in raw screen coords. */
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE)) continue;
    if (!(l->flags & GVIZ_LAYER_SCREEN_SPACE)) continue;
    if (l->vtable && l->vtable->draw)
      l->vtable->draw(l, &s->defaultCamera);
  }

  EndDrawing();
}
