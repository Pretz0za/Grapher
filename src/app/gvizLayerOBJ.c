#include "app/gvizLayerOBJ.h"
#include "utils/gvizOBJLoader.h"
#include "raymath.h"
#include <math.h>
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

void gvizLayerOBJGetContentBounds(void *layerV, Vector3 *centroid, float *radius) {
    gvizLayerOBJ *self = (gvizLayerOBJ *)layerV;
    *centroid = self->position;
    *radius   = 1.0f;
    if (!self->hasModel) return;
    BoundingBox bb = GetModelBoundingBox(self->model);
    centroid->x = self->position.x + (bb.min.x + bb.max.x) * 0.5f * self->scale.x;
    centroid->y = self->position.y + (bb.min.y + bb.max.y) * 0.5f * self->scale.y;
    centroid->z = self->position.z + (bb.min.z + bb.max.z) * 0.5f * self->scale.z;
    float dx = (bb.max.x - bb.min.x) * self->scale.x;
    float dy = (bb.max.y - bb.min.y) * self->scale.y;
    float dz = (bb.max.z - bb.min.z) * self->scale.z;
    *radius = sqrtf(dx*dx + dy*dy + dz*dz) * 0.5f;
    if (*radius < 0.01f) *radius = 0.01f;
}

void gvizLayerOBJUpdate(void *layerV, float dt) {
    gvizLayerOBJ *self = (gvizLayerOBJ *)layerV;
    /* R held: rotate model about its centroid along camera up axis */
    if (IsKeyDown(KEY_R) && self->hasModel) {
        Vector3 up  = Vector3Normalize(self->camera.c3d.up);
        float   dot = fabsf(Vector3DotProduct(up, (Vector3){0,1,0}));
        /* Decompose up into a rotation around Y for simplicity when up≈Y,
           otherwise use a general rotation stored as rotationY offset. */
        (void)dot;
        self->rotationY += 90.0f * dt;
    }
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
