#ifndef _GVIZ_APP_LAYER_OBJ_H_
#define _GVIZ_APP_LAYER_OBJ_H_

#include "core/gvizCamera.h"
#include "raylib.h"
#include "renderer/layers/gvizLayer.h"

/*
 * A 3D layer that renders a Wavefront .obj file as a textured raylib Model.
 * The layer always uses a 3D perspective camera; mouse drag/wheel pan and
 * zoom around the model's centre.
 */
typedef struct gvizLayerOBJ {
    gvizLayer  layer;     /* MUST be first */
    gvizCamera camera;
    Model      model;
    Vector3    position;
    Vector3    scale;
    float      rotationY;
    int        hasModel;  /* 0 if model is unset (load failed) */
} gvizLayerOBJ;

/*
 * Initialize @p layer by loading the given .obj file as a Model. Returns
 * 0 on success, -1 if loading failed (raylib reported zero meshes). The
 * layer takes ownership of the loaded model and unloads it on release.
 */
int gvizLayerOBJInit(gvizLayerOBJ *layer, const char *objPath, size_t z);

void gvizLayerOBJDraw(void *layer, const gvizCamera *camera);
void gvizLayerOBJUpdate(void *layer, float dt);
void gvizLayerOBJRelease(void *layer);
struct gvizCamera *gvizLayerOBJGetCamera(void *layer);

static const gvizLayerVTable GVIZ_LAYER_OBJ_VTABLE = {
    .draw    = gvizLayerOBJDraw,
    .update  = gvizLayerOBJUpdate,
    .release = gvizLayerOBJRelease,
    .onEvent = NULL,
    .hitTest = NULL,
    .getCamera = gvizLayerOBJGetCamera,
};

#endif
