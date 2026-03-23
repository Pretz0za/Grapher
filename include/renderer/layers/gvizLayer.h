#include "core/object.h"
#include <stddef.h>

typedef struct gvizViewport {
  int x, y;
  int width, height;
} gvizViewport;

typedef struct gvizLayerVTable {
  DrawFunction *draw;
  UpdateFunction *update;
  EventHandler *onEvent;
  ReleaseFunction *release;
} gvizLayerVTable;

typedef struct gvizLayer {
  const gvizLayerVTable *vtable;
  gvizViewport viewport;
  size_t z;
} gvizLayer;

void gvizLayerInit(gvizLayer *layer, const gvizViewport viewport,
                   const gvizLayerVTable *vtable, size_t z);
