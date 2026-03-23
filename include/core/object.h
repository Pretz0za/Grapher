#ifndef _GVIZ_CORE_OBJECT_H_
#define _GVIZ_CORE_OBJECT_H_

#include "core/event.h"
#include "raylib.h"

typedef void(DrawFunction)(void *self, const Camera2D *camera);
typedef void(UpdateFunction)(void *self, float dt);
typedef void(ReleaseFunction)(void *self);
typedef int(EventHandler)(void *self, const gvizEvent *event);

#endif
