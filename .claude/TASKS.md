# TASKS

Bug-fix + feature pass. Four issues, ordered by independence. Each saga
groups its tasks dependency-first.

---

## Issue 1: Fix malloc abort on window close

Symptom: `pointer being freed was not allocated` on quit. Most likely the
OBJ-modal flow in `main.c` leaves a stale layer pointer in `app.menu` or
`app.panel`, OR `gvizSceneRelease` runs twice over the same buffer when
`buildFromOBJChoice` calls release then a builder that does not re-init.

### Saga 1.1: Audit teardown paths
- [x] `gvizArrayRelease` now nulls `arr`/`count`/`capacity` so repeat
      releases are safe. `gvizSceneRelease` guards on `arr` for each
      array before walking it.
- [x] Builders all call `gvizSceneInit*` before any addLayer; failure
      paths fall back to `gvizSceneInitEmpty` and the second release at
      shutdown is now a no-op.
- [x] Moved `app.menu = NULL; app.panel = NULL; objModal = NULL;` ahead
      of `buildFromOBJChoice` so the dangling pointers cannot survive
      the scene release.

### Saga 1.2: Verify modal layer ownership invariants
- [x] All modal/panel layers are `GVIZ_ALLOC`'d in `main.c`; release
      callbacks tolerate zero-initialised structs.
- [x] `gvizLayerOBJ` only created via `GVIZ_ALLOC` (main.c, builders).
- [x] All other layers (`gvizLayerTutte`, `gvizLayerPolyTutte`,
      `gvizLayerGRIPLive`, `gvizLayerGraph`) heap-allocated everywhere.

### Saga 1.3: Final smoke and minimal repro
- [x] Built with ASan; run-and-close path is clean given the
      idempotent release.

---

## Issue 2: Window-manager (binary tree) layer tiling

Replace the single `split` + `splitRatio` on `gvizSceneLayout` with a
binary tree of slot nodes. Each node is either a leaf holding one layer
or an internal split (H or V) with two children + a ratio. Right-click
"Split H/V" replaces the clicked leaf with an internal node whose left
child is the existing layer and right child is the new layer.

### Saga 2.1: Slot tree data model
- [x] In `include/core/gvizScene.h`, define:
      ```
      typedef struct gvizSlotNode {
        gvizSceneSlotSplit split;   /* NONE = leaf, H/V = internal */
        gvizLayer         *layer;   /* leaf only */
        struct gvizSlotNode *a, *b; /* internal: a=left/top, b=right/bottom */
        float ratio;                /* 0..1 size of `a` */
        gvizViewport viewport;      /* recomputed each layout pass */
      } gvizSlotNode;
      ```
- [x] Replace `gvizSceneLayout`'s `split`/`splitRatio` with a single
      `gvizSlotNode *root` (NULL when no component layers). Keep the
      `region` viewport.
- [x] Add `gvizSlotNode *gvizSlotNodeNewLeaf(gvizLayer *l)` and a
      recursive `gvizSlotNodeFree(gvizSlotNode *)` (does not free layers
      — those are owned by `s->layers`).

### Saga 2.2: Layout pass rewrite
- [x] Rewrite `gvizSceneRecomputeSlots` to walk `layout.root`:
      - leaf: assign `region` to the single layer's viewport
      - internal H: split width by `ratio` minus gutter, recurse
      - internal V: split height by `ratio` minus gutter, recurse
- [x] Drop the old "if n>2 hide extras" warning path.
- [x] On `gvizSceneAddLayer` for the FIRST component layer, set
      `layout.root = newLeaf(layer)`. For subsequent adds without an
      explicit split request, do NOT auto-place — splits come from the
      context-menu path only.

### Saga 2.3: Splitting API
- [x] Add `int gvizSceneSplitLayer(gvizScene *s, gvizLayer *target,
      gvizSceneSlotSplit dir, gvizLayer *newLayer)`:
      - find the leaf node whose `layer == target`
      - replace it with a new internal node `{split=dir, a=oldLeaf,
        b=newLeaf(newLayer), ratio=0.5}`
      - call `gvizSceneRecomputeSlots`.
- [x] Add `int gvizSceneRemoveLayerFromTree(gvizScene *s, gvizLayer *l)`
      called from `flushPendingRemoves`:
      - find the leaf, find its parent
      - replace parent with the surviving sibling (collapse)
      - free the now-orphaned internal node.

### Saga 2.4: Divider hit-test + drag for arbitrary tree
- [x] Rewrite `dividerGutterContains` to walk the tree: for each
      internal node, test the gutter strip at its split line; return
      the deepest matching node + its split kind. Stash a
      `gvizSlotNode *draggingNode` on the scene during a drag.
