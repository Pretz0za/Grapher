# TASKS

GUI revamp Epic 2: empty-scene startup, active-layer routing, right-click
context menus, scene-camera removal, main-menu removal, wider margins.
Sagas are ordered by dependency; tasks within each Saga are top-to-bottom.

---

## Epic A: Strip the main menu and scene-owned camera

Goal: Reach a clean baseline where the app starts straight into the main
interface and the scene no longer carries a camera at all (every layer
already owns one).

### Saga A.1: Remove the main menu layer
- [x] In `src/app/main.c`, drop `installMainMenu`, the `menu` local, the
      whole `if (menu && menu->requestedAction != ...)` block, and the
      `#include "renderer/layers/gvizLayerMainMenu.h"`.
- [x] Boot path: replace `gvizBuildBlankScene(&scene)` + `installMainMenu`
      with a new `gvizSceneInitEmpty(&scene)` (no layers, see Saga B.1).
- [x] Delete `src/renderer/layers/gvizLayerMainMenu.c` and
      `include/renderer/layers/gvizLayerMainMenu.h`.
- [x] Remove the `gvizLayerMainMenu.c` entry from `CMakeLists.txt`.
- [x] Remove menu-only enums (`GVIZ_MENU_*`) from any remaining headers
      and any `gvizMainMenuAction` references.

### Saga A.2: Remove `defaultCamera` from `gvizScene`
- [x] Delete `defaultCamera` field from `gvizScene` in
      `include/core/gvizScene.h` and update the doc comment.
- [x] In `src/core/gvizScene.c`:
      - Drop the `gvizCameraMake2D/3D` calls in `gvizSceneInit2D/3D`.
      - In `cameraAt`, return `NULL` instead of falling back to
        `&s->defaultCamera`; callers must already null-check.
      - In `fillMouseWorld`, when no camera is found, write `wx=sx, wy=sy`
        (screen-space passthrough) so events still flow when there are
        zero layers.
      - In `gvizSceneDraw`, skip layers whose `getCamera` returns NULL
        (component layers must always have a camera now); screen-space
        layers' `draw` no longer receives `&s->defaultCamera` — pass NULL
        and update vtable doc.
      - In the resize handler, remove the `defaultCamera.c2d.offset`
        update.
- [x] Audit every `s->defaultCamera` / `scene->defaultCamera` reference
      across `src/` and `tests/` and convert or delete each.
- [x] Update screen-space layer `draw` signatures' doc comment in
      `gvizLayer.h` to note that `camera` may be NULL for screen-space.

---

## Epic B: Empty-scene startup state

Goal: App launches into a true empty scene that displays "No current
scene, create scene" centered in the active region.

### Saga B.1: Empty scene initializer
- [x] Add `int gvizSceneInitEmpty(gvizScene *s)` to `gvizScene.h/.c` —
      same as `gvizSceneInit2D` but does not pick a mode yet (default to
      `GVIZ_SCENE_2D` since mode is selected when the first layer is
      created via the layer-creation panel).
- [x] `gvizSceneRecomputeSlots` already handles 0 layers — verify and
      add an explicit early-return comment.

### Saga B.2: Empty-region placeholder text
- [x] In `gvizSceneDraw`, after the component-layer loop, if there are
      zero component layers draw centered text "No current scene, create
      scene" inside `s->layout.region` using raylib `DrawText` (screen
      coords, no camera).
- [x] Pick a muted color (e.g. `(Color){140,140,140,255}`) and size from
      `MeasureText` so it stays centered on resize.

### Saga B.3: main.c boot path
- [x] `main.c` calls `gvizSceneInitEmpty(&scene)` once, drops the
      `gvizBuildBlankScene` call. The OBJ-modal flow keeps working
      because it operates on whichever scene exists.

---

## Epic C: Wider L/R margins

Goal: More generous left/right margins around the active region, sized
for a video-editing-style layout.

### Saga C.1: Bump margin constants
- [x] In `include/core/gvizScene.h`, raise `GVIZ_SCENE_MARGIN_L` and
      `GVIZ_SCENE_MARGIN_R` from 12 to 48 (roughly 4x — judgment call;
      revisit if it looks too thick at narrow widths).
- [x] No other code change needed: `gvizSceneComputeRegion` already
      reads the constants and the resize path recomputes the region.

---

## Epic D: Active-layer concept + input routing

Goal: Exactly one component layer is "active" and consumes mouse/key
input; clicking inside another layer's viewport switches focus; the
active layer gets a visible border highlight.

### Saga D.1: Track active layer on the scene
- [x] Add `gvizLayer *activeLayer` to `gvizScene` (NULL when no layers).
- [x] In `gvizSceneAddLayer`, if `activeLayer` is NULL and the new layer
      is a component layer, set it active.
- [x] In `flushPendingRemoves`, if the removed layer was active, pick
      the topmost remaining component layer (or NULL).
- [x] Add `void gvizSceneSetActiveLayer(gvizScene *s, gvizLayer *l)` to
      the public API.

### Saga D.2: Route input through the active layer
- [x] Replace `dispatchEvent`'s top-down z walk for mouse-down/up/move,
      wheel, key, and text events with: deliver to `s->activeLayer`
      first; if it returns 0, fall through to screen-space layers (which
      keep z-order semantics for modals).
