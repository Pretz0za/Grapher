#include "core/gvizScene.h"
#include "core/alloc.h"
#include "raylib.h"
#include "rlgl.h"
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
  if (gvizArrayInit(&s->graphs, sizeof(gvizSceneGraphEntry)) != 0) {
    gvizArrayRelease(&s->layers);
    gvizArrayRelease(&s->pendingRemove);
    return -1;
  }
  /* Push a sentinel entry so handle 0 is reserved as invalid. */
  gvizSceneGraphEntry sentinel = {0};
  gvizArrayPush(&s->graphs, &sentinel);
  s->focused = NULL;
  s->activeLayer = NULL;
  s->onEmptyAreaContextMenu = NULL;
  s->onLayerContextMenu = NULL;
  s->contextMenuUserdata = NULL;
  s->bg[0] = 245;
  s->bg[1] = 245;
  s->bg[2] = 245;
  s->bg[3] = 255;
  s->layout.root = NULL;
  gvizSceneComputeRegion(GetScreenWidth(), GetScreenHeight(), &s->layout.region);
  s->dividerDragging = 0;
  s->draggingNode = NULL;
  return 0;
}

gvizSlotNode *gvizSlotNodeNewLeaf(gvizLayer *l) {
  gvizSlotNode *n = (gvizSlotNode *)GVIZ_ALLOC(sizeof(gvizSlotNode));
  if (!n) return NULL;
  n->split = GVIZ_SPLIT_NONE;
  n->layer = l;
  n->a = n->b = NULL;
  n->ratio = 0.5f;
  n->viewport = (gvizViewport){0, 0, 0, 0};
  return n;
}

void gvizSlotNodeFree(gvizSlotNode *n) {
  if (!n) return;
  gvizSlotNodeFree(n->a);
  gvizSlotNodeFree(n->b);
  GVIZ_DEALLOC(n);
}

int gvizSceneInit2D(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_2D;
  return 0;
}

int gvizSceneInit3D(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_3D;
  return 0;
}

int gvizSceneInitEmpty(gvizScene *s) {
  if (commonInit(s) != 0)
    return -1;
  s->mode = GVIZ_SCENE_2D;
  return 0;
}

void gvizSceneSetActiveLayer(gvizScene *s, gvizLayer *l) {
  if (!s) return;
  s->activeLayer = l;
}

void gvizSceneRelease(gvizScene *s) {
  if (!s) return;
  if (s->layers.arr) {
    for (size_t i = 0; i < s->layers.count; i++) {
      gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
      if (l && l->vtable && l->vtable->release)
        l->vtable->release(l);
      GVIZ_DEALLOC(l);
    }
    gvizArrayRelease(&s->layers);
  }
  if (s->pendingRemove.arr)
    gvizArrayRelease(&s->pendingRemove);
  if (s->layout.root) {
    gvizSlotNodeFree(s->layout.root);
    s->layout.root = NULL;
  }
  if (s->graphs.arr) {
    for (size_t i = 1; i < s->graphs.count; i++) {
      gvizSceneGraphEntry *e = (gvizSceneGraphEntry *)gvizArrayAtIndex(&s->graphs, i);
      if (e->graph) {
        gvizGraphRelease(e->graph);
        GVIZ_DEALLOC(e->graph);
        e->graph = NULL;
      }
      if (e->subscribers.arr) gvizArrayRelease(&e->subscribers);
    }
    gvizArrayRelease(&s->graphs);
  }
  s->focused = NULL;
  s->activeLayer = NULL;
}

/* ---- Graph registry ----------------------------------------------------- */

static gvizSceneGraphEntry *entryFor(gvizScene *s, gvizSceneGraphHandle h) {
  if (h == GVIZ_SCENE_GRAPH_INVALID || h >= s->graphs.count) return NULL;
  return (gvizSceneGraphEntry *)gvizArrayAtIndex(&s->graphs, h);
}

gvizSceneGraphHandle gvizSceneRegisterGraph(gvizScene *s, gvizGraph *graph) {
  if (!graph) return GVIZ_SCENE_GRAPH_INVALID;
  gvizSceneGraphEntry e;
  e.graph = graph;
  e.refCount = 1;
  if (gvizArrayInit(&e.subscribers, sizeof(gvizGraphSubscriber)) != 0)
    return GVIZ_SCENE_GRAPH_INVALID;
  if (gvizArrayPush(&s->graphs, &e) != 0) {
    gvizArrayRelease(&e.subscribers);
    return GVIZ_SCENE_GRAPH_INVALID;
  }
  return s->graphs.count - 1;
}

