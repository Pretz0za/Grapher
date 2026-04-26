# TASKS

## Epic 1: Fix permanent highlight residue

`pt_clearHighlights` memsets based on `vertexHighlightBits`/`edgeHighlightBits`, but after
`pt_rebuildIndex` those counts can change while the old GPU color buffer still holds stale data.
The fix: always track allocated unit counts separately so memset covers the full allocation,
and always upload the cleared state to the GPU immediately.

- [x] Add `size_t vertexHighlightUnits` and `size_t edgeHighlightUnits` fields to
  `gvizLayerPolyTutte` in `include/app/gvizLayerPolyTutte.h`. These store the actual
  number of `GVIZ_BIT_UNIT` words allocated (independent of the logical bit count).
- [x] In `pt_rebuildIndex` (`src/app/gvizLayerPolyTutte.c`), after each realloc set the
  corresponding `*Units` field to the allocated unit count before the memset, so the
  memset always covers the full allocation.
- [x] Rewrite `pt_clearHighlights` to memset using the `*Units` fields (not recomputing
  from bit counts): `memset(self->vertexHighlight, 0, self->vertexHighlightUnits * sizeof(GVIZ_BIT_UNIT))`.
  After zeroing both arrays, call `pt_writeColors` directly (don't just set `highlightDirty`)
  so the GPU color buffer is updated in the same call — this eliminates the one-frame lag
  where stale GPU data is visible. Keep `highlightDirty = 0` after the upload.
- [x] In `gvizLayerPolyTutteRelease`, zero out the new fields (no alloc change needed, just init).

---

## Epic 2: Right-click to highlight face under cursor

When the user right-clicks, find which enumerated face contains the world-space click
point and highlight it. If no face contains the point (clicked outside the embedding),
highlight the "outer" face (the largest-area face, which wraps the unbounded region).
Store the selected face index for Enter to act on.

- [ ] Add `size_t selectedFaceIdx` (SIZE_MAX = none) to `gvizLayerPolyTutte` in the header.
  Initialize to `SIZE_MAX` in `gvizLayerPolyTutteInit`.
- [ ] Add a static helper `pt_pointInFace(const size_t *verts, size_t n, double px, double py, gvizEmbeddedGraph *eg) -> int` in `src/app/gvizLayerPolyTutte.c` using the ray-casting point-in-polygon test: cast a horizontal ray rightward from `(px, py)` and count crossings of each face edge; return 1 if odd.
- [ ] Add a static helper `pt_findOuterFace(gvizLayerPolyTutte *self) -> size_t` that returns the index of the face with the largest area (using `polygonArea2D`). This is the outer/unbounded face in the Tutte drawing. Returns `SIZE_MAX` if `faces.count == 0`.
- [ ] In `gvizLayerPolyTutteHandleEvent`, add a `GVIZ_EVENT_MOUSE_DOWN` + `GVIZ_MOUSE_RIGHT` case:
  1. If `self->faces.count == 0`: lazily enumerate faces (same rotation-system + face-enum code as SPACE/R; set `gpuDirty = 2`). If still 0 after enumeration, return 1.
  2. World coordinates are in `event->mouse.wx`, `event->mouse.wy`.
  3. Iterate all faces; for each call `pt_pointInFace`. Take the first face that contains the point.
  4. If no face contains the point: call `pt_findOuterFace` and use that index.
  5. Set `self->selectedFaceIdx` to the found index.
  6. Call `pt_clearHighlights(self)` then `pt_highlightFace(self, self->selectedFaceIdx)`.
  7. Return 1.
- [ ] Update the keybinding HUD string in `gvizLayerPolyTutteDraw` for the INITIAL phase to include: `"right-click  select face   ENTER  fix & re-embed"`.

---

## Epic 3: Enter fixes selected face and re-embeds

- [ ] In `gvizLayerPolyTutteHandleEvent`, add a `KEY_ENTER` case:
  1. If `self->selectedFaceIdx == SIZE_MAX` or `self->faces.count == 0`, return 0 (nothing selected).
  2. Get the face verts from `self->faces[self->selectedFaceIdx]`. If `n < 3`, return 1.
  3. Release and re-init TutteSolve: `gvizTutteSolveEmbeddingRelease`, `gvizTutteSolveEmbeddingInit(&self->tutte, &self->graph, 2, 0)`, `gvizTutteSolveFixConvexPolygon(&self->tutte, bv, bn, self->boundaryRadius)`, `gvizTutteSolveEmbeddingStep(&self->tutte, 0)`. Set `self->hasTutte = 1`.
  4. `pt_rebuildIndex(self)`, `pt_clearHighlights(self)`.
  5. Set `gpuDirty = 2`, `phase = GVIZ_POLY_TUTTE_INITIAL`, `selectedFaceIdx = SIZE_MAX`, `scanFaceIdx = 0`, `bestFaceArea = -DBL_MAX`, `scanTimer = 0`.
  6. Return 1.

## Build
- [ ] `cd build && cmake .. && make`, fix any errors.
