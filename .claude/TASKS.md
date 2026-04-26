# TASKS

## Epic 1: Fix OBJ loading

### Replace stdin prompt with macOS file dialog
- [x] In `src/app/main.c`, replace the `fgets(stdin)` block inside `GVIZ_MENU_LOAD_OBJ_TUTTE` with a call to a new helper `char *gvizOpenFileDialog(void)` (returns heap-allocated path or NULL). Implement that helper in `src/app/main.c` (static, no header needed) using `popen` with the AppleScript command: `osascript -e 'POSIX path of (choose file with prompt "Select OBJ mesh" of type {"obj", "OBJ"})'`. Trim the trailing newline from the result. On failure/cancel, return NULL and fall back to `gvizBuildBlankScene`.
- [x] For quick testing, also add a `#define GVIZ_OBJ_TEST_PATH` at the top of `main.c` defaulting to `"/Users/abdulazizalahmadi/Desktop/COMPSCI 163/Grapher/build/face.obj"`. If defined, skip the dialog and use this path directly (guarded by `#ifdef GVIZ_OBJ_TEST_PATH`). Leave the define commented out after verifying loading works.

### Add printf debug to OBJ loader
- [x] In `gvizLoadOBJAsGraph` (`src/utils/gvizOBJLoader.c`), add `fprintf(stderr, ...)` prints.

### Fix outer face selection
- [x] Change `gvizLoadOBJAsGraph` signature to also return the full first face length.
- [x] In `gvizBuildPolyTutteFromOBJScene` pass `outerFaceLen` to `gvizLayerPolyTutteInit`.

---

## Epic 2: Fix face-scan highlighting

### Add debug prints to understand face count
- [ ] In `gvizLayerPolyTutteHandleEvent` (`src/app/gvizLayerPolyTutte.c`), after the face enumeration loop, add: `fprintf(stderr, "[PolyTutte] faces enumerated: %zu\n", self->faces.count);`
- [ ] In `gvizLayerPolyTutteUpdate` SCANNING branch, at the start of each timer tick (after timer fires), add: `fprintf(stderr, "[PolyTutte] scanning face %zu / %zu\n", self->scanFaceIdx, self->faces.count);`

### Fix clear-after-display logic
- [ ] The current scan loop clears highlights at the START of each timer tick (i.e. immediately overwrites the previous face before showing the new one). Restructure so each face is shown for a full 0.2s dwell THEN cleared before the next. New logic:
  ```
  scanTimer += dt;
  if (scanTimer < 0.2f) return;   // show current face for 0.2s
  scanTimer = 0.0f;
  pt_clearHighlights(self);        // clear AFTER the dwell
  self->scanFaceIdx++;
  if (self->scanFaceIdx > self->faces.count) {
      self->phase = GVIZ_POLY_TUTTE_FINAL;
      return;
  }
  // set highlights for the new current face (scanFaceIdx - 1 already processed; now show scanFaceIdx)
  ```
  Actually the cleanest restructure: on entering SCANNING (scanFaceIdx=0), immediately set face 0 highlights (no timer wait). Then each timer tick: compute area for current face, clear highlights, increment scanFaceIdx, set highlights for next face (or transition to FINAL if done). This way the CLEAR of the just-shown face happens explicitly before setting the next face.

  Concretely, split into two sub-steps per tick:
  1. When entering SCANNING (or after transitioning), immediately highlight face `scanFaceIdx` — call this from HandleEvent right after setting phase=SCANNING so face 0 is visible from frame 1.
  2. Each 0.2s timer tick in Update: compute area for current face `scanFaceIdx`, call `pt_clearHighlights`, increment `scanFaceIdx`, if done → FINAL, else highlight new face and reset timer.

- [ ] Extract a helper `pt_highlightFace(gvizLayerPolyTutte *self, size_t faceIdx)` that clears all highlights, then sets vertex and edge highlights for `self->faces[faceIdx]`. Call it from HandleEvent (for face 0) and from Update after each increment.

### Fix vertex disc highlight (discs always draw with uniform color)
- [ ] In `gvizGraphVBODraw` (`src/renderer/layers/gvizGraphVBO.c`), the disc pass calls `gvizVertexDiscVBODraw(&vbo->discs, discColor, vbo->discFill)` with a hardcoded black color for ALL discs. Vertex highlights therefore never turn red for the disc circles.
- [ ] Read `include/renderer/layers/gvizVertexDiscVBO.h` and `src/renderer/layers/gvizVertexDiscVBO.c` to understand the disc shader. Then add a `float *highlightMask` array (one float per vertex, 1.0 = highlighted, 0.0 = normal) to `gvizGraphVBO` — allocated in `gvizGraphVBORebuild`, freed in `gvizGraphVBORelease`.
- [ ] Add `gvizGraphVBOSetDiscHighlights(gvizGraphVBO *vbo, const float *mask, size_t n)` that stores the mask. In `gvizGraphVBODraw`, draw discs in TWO passes: first `glDrawArraysInstanced` (or equivalent) for non-highlighted vertices with black color, then for highlighted vertices with red `{1,0,0,1}` — OR, simpler: if the disc shader has a per-instance color attribute, upload a per-instance color array. Pick the simpler approach after reading the disc VBO code.
- [ ] Wire `pt_writeColors` (or a new `pt_writeDiscColors`) in `gvizLayerPolyTutte.c` to update the disc highlight mask whenever `highlightDirty` is set: walk the vertex highlight bitarray and fill a float mask array, then call `gvizGraphVBOSetDiscHighlights`.

### Build and verify
- [ ] Build with `cd build && cmake .. && make` and fix any errors.