void gvizSceneRetainGraph(gvizScene *s, gvizSceneGraphHandle h) {
  gvizSceneGraphEntry *e = entryFor(s, h);
  if (e) e->refCount++;
}

void gvizSceneReleaseGraph(gvizScene *s, gvizSceneGraphHandle h) {
  gvizSceneGraphEntry *e = entryFor(s, h);
  if (!e || e->refCount == 0) return;
  e->refCount--;
  if (e->refCount == 0) {
    if (e->graph) {
      gvizGraphRelease(e->graph);
      GVIZ_DEALLOC(e->graph);
      e->graph = NULL;
    }
    if (e->subscribers.arr) gvizArrayRelease(&e->subscribers);
    /* Slot left as a tombstone — handles never get reused. */
  }
}

gvizGraph *gvizSceneGetGraph(gvizScene *s, gvizSceneGraphHandle h) {
  gvizSceneGraphEntry *e = entryFor(s, h);
  return e ? e->graph : NULL;
}

int gvizSceneSubscribeGraph(gvizScene *s, gvizSceneGraphHandle h,
                            void *self, gvizGraphCallback cb) {
  gvizSceneGraphEntry *e = entryFor(s, h);
  if (!e) return -1;
  gvizGraphSubscriber sub = {self, cb};
  return gvizArrayPush(&e->subscribers, &sub);
}

void gvizSceneUnsubscribeGraph(gvizScene *s, gvizSceneGraphHandle h,
                               void *self) {
  gvizSceneGraphEntry *e = entryFor(s, h);
  if (!e) return;
  for (size_t i = 0; i < e->subscribers.count; i++) {
    gvizGraphSubscriber *sub =
        (gvizGraphSubscriber *)gvizArrayAtIndex(&e->subscribers, i);
    if (sub->self == self) {
      gvizArrayDeleteAtIndex(&e->subscribers, i);
      return;
    }
  }
}

void gvizSceneNotifyGraphChanged(gvizScene *s, gvizSceneGraphHandle h,
                                 void *originator,
                                 gvizGraphEventKind kind,
                                 const void *payload) {
  gvizSceneGraphEntry *e = entryFor(s, h);
  if (!e) return;
  for (size_t i = 0; i < e->subscribers.count; i++) {
    gvizGraphSubscriber *sub =
        (gvizGraphSubscriber *)gvizArrayAtIndex(&e->subscribers, i);
    if (sub->self == originator) continue;
    if (sub->cb) sub->cb(sub->self, kind, payload);
  }
}

static int isComponentLayerFlags(unsigned f) {
  return !(f & GVIZ_LAYER_SCREEN_SPACE);
}

static int isComponentLayer(const gvizLayer *l) {
  return isComponentLayerFlags(l->flags);
}

static gvizSlotNode *findRightmostLeaf(gvizSlotNode *n) {
  if (!n) return NULL;
  if (n->split == GVIZ_SPLIT_NONE) return n;
  return findRightmostLeaf(n->b);
}

static gvizSlotNode *findLeafForLayer(gvizSlotNode *n, const gvizLayer *l) {
  if (!n) return NULL;
  if (n->split == GVIZ_SPLIT_NONE) return (n->layer == l) ? n : NULL;
  gvizSlotNode *r = findLeafForLayer(n->a, l);
  if (r) return r;
  return findLeafForLayer(n->b, l);
}

static gvizSlotNode *findParent(gvizSlotNode *n, const gvizSlotNode *child) {
  if (!n || n->split == GVIZ_SPLIT_NONE) return NULL;
  if (n->a == child || n->b == child) return n;
  gvizSlotNode *r = findParent(n->a, child);
  if (r) return r;
  return findParent(n->b, child);
}

int gvizSceneAddLayer(gvizScene *s, gvizLayer *layer) {
  if (!layer)
    return -1;
  if (gvizArrayPush(&s->layers, &layer) != 0)
    return -1;
  sortLayers(s);
  if (isComponentLayerFlags(layer->flags)) {
    if (!s->layout.root) {
      s->layout.root = gvizSlotNodeNewLeaf(layer);
    } else if (!findLeafForLayer(s->layout.root, layer)) {
      /* No explicit split: collapse into the rightmost leaf as a fallback. */
      gvizSlotNode *rm = findRightmostLeaf(s->layout.root);
      if (rm && !rm->layer) rm->layer = layer;
    }
  }
  gvizSceneRecomputeSlots(s);
  if (!s->activeLayer && isComponentLayerFlags(layer->flags))
    s->activeLayer = layer;
  return 0;
}

