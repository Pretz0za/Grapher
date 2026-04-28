#ifndef _GVIZ_APP_OBJ_LOAD_MODAL_H_
#define _GVIZ_APP_OBJ_LOAD_MODAL_H_

#include "renderer/layers/gvizLayer.h"

/*
 * Three-way OBJ-load modal. Asks the user how to render a freshly picked
 * .obj file: as a 3D mesh only, as a planar PolyTutte embedding only, or
 * both side-by-side. The owning code reads `result` after each frame and
 * acts on it once `result != GVIZ_OBJ_MODAL_PENDING`.
 */
typedef enum gvizOBJModalChoice {
  GVIZ_OBJ_MODAL_PENDING = 0,
  GVIZ_OBJ_MODAL_OBJ_ONLY,
  GVIZ_OBJ_MODAL_POLYTUTTE_ONLY,
  GVIZ_OBJ_MODAL_BOTH,
  GVIZ_OBJ_MODAL_CANCELLED,
} gvizOBJModalChoice;

typedef struct gvizOBJLoadModal {
  gvizLayer          layer;          /* MUST be first */
  gvizOBJModalChoice result;
  char               objPath[1024];
} gvizOBJLoadModal;

void gvizOBJLoadModalInit(gvizOBJLoadModal *m, const char *objPath, size_t z);
void gvizOBJLoadModalDraw(void *layer, const gvizCamera *camera);
void gvizOBJLoadModalRelease(void *layer);
int  gvizOBJLoadModalHandleEvent(void *layer, const gvizEvent *event);

static const gvizLayerVTable GVIZ_OBJ_LOAD_MODAL_VTABLE = {
    .draw    = gvizOBJLoadModalDraw,
    .update  = NULL,
    .release = gvizOBJLoadModalRelease,
    .onEvent = gvizOBJLoadModalHandleEvent,
    .hitTest = NULL,
    .getCamera = NULL,
};

#endif
