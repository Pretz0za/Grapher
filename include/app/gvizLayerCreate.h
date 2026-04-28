#ifndef _GVIZ_APP_LAYER_CREATE_H_
#define _GVIZ_APP_LAYER_CREATE_H_

#include "app/gvizLayerCreatePanel.h"
#include "core/gvizScene.h"

/*
 * Apply the user's choices from a gvizLayerCreatePanel to the scene:
 *   - For GVIZ_SLOT_NEW_EMPTY_SCENE: switches the scene mode if needed and
 *     adds the first component layer.
 *   - For GVIZ_SLOT_SPLIT_H/V: sets the layout split direction and adds a
 *     second component layer (split ratio kept at 0.5).
 *
 * Returns 0 on success, -1 on failure (allocation, file load, etc.).
 */
int gvizApplyLayerCreate(gvizScene *scene, const gvizLayerCreateParams *p);

/*
 * Lower-level helper: build a heap layer matching @p params and return it
 * via @p out. Caller adds it to the scene. Returns 0 on success.
 */
int gvizCreateLayerFromParams(gvizScene *scene,
                              const gvizLayerCreateParams *params,
                              gvizLayer **out);

#endif
