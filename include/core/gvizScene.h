#ifndef _GVIZ_CORE_SCENE_H_
#define _GVIZ_CORE_SCENE_H_

#include "core/gvizGraphEvent.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizGraphView.h"
#include "renderer/layers/gvizLayer.h"

/*
 * Handle into the scene's graph registry. 0 is reserved as invalid; valid
 * handles are 1-based indices into the entries array.
 */
typedef size_t gvizSceneGraphHandle;
#define GVIZ_SCENE_GRAPH_INVALID ((gvizSceneGraphHandle)0)

typedef struct gvizGraphSubscriber {
  void *self;
  gvizGraphCallback cb;
} gvizGraphSubscriber;

/*
 * Per-view registry entry. The scene owns the `gvizGraphView *` (allocated
 * separately from the layer) and frees it on unregister. `layer` is a
 * borrowed pointer; views are not auto-removed when their layer is removed.
 */
typedef struct gvizSceneViewEntry {
  gvizGraphView *view;     /* scene-owned */
  struct gvizLayer *layer; /* borrowed; may be NULL */
  char *name;              /* scene-owned dup */
} gvizSceneViewEntry;

typedef struct gvizSceneGraphEntry {
  gvizGraph *graph;        /* heap-owned */
  size_t refCount;
  gvizArray subscribers;   /* gvizGraphSubscriber values */
  gvizArray views;         /* gvizSceneViewEntry values */
} gvizSceneGraphEntry;

typedef enum gvizSceneMode {
  GVIZ_SCENE_2D,
  GVIZ_SCENE_3D,
} gvizSceneMode;

/*
 * Scene region layout. The "active region" is the rect inside the window where
 * component layers are laid out. Currently the top 2/3 of the window with
 * left/right margins; the bottom 1/3 is reserved for future HUD/console.
 */
enum {
  GVIZ_SCENE_MARGIN_L = 240,
  GVIZ_SCENE_MARGIN_R = 48,
  GVIZ_SCENE_MARGIN_TOP = 12,
  /* The bottom of the active region sits at (screenH * 2/3); see
   * gvizSceneComputeRegion. The constant below is the gap *below* the active
   * region (i.e. between active region and the empty bottom 1/3). */
  GVIZ_SCENE_MARGIN_BOTTOM = 8,
  GVIZ_SCENE_DIVIDER_GUTTER = 8,
};

/*
 * Compute the active layout region (top 2/3, with margins) for the given
 * window size. Output rect uses gvizViewport.
 */
void gvizSceneComputeRegion(int sw, int sh, gvizViewport *out);

/*
 * Slot subdivision policy.
 */
typedef enum gvizSceneSlotSplit {
  GVIZ_SPLIT_NONE = 0,
  GVIZ_SPLIT_H,   /* split horizontally → left + right slot */
  GVIZ_SPLIT_V,   /* split vertically   → top + bottom slot */
} gvizSceneSlotSplit;

/*
 * Slot tree node. A leaf (`split == GVIZ_SPLIT_NONE`) holds a single
 * component layer. An internal node (`split == H/V`) has two children
 * `a` and `b` (left/top and right/bottom respectively) and a `ratio`
 * giving the relative size of `a`. Each node caches its viewport from
 * the most recent layout pass.
 */
typedef struct gvizSlotNode {
  gvizSceneSlotSplit split;
  struct gvizLayer  *layer;       /* leaf only */
  struct gvizSlotNode *a, *b;     /* internal only */
  float ratio;                    /* internal only, 0..1 */
  gvizViewport viewport;
} gvizSlotNode;

gvizSlotNode *gvizSlotNodeNewLeaf(struct gvizLayer *l);
void          gvizSlotNodeFree(gvizSlotNode *n);

typedef struct gvizSceneLayout {
  gvizViewport  region;        /* current active region */
  gvizSlotNode *root;          /* NULL when no component layers */
} gvizSceneLayout;

/*
 * A gvizScene owns a list of layers (pointers, z-ordered) and a
 * pending-destroy list for layers scheduled to be removed after the current
 * frame. Input is polled from raylib, translated into gvizEvents, and
 * dispatched through the active component layer (with screen-space layers
 * receiving fall-through). Component layers carry their own gvizCamera via
 * the vtable getCamera accessor; the scene itself owns no camera.
 */
