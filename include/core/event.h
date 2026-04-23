#ifndef _GVIZ_CORE_EVENT_H_
#define _GVIZ_CORE_EVENT_H_

#include <stddef.h>

/*
 * Event type tag. Handlers dispatch on `event->type` and read the matching
 * union arm.
 */
typedef enum gvizEventType {
  GVIZ_EVENT_NONE = 0,
  GVIZ_EVENT_MOUSE_MOVE,
  GVIZ_EVENT_MOUSE_DOWN,
  GVIZ_EVENT_MOUSE_UP,
  GVIZ_EVENT_MOUSE_WHEEL,
  GVIZ_EVENT_KEY_DOWN,
  GVIZ_EVENT_KEY_UP,
  GVIZ_EVENT_TEXT_INPUT,
  GVIZ_EVENT_RESIZE,
  /* legacy alias — prefer GVIZ_EVENT_MOUSE_DOWN */
  GVIZ_EVENT_MOUSE_CLICK = GVIZ_EVENT_MOUSE_DOWN,
} gvizEventType;

typedef enum gvizMouseButton {
  GVIZ_MOUSE_LEFT = 0,
  GVIZ_MOUSE_RIGHT = 1,
  GVIZ_MOUSE_MIDDLE = 2,
} gvizMouseButton;

/* Bitmask of modifier keys held during the event. */
typedef enum gvizModFlags {
  GVIZ_MOD_NONE = 0,
  GVIZ_MOD_SHIFT = 1 << 0,
  GVIZ_MOD_CTRL = 1 << 1,
  GVIZ_MOD_ALT = 1 << 2,
  GVIZ_MOD_SUPER = 1 << 3, /* Cmd on macOS */
} gvizModFlags;

typedef struct gvizMouseEvent {
  float sx, sy;      /* screen-space px */
  float wx, wy;      /* world-space (resolved by scene before dispatch) */
  int button;        /* gvizMouseButton, -1 if not applicable (move) */
  unsigned int mods; /* bitmask of gvizModFlags */
} gvizMouseEvent;

typedef struct gvizWheelEvent {
  float sx, sy;
  float wx, wy;
  float dx, dy;
  unsigned int mods;
} gvizWheelEvent;

typedef struct gvizKeyEvent {
  int key; /* raylib KEY_* scancode passthrough */
  unsigned int mods;
} gvizKeyEvent;

typedef struct gvizTextEvent {
  int codepoint;
} gvizTextEvent;

typedef struct gvizResizeEvent {
  int width, height;
} gvizResizeEvent;

typedef struct gvizEvent {
  gvizEventType type;
  union {
    gvizMouseEvent mouse;
    gvizWheelEvent wheel;
    gvizKeyEvent key;
    gvizTextEvent text;
    gvizResizeEvent resize;
  };
} gvizEvent;

#endif
