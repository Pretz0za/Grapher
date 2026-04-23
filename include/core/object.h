#ifndef _GVIZ_CORE_OBJECT_H_
#define _GVIZ_CORE_OBJECT_H_

#include "core/event.h"
#include "core/gvizCamera.h"

typedef void(DrawFunction)(void *self, const gvizCamera *camera);
typedef void(UpdateFunction)(void *self, float dt);
typedef void(ReleaseFunction)(void *self);

/*
 * Event handler return convention:
 *   1 -> event accepted, propagation stops (scene does not pass the event
 *        to lower layers).
 *   0 -> event not handled, scene continues dispatching to the next layer.
 */
typedef int(EventHandler)(void *self, const gvizEvent *event);

#endif
