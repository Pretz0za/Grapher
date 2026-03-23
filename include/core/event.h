#ifndef _GVIZ_CORE_EVENT_H_
#define _GVIZ_CORE_EVENT_H_

typedef enum gvizEventType {
  GVIZ_EVENT_MOUSE_CLICK,
} gvizEventType;

typedef struct gvizEvent {
  gvizEventType type;
} gvizEvent;

#endif