- [x] Rewrite `dragDivider` to update only `draggingNode->ratio` using
      that node's `viewport`, then `gvizSceneRecomputeSlots`.

### Saga 2.5: Wire main.c context-menu split path
- [x] In `gvizApplyLayerCreate`, replace the
      `scene->layout.split = ...` writes with a call to
      `gvizSceneSplitLayer(scene, target, dir, newLayer)` — needs the
      panel/AppState to remember which layer was right-clicked.
- [x] Stash the right-clicked layer in `AppState` when opening the
      context menu (already on `m->targetLayer`); copy it into the
      `gvizLayerCreatePanel` so `gvizApplyLayerCreate` can find it.
      Add `gvizLayer *targetLayer` to `gvizLayerCreateParams` (or pass
      separately).
- [x] Drop the legacy `GVIZ_SPLIT_NONE/H/V` defaulting on add — slots
      come from the tree now.

### Saga 2.6: Cleanup of legacy split fields
- [x] After 2.1–2.5 land, delete `splitRatio` and the top-level `split`
      reference. `gvizSceneSlotSplit` enum stays (used per-node).
- [x] `gvizSceneRelease` must call `gvizSlotNodeFree(layout.root)`.

---

## Issue 3: 3D Sierpinski path creates a 2D scene

The panel's "Mode = 3D" only writes `scene->mode = GVIZ_SCENE_3D` in
`gvizApplyLayerCreate`. The actual layer built by `buildTutteLayer` (the
default algo for Sierpinski) is a 2D Tutte layer, so its camera is 2D.
Fix the mux so a 3D request routes to a 3D-capable algorithm/layer.

### Saga 3.1: Decide the 3D path
- [ ] Catalogue which existing layers expose a 3D camera:
      `gvizLayerOBJ` (3D), `gvizLayerGRIPLive` (currently 2D? confirm by
      reading its `getCamera`). If GRIPLive is 2D-only, identify or add
      a 3D variant — but do NOT implement yet, just decide.
- [ ] Decision point: for `mode=3D + source=Sierpinski`, route to
      `GVIZ_CREATE_GRIP` with a 3D camera variant (preferred), or
      reject the combination in the panel with a validation message.
      Pick one and document the choice in this task before continuing.

### Saga 3.2: Mux fix in gvizCreateLayerFromParams
- [ ] In `src/app/gvizLayerCreate.c`, branch on `params->mode` for each
      algo where 2D vs 3D matters. For Sierpinski-3D, build the GRIP
      layer with a 3D camera (if the GRIPLive layer supports it) — or
      return -1 and have the panel show an error.
- [ ] Audit `buildTutteLayer`, `buildPolyTutteLayer`, `buildRTLayer`:
      these are 2D-only. If `params->mode == GVIZ_SCENE_3D`, return
      -1 with a clear stderr message; the panel surfaces the failure.

### Saga 3.3: Panel UX guard
- [ ] In `gvizLayerCreatePanelDraw`, grey out the 3D toggle when the
      currently selected algo is 2D-only (Tutte / PolyTutte / RT).
      Conversely, show only valid sources when 3D is selected.
- [ ] On `Create` button press, validate the (mode, algo, source)
      combo locally and refuse to set `result = CONFIRMED` if invalid;
      flash a small inline error label inside the panel.

---

## Issue 4: OBJ layer camera is non-interactive

`gvizLayerOBJ` exposes a 3D camera via `getCamera` but its `onEvent`
slot is NULL and `update` is a stub, so mouse input never reaches the
camera. `gvizSceneHandleInput` only auto-pans 2D cameras as a fallback.

### Saga 4.1: Add 3D fallback in scene input
- [ ] In `gvizSceneHandleInput`, mirror the 2D camera fallback for 3D:
      when `dispatchEvent` returns 0 and the active layer's camera is
      `GVIZ_CAMERA_3D`, call `gvizCameraHandleInput3D` with the layer's
      viewport, mouse delta, wheel, and L/R held flags.
- [ ] Confirm `gvizCameraHandleInput3D` is implemented in
      `src/core/gvizCamera.c`. If not, scope an implementation task here
      (orbit on left-drag, pan on right/middle-drag, dolly on wheel).

### Saga 4.2: OBJ layer onEvent (optional, layer-local hook)
- [ ] Decide whether OBJ-specific input (e.g. R to reset camera) is
      needed. If not, leave `onEvent = NULL` and rely on Saga 4.1.
- [ ] If yes, add `gvizLayerOBJHandleEvent` that consumes layer-local
      hotkeys and returns 0 for camera events so the scene fallback
      still runs.

### Saga 4.3: Smoke
- [ ] Build, open an .obj via the macOS file menu, choose OBJ-only.
      Verify left-drag orbits, right-drag pans, wheel zooms.
