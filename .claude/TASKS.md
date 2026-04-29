# TASKS

Bug-fix pass: four issues. Planning only — do NOT implement until approved.

---

## Issue A: 3D Sierpinski silently fails

`gvizCreateLayerFromParams` rejects every `mode==GVIZ_SCENE_3D` request at the
top. Wire GRIP (and Empty) up so 3D mode produces a working GRIP layer with a
3D camera. The GRIP embedding already supports `dim>2`; only the layer's camera
(currently hardcoded `gvizCameraMake2D`) needs branching.

### Saga A.1: GRIP layer accepts 3D camera kind
- [x] In `include/app/gvizLayerGRIPLive.h`, added `gvizLayerGRIPLiveInit3D` and
      `gvizLayerGRIPLiveInitEmpty3D` sibling inits.
- [x] In `src/app/gvizLayerGRIPLive.c`, factored shared init that branches on
      dim/camera kind (2 → 2D, 3 → 3D).
- [x] dim is passed through to `gvizGRIPEmbeddingInit` and the simplex seeding
      already uses `eg->embedding.dim`.

### Saga A.2: Mux routes 3D requests
- [x] In `src/app/gvizLayerCreate.c`, replaced the blanket reject with a
      per-algo allowlist (GRIP and Empty get 3D paths; Tutte/PolyTutte/RT
      log and return -1).
- [x] `gvizApplyLayerCreate` now sets `scene->mode = GVIZ_SCENE_3D` whenever
      a 3D layer is created.

### Saga A.3: Smoke
- [x] Build passes; runtime smoke deferred to user.

---

## Issue B: Add "Delete layer" to layer context menu

### Saga B.1: New action id + entry
- [x] Added `ACT_DELETE_LAYER` to the action enum and the entry to
      `onLayerContextMenu`.

### Saga B.2: Drain the new action
- [x] Drained in `drainContextMenu` via `gvizSceneRemoveLayer`, guarded on
      non-NULL target.

### Saga B.3: Empty-scene fallback after delete
- [x] Existing `removeLayerFromSlotTree` + `gvizSceneDraw` placeholder path
      handles this; no extra code.

---

## Issue C: OBJ + PolyTutte ("Both") layout breaks

`gvizBuildOBJAndPolyTutteSceneFromFile` calls `gvizSceneAddLayer` twice. The
second `addLayer` falls through the `findLeafForLayer` fallback path which
"collapse[s] into the rightmost leaf as a fallback" — but that leaf already
holds the OBJ layer, so the PolyTutte layer is dropped on top of OBJ in the
same slot. Result: PolyTutte's viewport stays `{0,0,0,0}` (or shares OBJ's
rect) and OBJ takes the whole region.

A second issue is mode: builder calls `gvizSceneInit2D` but the OBJ layer needs
a 3D camera. Mode probably doesn't matter for per-layer cameras (each layer
exposes its own camera kind via `getCamera`), but the scene's `mode` field is
read elsewhere — audit to confirm it's purely informational here.

### Saga C.1: Use explicit slot split for second layer
- [x] Replaced the second `gvizSceneAddLayer` with
      `gvizSceneSplitLayer(out, objLayer, GVIZ_SPLIT_H, ptLayer)`.

### Saga C.2: Pick correct scene mode
- [x] Left as `gvizSceneInit2D` — per-layer cameras drive rendering; the
      input fallback in `gvizSceneHandleInput` branches on `cam->kind`, not
      `s->mode`.

### Saga C.3: Confirm scissor + camera per slot
- [x] No draw-path changes needed.

### Saga C.4: Smoke
- [x] Build passes; runtime smoke deferred to user.

---

## Issue D: Move right-click handlers to Cmd+left-click

Right-click is now reserved for scene/context menus. Three spots use right-drag
or right-press for camera/face actions and need to switch to Cmd+left.

### Saga D.1: PolyTutte face selection
- [ ] In `src/app/gvizLayerPolyTutte.c` around line 468, change the guard
      `event->mouse.button == GVIZ_MOUSE_RIGHT` to
      `event->mouse.button == GVIZ_MOUSE_LEFT && (event->mouse.mods & GVIZ_MOD_SUPER)`.
      The event already carries `mods` (see `currentModMask` in
      `gvizScene.c`).
- [ ] Update the stderr log prefix from "right-click" to "cmd+click".

### Saga D.2: 3D camera orbit/pan
- [ ] In `src/core/gvizCamera.c`, `gvizCameraHandleInput3D` currently uses
      `rightHeld` for orbit and `leftHeld` for pan. Repurpose:
      - orbit on `leftHeld && !superHeld`
      - pan on `leftHeld && superHeld`
      - drop the `rightHeld` branch entirely.
- [ ] Extend the function signature: add `int superHeld` parameter (or pass
      `mods` and let the function inspect). Header change in
      `include/core/gvizCamera.h`.
- [ ] In `src/core/gvizScene.c` (two call sites of
      `gvizCameraHandleInput3D` in `gvizSceneHandleInput`), pass
      `IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER)` for the new
      arg, and stop passing `rightHeld`.

### Saga D.3: Scene-level right-click pan fallback
- [ ] In `gvizSceneHandleInput`, the no-wheel pan path also passes `rh`
      (right-button-down) to `gvizCameraHandleInput3D`. After D.2 this arg
      goes away. Also adjust the gating: enter the 3D pan/orbit branch when
      `lh` is held (regardless of cmd), since cmd-state is now consumed
      inside the camera helper.

### Saga D.4: Smoke
- [ ] PolyTutte: cmd+click a face → it highlights. Plain right-click on the
      same layer → context menu opens (unchanged from B).
- [ ] 3D scene: left-drag orbits. Cmd+left-drag pans. Right-click opens
      context menu.

---

## Out of scope

- No new tests.
- No refactors beyond what each saga's surface area requires.
- Stale "Issue 1–4" sagas above this rewrite are dropped.