- [x] Mouse-down on a different component layer (found via
      `gvizSceneFindLayerAt`) flips `activeLayer` to that layer BEFORE
      dispatching the event so the new active layer receives the click.
- [x] Camera pan/wheel fallback in `gvizSceneHandleInput` (the
      non-event 2D-camera handling near the bottom) should target
      `s->activeLayer`'s camera, not whatever's under the cursor.
- [x] Divider drag and screen-space layer (modal) handling stay
      unchanged — they pre-empt active routing.

### Saga D.3: Active-layer border highlight
- [x] In `gvizSceneDraw`, after a component layer finishes drawing,
      if it equals `s->activeLayer` draw a 2-px rectangle border around
      its viewport in screen space (e.g. `(Color){70,140,255,200}`).
      Use `DrawRectangleLinesEx` outside `BeginScissorMode`.

---

## Epic E: Right-click context menus + layer-creation panel

Goal: Right-click on empty area or on a layer brings up a context menu;
"Create new layer" / "Split H" / "Split V" all funnel into the same
layer-creation panel.

### Saga E.1: Layer-creation panel data model
- [ ] Add `include/app/gvizLayerCreatePanel.h` defining a screen-space
      modal layer with fields: target slot info (split direction +
      parent layer to split, or "new in empty scene"), selected mode
      (`GVIZ_SCENE_2D/3D`), selected algorithm enum
      (`CREATE_TUTTE|GRIP|POLYTUTTE|RT|EMPTY`), selected source enum
      (`SRC_DEMO_SIERPINSKI|SRC_DEMO_OCTAHEDRON|SRC_DEMO_RANDOM_TREE|`
      `SRC_FROM_FILE`), filepath buffer, and a `result` enum
      (`PENDING|CONFIRMED|CANCELLED`).
- [ ] Implement `gvizLayerCreatePanelInit/Draw/HandleEvent/Release`
      using raygui (mirror `gvizOBJLoadModal.c` style). Z high enough
      to overlay everything; `GVIZ_LAYER_SCREEN_SPACE |
      GVIZ_LAYER_CAPTURES_INPUT`.

### Saga E.2: Context-menu layer
- [ ] Add `include/app/gvizContextMenu.h` — a small screen-space layer
      drawn at a click location. Holds a list of `{label, actionId}`
      entries and a `result` (selected actionId or -1 = cancelled,
      0 = pending). Closes on outside click or Esc.
- [ ] Empty-area menu builds a single "Create new layer" entry.
- [ ] Layer-area menu builds two entries: "Split Horizontal", "Split
      Vertical".

### Saga E.3: Wire right-click in the scene
- [ ] In `gvizSceneHandleInput`, on `MOUSE_BUTTON_RIGHT` press inside
      `layout.region`:
      - If `gvizSceneFindLayerAt(s, sx, sy)` returns NULL → emit a
        scene-level callback `s->onEmptyAreaContextMenu(sx, sy)`.
      - Otherwise → emit `s->onLayerContextMenu(layer, sx, sy)`.
      Both callbacks are function pointers settable from `main.c`.
- [ ] Suppress scene's normal mouse-down dispatch when the right-click
      is consumed by a callback.

### Saga E.4: main.c orchestration
- [ ] In `main.c`, install the two callbacks: each pushes a
      `gvizContextMenu` layer and stashes the click info.
- [ ] Each frame, drain context-menu `result`: on "Create new layer" or
      "Split H/V" push a `gvizLayerCreatePanel` configured with the
      correct slot info.
- [ ] On panel `CONFIRMED`, call into a new helper
      `applyLayerCreate(scene, params)` (in main.c or a new
      `gvizLayerCreate.c`) that:
      - For empty-scene case: switches scene to 2D/3D as chosen, builds
        the requested graph + layer (reusing existing builders or new
        small helpers), adds it to the scene.
      - For split case: sets `scene->layout.split` to H or V (split
        ratio stays at 0.5), then constructs the second layer via the
        same builder helpers and adds it.
- [ ] On panel `CANCELLED`: discard, do nothing.

### Saga E.5: Algorithm/source builder mux
- [ ] Add `int gvizCreateLayerFromParams(gvizScene *, const
      gvizLayerCreateParams *, gvizLayer **out)` that switches on
      algorithm + source and returns a heap-allocated layer ready to
      add. Reuse internals from `gvizSceneBuilders.c` (extract small
      helpers if needed; do NOT refactor the existing builders).
- [ ] Sources: built-in demos (sierpinski-3, octahedron, random-tree-5)
      and "load from file" (tree JSON for RT, .obj for PolyTutte/OBJ).
      File path comes from the panel's text field; OBJ files reuse the
      existing macOS file menu only if the user types nothing — for the
      first cut, accept a typed path only.

---

## Epic F: Smoke verification

### Saga F.1: Build + manual verification
- [ ] `cmake .. && make` clean.
- [ ] App launches into empty scene; placeholder text visible.
- [ ] Right-click empty area → "Create new layer" → choose 2D + Tutte +
      octahedron → layer appears, active border drawn.
- [ ] Right-click that layer → "Split Horizontal" → second layer
      created, divider drag still works, clicking either layer flips
      active border.
- [ ] OBJ open via macOS File menu still works (modal dispatches to
      builders unchanged).
