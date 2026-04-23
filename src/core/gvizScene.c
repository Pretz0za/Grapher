#include "core/gvizScene.h"
#include "core/alloc.h"
#include "raylib.h"
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
  return 0;
}

int gvizSceneInit2D(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_2D;
  Vector2 offset = {(float)GetScreenWidth() / 2.0f,
                    (float)GetScreenHeight() / 2.0f};
  s->camera = gvizCameraMake2D((Vector2){0, 0}, offset, 0.0f, 1.0f);
  return 0;
}

int gvizSceneInit3D(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_3D;
  s->camera =
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
  }
  s->pendingRemove.count = 0;
}

static void update2DCamera(gvizScene *s, float wheel) {
  Camera2D *c = &s->camera.c2d;
  if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !s->focused) {
    Vector2 d = GetMouseDelta();
    c->target.x -= d.x / c->zoom;
    c->target.y -= d.y / c->zoom;
  }
  if (wheel != 0.0f) {
    Vector2 m = GetMousePosition();
    Vector2 before = GetScreenToWorld2D(m, *c);
    c->zoom *= (1.0f + wheel * 0.1f);
    Vector2 after = GetScreenToWorld2D(m, *c);
    c->target.x += (before.x - after.x);
    c->target.y += (before.y - after.y);
  }
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

static void fillMouseWorld(const gvizScene *s, float sx, float sy, float *wx,
                           float *wy) {
  Vector2 w = gvizCameraScreenToWorld2D(&s->camera, (Vector2){sx, sy});
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
      s->camera.c2d.offset =
          (Vector2){(float)ev.resize.width / 2.0f,
                    (float)ev.resize.height / 2.0f};
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

  /* Mouse wheel + camera pan/zoom (2D) */
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
      update2DCamera(s, wheel);
    }
  } else if (s->mode == GVIZ_SCENE_2D) {
    update2DCamera(s, 0.0f);
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

  /* World-space pass */
  if (s->mode == GVIZ_SCENE_2D)
    BeginMode2D(s->camera.c2d);
  else
    BeginMode3D(s->camera.c3d);

  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE))
      continue;
    if (l->flags & GVIZ_LAYER_SCREEN_SPACE)
      continue;
    if (l->vtable && l->vtable->draw)
      l->vtable->draw(l, &s->camera);
  }

  if (s->mode == GVIZ_SCENE_2D)
    EndMode2D();
  else
    EndMode3D();

  /* Screen-space pass */
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE))
      continue;
    if (!(l->flags & GVIZ_LAYER_SCREEN_SPACE))
      continue;
    if (l->vtable && l->vtable->draw)
      l->vtable->draw(l, &s->camera);
  }

  EndDrawing();
}