int gvizSceneSplitLayer(gvizScene *s, gvizLayer *target,
                        gvizSceneSlotSplit dir, gvizLayer *newLayer) {
  if (!s || !newLayer || dir == GVIZ_SPLIT_NONE) return -1;
  /* Push the layer into s->layers first if not already there. */
  int already = 0;
  for (size_t i = 0; i < s->layers.count; i++) {
    if (*(gvizLayer **)gvizArrayAtIndex(&s->layers, i) == newLayer) {
      already = 1;
      break;
    }
  }
  if (!already) {
    if (gvizArrayPush(&s->layers, &newLayer) != 0) return -1;
    sortLayers(s);
  }
  if (!s->layout.root) {
    s->layout.root = gvizSlotNodeNewLeaf(newLayer);
    gvizSceneRecomputeSlots(s);
    if (!s->activeLayer && isComponentLayerFlags(newLayer->flags))
      s->activeLayer = newLayer;
    return 0;
  }
  gvizSlotNode *leaf = target ? findLeafForLayer(s->layout.root, target) : NULL;
  if (!leaf) leaf = findRightmostLeaf(s->layout.root);
  if (!leaf) return -1;
  /* Promote leaf into an internal node by giving it two children. */
  gvizSlotNode *oldLeaf = gvizSlotNodeNewLeaf(leaf->layer);
  gvizSlotNode *newLeaf = gvizSlotNodeNewLeaf(newLayer);
  if (!oldLeaf || !newLeaf) {
    if (oldLeaf) GVIZ_DEALLOC(oldLeaf);
    if (newLeaf) GVIZ_DEALLOC(newLeaf);
    return -1;
  }
  leaf->split = dir;
  leaf->layer = NULL;
  leaf->a = oldLeaf;
  leaf->b = newLeaf;
  leaf->ratio = 0.5f;
  gvizSceneRecomputeSlots(s);
  if (!s->activeLayer && isComponentLayerFlags(newLayer->flags))
    s->activeLayer = newLayer;
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

static void removeLayerFromSlotTree(gvizScene *s, gvizLayer *l) {
  if (!s->layout.root) return;
  gvizSlotNode *leaf = findLeafForLayer(s->layout.root, l);
  if (!leaf) return;
  if (leaf == s->layout.root) {
    gvizSlotNodeFree(s->layout.root);
    s->layout.root = NULL;
    s->draggingNode = NULL;
    return;
  }
  gvizSlotNode *parent = findParent(s->layout.root, leaf);
  if (!parent) return;
  gvizSlotNode *sibling = (parent->a == leaf) ? parent->b : parent->a;
  /* Splice sibling into the parent slot. */
  gvizSlotNode *gp = findParent(s->layout.root, parent);
  if (!gp) {
    /* parent is root */
    s->layout.root = sibling;
  } else {
    if (gp->a == parent) gp->a = sibling;
    else                 gp->b = sibling;
  }
  parent->a = parent->b = NULL;
  GVIZ_DEALLOC(parent);
  GVIZ_DEALLOC(leaf);
  if (s->draggingNode == parent || s->draggingNode == leaf) {
    s->draggingNode = NULL;
    s->dividerDragging = 0;
  }
}

static void flushPendingRemoves(gvizScene *s) {
  int removed = 0;
  for (size_t i = 0; i < s->pendingRemove.count; i++) {
    gvizLayer *victim =
        *(gvizLayer **)gvizArrayAtIndex(&s->pendingRemove, i);
    int idx = findLayerIndex(&s->layers, victim);
    if (idx < 0)
      continue;
    if (isComponentLayer(victim))
      removeLayerFromSlotTree(s, victim);
    gvizArrayDeleteAtIndex(&s->layers, (size_t)idx);
    if (s->focused == victim)
      s->focused = NULL;
    int wasActive = (s->activeLayer == victim);
    if (wasActive)
      s->activeLayer = NULL;
    if (victim->vtable && victim->vtable->release)
      victim->vtable->release(victim);
    GVIZ_DEALLOC(victim);
    removed = 1;
    if (wasActive) {
      /* Pick the topmost remaining component layer. */
      for (size_t k = s->layers.count; k-- > 0;) {
        gvizLayer *cand = *(gvizLayer **)gvizArrayAtIndex(&s->layers, k);
        if (!(cand->flags & GVIZ_LAYER_SCREEN_SPACE)) {
          s->activeLayer = cand;
          break;
        }
      }
    }
  }
  s->pendingRemove.count = 0;
  if (removed) gvizSceneRecomputeSlots(s);
}

static void layoutSlotTree(gvizSlotNode *n, gvizViewport r) {
  if (!n) return;
  n->viewport = r;
  if (n->split == GVIZ_SPLIT_NONE) {
    if (n->layer) n->layer->viewport = r;
    return;
  }
  int gutter = GVIZ_SCENE_DIVIDER_GUTTER;
  if (n->split == GVIZ_SPLIT_H) {
    int firstW = (int)(r.width * n->ratio);
    gvizViewport ra = {r.x, r.y, firstW - gutter / 2, r.height};
    gvizViewport rb = {r.x + firstW + gutter / 2, r.y,
                       r.width - firstW - gutter / 2, r.height};
    layoutSlotTree(n->a, ra);
    layoutSlotTree(n->b, rb);
  } else {
    int firstH = (int)(r.height * n->ratio);
    gvizViewport ra = {r.x, r.y, r.width, firstH - gutter / 2};
    gvizViewport rb = {r.x, r.y + firstH + gutter / 2, r.width,
                       r.height - firstH - gutter / 2};
    layoutSlotTree(n->a, ra);
    layoutSlotTree(n->b, rb);
  }
}

void gvizSceneRecomputeSlots(gvizScene *s) {
  /* Zero out viewports first so layers not represented in the tree
   * don't draw with stale rects. */
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (isComponentLayer(l))
      l->viewport = (gvizViewport){0, 0, 0, 0};
  }
  if (s->layout.root)
    layoutSlotTree(s->layout.root, s->layout.region);
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

static gvizSlotNode *hitInternalDivider(gvizSlotNode *n, int sx, int sy,
                                        int *cursorOut) {
  if (!n || n->split == GVIZ_SPLIT_NONE) return NULL;
  /* Recurse depth-first so deeper (smaller) dividers win. */
  gvizSlotNode *r = hitInternalDivider(n->a, sx, sy, cursorOut);
  if (r) return r;
  r = hitInternalDivider(n->b, sx, sy, cursorOut);
  if (r) return r;
  gvizViewport vp = n->viewport;
  if (n->split == GVIZ_SPLIT_H) {
    int gx = vp.x + (int)(vp.width * n->ratio);
    if (sx >= gx - GVIZ_SCENE_DIVIDER_GUTTER &&
        sx <= gx + GVIZ_SCENE_DIVIDER_GUTTER &&
        sy >= vp.y && sy < vp.y + vp.height) {
      if (cursorOut) *cursorOut = MOUSE_CURSOR_RESIZE_EW;
      return n;
    }
  } else {
    int gy = vp.y + (int)(vp.height * n->ratio);
    if (sy >= gy - GVIZ_SCENE_DIVIDER_GUTTER &&
        sy <= gy + GVIZ_SCENE_DIVIDER_GUTTER &&
        sx >= vp.x && sx < vp.x + vp.width) {
      if (cursorOut) *cursorOut = MOUSE_CURSOR_RESIZE_NS;
      return n;
    }
  }
  return NULL;
}

static int dividerGutterContains(const gvizScene *s, int sx, int sy,
                                 int *cursorOut) {
  return hitInternalDivider(s->layout.root, sx, sy, cursorOut) != NULL;
}

static void dragDivider(gvizScene *s, int sx, int sy) {
  gvizSlotNode *n = s->draggingNode;
  if (!n) return;
  gvizViewport vp = n->viewport;
  float ratio;
  if (n->split == GVIZ_SPLIT_H) {
    if (vp.width <= 0) return;
    ratio = (float)(sx - vp.x) / (float)vp.width;
  } else {
    if (vp.height <= 0) return;
    ratio = (float)(sy - vp.y) / (float)vp.height;
  }
  if (ratio < 0.1f) ratio = 0.1f;
  if (ratio > 0.9f) ratio = 0.9f;
  n->ratio = ratio;
  gvizSceneRecomputeSlots(s);
}

/*
 * Dispatch an event. Screen-space layers (modals/HUD) get first dibs in
 * top-down z order — they pre-empt active routing. If none consume, the
 * scene's activeLayer (the focused component layer) receives the event.
 */
static int dispatchEvent(gvizScene *s, const gvizEvent *ev) {
  for (size_t i = s->layers.count; i-- > 0;) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE)) continue;
    if (!(l->flags & GVIZ_LAYER_SCREEN_SPACE)) continue;
    if (l->vtable && l->vtable->onEvent) {
      if (l->vtable->onEvent(l, ev))
        return 1;
    }
    if (l->flags & GVIZ_LAYER_CAPTURES_INPUT)
      return 1;
  }
  gvizLayer *al = s->activeLayer;
  if (al && (al->flags & GVIZ_LAYER_VISIBLE) &&
      al->vtable && al->vtable->onEvent) {
    if (al->vtable->onEvent(al, ev))
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
  return layerCamera(l);
}