typedef struct gvizScene {
  gvizSceneMode mode;

  /* gvizArray of `gvizLayer *` pointers, kept sorted by z ascending. */
  gvizArray layers;

  /* gvizArray of `gvizLayer *` pointers scheduled for removal. */
  gvizArray pendingRemove;

  /* The layer the most recent mouse-down was delivered to (for drag tracking).
   * May be NULL. */
  gvizLayer *focused;

  /* The currently active component layer — receives input first and is
   * highlighted with a border. NULL when there are no component layers. */
  gvizLayer *activeLayer;

  /* Right-click context-menu callbacks. Invoked from gvizSceneHandleInput on
   * a right-click press inside layout.region. Either may be NULL. */
  void (*onEmptyAreaContextMenu)(struct gvizScene *s, int sx, int sy,
                                 void *userdata);
  void (*onLayerContextMenu)(struct gvizScene *s, gvizLayer *layer,
                             int sx, int sy, void *userdata);
  void *contextMenuUserdata;

  /* Active region + slot policy. Recomputed on resize and on layer add/remove. */
  gvizSceneLayout layout;

  /* Graph registry: array of gvizSceneGraphEntry. Index 0 is unused so that
   * handle 0 can serve as invalid. */
  gvizArray graphs;

  /* Background clear color (RGBA). */
  unsigned char bg[4];

  /* Internal: 1 while the user is dragging a slot divider. */
  int dividerDragging;
  gvizSlotNode *draggingNode;
} gvizScene;

/*
 * Replace the leaf hosting @p target with an internal node of split
 * direction @p dir, whose children are the existing leaf and a new leaf
 * holding @p newLayer (added second so it sits in the right/bottom
 * slot). Returns 0 on success.
 */
int gvizSceneSplitLayer(gvizScene *s, gvizLayer *target,
                        gvizSceneSlotSplit dir, gvizLayer *newLayer);

/*
 * Lifecycle.
 */
int gvizSceneInit2D(gvizScene *s);
int gvizSceneInit3D(gvizScene *s);
/*
 * Same as gvizSceneInit2D but the scene starts with no layers — the layer-
 * creation panel chooses the mode when the first layer is added. Defaults
 * to GVIZ_SCENE_2D so 2D math (slot layout etc.) keeps working.
 */
int gvizSceneInitEmpty(gvizScene *s);
void gvizSceneRelease(gvizScene *s);

/* Set the active component layer (the one receiving input + highlighted).
 * NULL is allowed (deactivates input routing). */
void gvizSceneSetActiveLayer(gvizScene *s, gvizLayer *l);

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

/*
 * Walks the scene's component layers (any layer that is not SCREEN_SPACE)
 * and assigns each a viewport rect inside `layout.region` according to the
 * current split policy. Excess layers (>2) are given zero-sized viewports
 * for now and a warning is logged to stderr.
 */
void gvizSceneRecomputeSlots(gvizScene *s);

/*
 * Find the topmost component layer whose viewport contains screen point (sx,sy),
 * or NULL if none. Screen-space layers are skipped.
 */
gvizLayer *gvizSceneFindLayerAt(gvizScene *s, int sx, int sy);

/* ---- Graph registry ----------------------------------------------------- */

/*
 * Register an existing heap-allocated graph with the scene. The scene takes
 * ownership and frees it once refCount drops to 0. Returns a handle (>=1)
 * or GVIZ_SCENE_GRAPH_INVALID on allocation failure.
 */
gvizSceneGraphHandle gvizSceneRegisterGraph(gvizScene *s, gvizGraph *graph);

/* Bump refCount on a registered graph. */
void gvizSceneRetainGraph(gvizScene *s, gvizSceneGraphHandle h);

/* Decrement refCount; frees + clears entry when it hits 0. */
void gvizSceneReleaseGraph(gvizScene *s, gvizSceneGraphHandle h);

/* Resolve a handle to its underlying gvizGraph. Returns NULL if invalid. */
gvizGraph *gvizSceneGetGraph(gvizScene *s, gvizSceneGraphHandle h);

/* Subscribe / unsubscribe to graph mutation events. */
int  gvizSceneSubscribeGraph(gvizScene *s, gvizSceneGraphHandle h,
                             void *self, gvizGraphCallback cb);
void gvizSceneUnsubscribeGraph(gvizScene *s, gvizSceneGraphHandle h,
                               void *self);

/*
 * Fan out an event to all subscribers of @p h, skipping @p originator
 * (pass NULL to notify everyone).
 */
void gvizSceneNotifyGraphChanged(gvizScene *s, gvizSceneGraphHandle h,
                                 void *originator,
                                 gvizGraphEventKind kind,
                                 const void *payload);

/* ---- View registry ------------------------------------------------------ */

/*
 * Register a view of a graph. The scene takes ownership of @p view (must
 * be heap-allocated and already initialised against the graph identified
 * by @p h) and frees it on unregister. @p layer is the layer rendering
 * this view (borrowed; may be NULL). @p name may be NULL.
 */
int gvizSceneRegisterView(gvizScene *s, gvizSceneGraphHandle h,
                          gvizGraphView *view, struct gvizLayer *layer,
                          const char *name);

/*
 * Remove and free the view entry whose `view` pointer matches. Idempotent.
 */
void gvizSceneUnregisterView(gvizScene *s, gvizSceneGraphHandle h,
                             gvizGraphView *view);

/* Borrowed pointer to the views array on a graph entry. NULL on bad handle. */
gvizArray *gvizSceneGetGraphViews(gvizScene *s, gvizSceneGraphHandle h);

#endif
