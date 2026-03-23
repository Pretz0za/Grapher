#include "renderer/layers/gvizLayer.h"

void gvizLayerInit(gvizLayer *layer, const gvizViewport viewport,
                   const gvizLayerVTable *vtable, size_t z) {
  layer->vtable = vtable;
  layer->viewport = viewport;
  layer->z = z;
}