static void fillMouseWorld(gvizScene *s, float sx, float sy, float *wx,
                           float *wy) {
  gvizCamera *cam = cameraAt(s, (int)sx, (int)sy);
  if (!cam) {
    *wx = sx;
    *wy = sy;
    return;
  }
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
      s->draggingNode = NULL;
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
      if (i == 0 && !s->dividerDragging) {
        gvizSlotNode *hit = hitInternalDivider(s->layout.root,
                                               (int)mp.x, (int)mp.y, NULL);
        if (hit) {
          s->dividerDragging = 1;
          s->draggingNode = hit;
          continue;
        }
      }
      /* Right-click → context-menu hooks (only when no screen-space layer
       * is up to absorb it; we approximate by always firing — modals can
       * still pre-empt via dispatchEvent below). */
      int sxI = (int)mp.x, syI = (int)mp.y;
      int inRegion = sxI >= s->layout.region.x &&
                     sxI < s->layout.region.x + s->layout.region.width &&
                     syI >= s->layout.region.y &&
                     syI < s->layout.region.y + s->layout.region.height;
      if (gbtns[i] == GVIZ_MOUSE_RIGHT && inRegion) {
        gvizLayer *hit = gvizSceneFindLayerAt(s, sxI, syI);
        int hasModal = 0;
        for (size_t k = s->layers.count; k-- > 0;) {
          gvizLayer *ll = *(gvizLayer **)gvizArrayAtIndex(&s->layers, k);
          if (ll->flags & GVIZ_LAYER_SCREEN_SPACE) { hasModal = 1; break; }
        }
        if (!hasModal) {
          if (!hit && s->onEmptyAreaContextMenu) {
            s->onEmptyAreaContextMenu(s, sxI, syI, s->contextMenuUserdata);
            continue;
          }
          if (hit && s->onLayerContextMenu) {
            s->onLayerContextMenu(s, hit, sxI, syI, s->contextMenuUserdata);
            continue;
          }
        }
      }
      /* Left-click on a different component layer flips activeLayer first. */
      if (gbtns[i] == GVIZ_MOUSE_LEFT) {
        gvizLayer *hit = gvizSceneFindLayerAt(s, sxI, syI);
        if (hit && hit != s->activeLayer)
          s->activeLayer = hit;
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

  /* Per-frame camera input for the active layer. Resolve content bounds once
   * for T/L/R shortcuts; pass zeros when the layer doesn't provide them. */
  {
    gvizLayer  *al  = s->activeLayer;
    gvizCamera *cam = layerCamera(al);
    if (cam && al) {
      Vector3 centroid = {0};
      float   radius   = 0.0f;
      if (al->vtable && al->vtable->getContentBounds)
        al->vtable->getContentBounds(al, &centroid, &radius);
      if (cam->kind == GVIZ_CAMERA_3D)
        gvizCameraHandleInput3D(cam, al->viewport.x, al->viewport.y,
                                al->viewport.width, al->viewport.height,
                                1, centroid, radius);
    }
  }

  /* 2D camera: T/R also need centroid — resolve once here. */
  Vector3 centroid2D3 = {0}; float radius2D = 0.0f;
  {
    gvizLayer *al = s->activeLayer;
    if (al && al->vtable && al->vtable->getContentBounds)
      al->vtable->getContentBounds(al, &centroid2D3, &radius2D);
  }
  Vector2 centroid2D = {centroid2D3.x, centroid2D3.y};

  float wheel = GetMouseWheelMove();
  if (wheel != 0.0f) {
    gvizEvent ev = {0};
    ev.type = GVIZ_EVENT_MOUSE_WHEEL;
    ev.wheel.sx = mp.x; ev.wheel.sy = mp.y;
    fillMouseWorld(s, mp.x, mp.y, &ev.wheel.wx, &ev.wheel.wy);
    ev.wheel.dy = wheel; ev.wheel.mods = mods;
    if (!dispatchEvent(s, &ev)) {
      gvizLayer *l = s->activeLayer;
      gvizCamera *cam = layerCamera(l);
      if (cam && cam->kind == GVIZ_CAMERA_2D)
        gvizCameraHandleInput2D(cam, l->viewport.x, l->viewport.y,
                                l->viewport.width, l->viewport.height,
                                mp.x, mp.y, md.x, md.y, wheel,
                                IsMouseButtonDown(MOUSE_BUTTON_LEFT), 1,
                                centroid2D, radius2D);
    }
  } else if (!s->dividerDragging) {
    int lh = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (lh && !s->focused) {
      gvizLayer *l = s->activeLayer;
      gvizCamera *cam = layerCamera(l);
      if (cam && cam->kind == GVIZ_CAMERA_2D)
        gvizCameraHandleInput2D(cam, l->viewport.x, l->viewport.y,
                                l->viewport.width, l->viewport.height,
                                mp.x, mp.y, md.x, md.y, 0.0f, 1, 1,
                                centroid2D, radius2D);
    }
  }
  /* 2D T/R don't depend on drag/wheel — call handler unconditionally too. */
  if (!s->dividerDragging) {
    gvizLayer *l = s->activeLayer;
    gvizCamera *cam = layerCamera(l);
    if (cam && cam->kind == GVIZ_CAMERA_2D && l)
      gvizCameraHandleInput2D(cam, l->viewport.x, l->viewport.y,
                              l->viewport.width, l->viewport.height,
                              mp.x, mp.y, 0.0f, 0.0f, 0.0f, 0, 1,
                              centroid2D, radius2D);
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

  size_t componentCount = 0;

  /* Component layers: each in its own viewport scissor + per-layer camera. */
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE)) continue;
    if (l->flags & GVIZ_LAYER_SCREEN_SPACE) continue;
    if (!l->vtable || !l->vtable->draw) continue;

    gvizCamera *cam = layerCamera(l);
    if (!cam) continue;
    /* Recenter 2D camera offset to the viewport centre so world-(0,0)
     * lands at the slot's middle. */
    if (cam->kind == GVIZ_CAMERA_2D) {
      cam->c2d.offset = (Vector2){l->viewport.x + l->viewport.width * 0.5f,
                                  l->viewport.y + l->viewport.height * 0.5f};
    }

    if (l->viewport.width > 0 && l->viewport.height > 0)
      BeginScissorMode(l->viewport.x, l->viewport.y,
                       l->viewport.width, l->viewport.height);
    if (cam->kind == GVIZ_CAMERA_2D) {
      BeginMode2D(cam->c2d);
    } else {
      rlSetClipPlanes(0.01, 1e8);
      BeginMode3D(cam->c3d);
    }
    l->vtable->draw(l, cam);
    if (cam->kind == GVIZ_CAMERA_2D) {
      EndMode2D();
    } else {
      EndMode3D();
      rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    }
    if (l->viewport.width > 0 && l->viewport.height > 0)
      EndScissorMode();

    if (l == s->activeLayer && l->viewport.width > 0 && l->viewport.height > 0) {
      Rectangle r = {(float)l->viewport.x, (float)l->viewport.y,
                     (float)l->viewport.width, (float)l->viewport.height};
      DrawRectangleLinesEx(r, 2.0f, (Color){70, 140, 255, 200});
    }
    componentCount++;
  }

  /* Empty-region placeholder. */
  if (componentCount == 0) {
    const char *msg = "No current scene, create scene";
    int fontSize = 22;
    int tw = MeasureText(msg, fontSize);
    int cx = s->layout.region.x + s->layout.region.width / 2 - tw / 2;
    int cy = s->layout.region.y + s->layout.region.height / 2 - fontSize / 2;
    DrawText(msg, cx, cy, fontSize, (Color){140, 140, 140, 255});
  }

  /* Screen-space layers (menus, HUD) drawn last in raw screen coords. */
  for (size_t i = 0; i < s->layers.count; i++) {
    gvizLayer *l = *(gvizLayer **)gvizArrayAtIndex(&s->layers, i);
    if (!(l->flags & GVIZ_LAYER_VISIBLE)) continue;
    if (!(l->flags & GVIZ_LAYER_SCREEN_SPACE)) continue;
    if (l->vtable && l->vtable->draw)
      l->vtable->draw(l, NULL);
  }

  EndDrawing();
}
