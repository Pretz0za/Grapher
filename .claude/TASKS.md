# TASKS

## Epic 1: Change face-scan maximizing criterion to vertex count

SPACE currently picks the face with the largest area (`bestFaceArea` / `polygonArea2D`).
Change it to pick the face with the most vertices.

- [x] In `include/app/gvizLayerPolyTutte.h`, rename `double bestFaceArea` → `size_t bestFaceVertCount`. Update the comment if any.
- [x] In `src/app/gvizLayerPolyTutte.c`:
  - In `gvizLayerPolyTutteInit`: initialize `bestFaceVertCount = 0` instead of `-DBL_MAX`.
  - In the SCANNING branch of `gvizLayerPolyTutteUpdate`: replace the `polygonArea2D` call with a direct comparison of `n` (the face vertex count): `if (n > self->bestFaceVertCount) { self->bestFaceVertCount = n; self->bestFaceIdx = self->scanFaceIdx; }`.
  - In every reset site that sets `bestFaceArea = -DBL_MAX` (KEY_ENTER, KEY_R, KEY_SPACE handlers), change to `self->bestFaceVertCount = 0`.

## Epic 2: Fix right-click world-coordinate conversion

The event's pre-filled `wx/wy` are resolved by the scene's camera at input time, which may
not match the layer's draw-time transform. Fix: cache the camera pointer in the layer and
redo the screen→world conversion in the event handler using `gvizCameraScreenToWorld2D`.
Also add fprintf debug lines so the coordinates are visible.

- [ ] Add `const gvizCamera *lastCamera` to `gvizLayerPolyTutte` in `include/app/gvizLayerPolyTutte.h`. Initialize to NULL in `gvizLayerPolyTutteInit`. Include `"core/gvizCamera.h"` in the header if not already present.
- [ ] In `gvizLayerPolyTutteDraw` (`src/app/gvizLayerPolyTutte.c`), at the top of the function assign `self->lastCamera = camera`.
- [ ] In the `GVIZ_EVENT_MOUSE_DOWN` + `GVIZ_MOUSE_RIGHT` handler:
  - Compute world coordinates manually: if `self->lastCamera != NULL`, call `Vector2 w = gvizCameraScreenToWorld2D(self->lastCamera, (Vector2){event->mouse.sx, event->mouse.sy})` and use `w.x / w.y` as `px / py`; otherwise fall back to `event->mouse.wx / wy`.
  - Add `fprintf(stderr, "[PolyTutte] right-click screen=(%.1f,%.1f) world=(%.2f,%.2f) faces=%zu\n", event->mouse.sx, event->mouse.sy, px, py, self->faces.count);` immediately after computing `px/py`.
  - After the face search loop, add `fprintf(stderr, "[PolyTutte] hit face=%zu (SIZE_MAX=%d)\n", hit, hit == SIZE_MAX);`.
- [ ] In `pt_pointInFace`, add a one-shot debug print for the first invocation per right-click: pass an extra `int faceIdx` parameter and print face vertex positions for face 0 only (guard with `if (faceIdx == 0)` so it doesn't flood). Actually simpler: just add a `fprintf` at the top of `pt_pointInFace` that prints `n`, `px`, `py`, and the first vertex position — gate it with `if (n > 0)` so it prints once per face per click. This is debug-only; the adventurer can remove the gate after verifying.

## Build
- [ ] `cd build && cmake .. && make`, fix any errors.
