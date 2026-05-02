#ifndef _GVIZ_LAYER_GRAPH_TREE_H_
#define _GVIZ_LAYER_GRAPH_TREE_H_

#include "core/gvizScene.h"
#include "dsa/gvizArray.h"
#include "renderer/layers/gvizLayer.h"

/**
 * Screen-space left-strip panel listing every graph registered with the
 * scene, with each registered view as a child entry. Selecting a view
 * activates the layer rendering it (calls `gvizSceneSetActiveLayer`).
 *
 * Per-graph rows are collapsible via a chevron toggle; child view rows
 * render only when their parent is expanded. The panel scrolls
 * vertically when content overflows.
 *
 * Width is `GVIZ_SCENE_MARGIN_L` so the panel sits flush against the
 * active layout region; height matches the screen height.
 */
typedef struct gvizLayerGraphTree {
  gvizLayer layer;
  gvizScene *scene;       /* borrowed; must outlive the panel */
  float scrollY;          /* current vertical scroll offset (px) */
  gvizArray collapsed;    /* unsigned char per graph slot; 1 = collapsed */
} gvizLayerGraphTree;

void gvizLayerGraphTreeInit(gvizLayerGraphTree *self, gvizScene *s, size_t z);

/**
 * Map a screen point inside the panel to a graph handle. Returns
 * `GVIZ_SCENE_GRAPH_INVALID` (== 0) when (sx, sy) is outside the panel or
 * lands on header / view row / empty strip. Mirrors the row layout used by
 * gvizLayerGraphTreeDraw.
 */
gvizSceneGraphHandle gvizLayerGraphTreeHitGraph(const gvizLayerGraphTree *self,
                                                int sx, int sy);

void gvizLayerGraphTreeDraw(void *layer, const struct gvizCamera *camera);
void gvizLayerGraphTreeUpdate(void *layer, float dt);
void gvizLayerGraphTreeRelease(void *layer);
int gvizLayerGraphTreeHandleEvent(void *layer, const struct gvizEvent *event);

static const gvizLayerVTable GVIZ_LAYER_GRAPH_TREE_VTABLE = {
    .draw = gvizLayerGraphTreeDraw,
    .update = gvizLayerGraphTreeUpdate,
    .onEvent = gvizLayerGraphTreeHandleEvent,
    .release = gvizLayerGraphTreeRelease,
};

#endif
