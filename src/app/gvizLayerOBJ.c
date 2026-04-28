#include "app/gvizLayerOBJ.h"
#include "utils/gvizOBJLoader.h"
#include <string.h>

int gvizLayerOBJInit(gvizLayerOBJ *layer, const char *objPath, size_t z) {
    memset(layer, 0, sizeof(*layer));
    gvizLayerInit((gvizLayer *)layer, (gvizViewport){0, 0, 0, 0},
                  &GVIZ_LAYER_OBJ_VTABLE, z);
    layer->camera = gvizCameraMake3D((Vector3){4, 3, 6}, (Vector3){0, 0, 0},
                                     (Vector3){0, 1, 0}, 45.0f,
                                     CAMERA_PERSPECTIVE);
    layer->layer.flags = GVIZ_LAYER_VISIBLE;
    layer->position = (Vector3){0, 0, 0};
    layer->scale = (Vector3){1, 1, 1};
    layer->rotationY = 0.0f;
    layer->hasModel = 0;

    if (gvizLoadOBJAsMesh(objPath, &layer->model) != 0)
        return -1;
    layer->hasModel = 1;
    return 0;
}

void gvizLayerOBJDraw(void *layerV, const gvizCamera *camera) {
    (void)camera;
    gvizLayerOBJ *self = (gvizLayerOBJ *)layerV;
    if (!self->hasModel) return;
    DrawModelEx(self->model, self->position, (Vector3){0, 1, 0},
                self->rotationY, self->scale, WHITE);
    DrawGrid(10, 1.0f);
}

void gvizLayerOBJUpdate(void *layerV, float dt) {
    (void)layerV;
    (void)dt;
}

void gvizLayerOBJRelease(void *layerV) {
    gvizLayerOBJ *self = (gvizLayerOBJ *)layerV;
    if (self->hasModel) {
        UnloadModel(self->model);
        self->hasModel = 0;
    }
}

struct gvizCamera *gvizLayerOBJGetCamera(void *layer) {
    return &((gvizLayerOBJ *)layer)->camera;
}
