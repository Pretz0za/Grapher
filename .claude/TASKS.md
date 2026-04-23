# Grapher GUI Plan

## Epic: GUI System

A Figma/Miro-style infinite whiteboard for graph visualization. A `gvizScene`
owns a world (2D or 3D), a camera, a z-ordered list of `gvizLayer` objects,
and an input/event dispatcher. Layers are polymorphic via vtable (already
established in `include/renderer/layers/gvizLayer.h`): graph layers, UI
layers (main menu, sidebar, HUD), and context menus. raygui is added under
`third-party/raygui/` for widgets. Entry point is a new `src/app/main.c`
executable target `grapher` in `CMakeLists.txt`.

### Dependencies & Build
- [x] Add new TU `src/renderer/raygui_impl.c` (inside graphvis lib) that defines `RAYGUI_IMPLEMENTATION`
- [ ] Add `grapher` executable target in `CMakeLists.txt` linking `graphvis`
- [ ] Add `src/app/main.c` source file to the `grapher` target

### Event System Expansion
- [ ] Expand `include/core/event.h` with event types: MOUSE_MOVE, MOUSE_DOWN/UP (with button + modifier), MOUSE_WHEEL, KEY_DOWN/UP, TEXT_INPUT, RESIZE
- [ ] Add world-space and screen-space coordinates to mouse events in `gvizEvent`
- [ ] Add `accepted` / `propagate` flag convention for `EventHandler` return value, documented in `include/core/object.h`

### Scene & Camera
- [ ] Create `include/core/gvizScene.h` defining `gvizScene` struct: layer list (`gvizArray` of `gvizLayer*`), camera union (Camera2D/Camera3D), mode flag (2D/3D), focused layer pointer, pending-destroy list
- [ ] Declare scene lifecycle in header: `gvizSceneInit2D`, `gvizSceneInit3D`, `gvizSceneRelease`, `gvizSceneAddLayer`, `gvizSceneRemoveLayer`, `gvizSceneBringToFront`
- [ ] Declare per-frame API: `gvizSceneHandleInput`, `gvizSceneUpdate(dt)`, `gvizSceneDraw`
- [ ] Implement `src/core/gvizScene.c` — polls raylib input, builds `gvizEvent`s, dispatches top-down by z, calls update/draw on all layers
- [ ] Route camera pan/zoom through scene (reuse logic in `src/renderer/gvizRenderer.c`) so layers never touch the camera directly
- [ ] Add scene source files to `CMakeLists.txt`

### Layer System
- [ ] Extend `gvizLayerVTable` in `include/renderer/layers/gvizLayer.h` with `hitTest(self, worldPos) -> int` for click routing
- [ ] Add `gvizLayer` flags field: visible, screen-space vs world-space, capturesInput
- [ ] Document that draw callbacks receive the scene camera; screen-space layers draw outside `BeginMode2D`
- [ ] Update `gvizLayerGraph` draw to use the passed camera consistently (already partially done) and implement `hitTest` against vertex positions

### Main Menu Layer
- [ ] Create `include/renderer/layers/gvizLayerMainMenu.h` exposing `gvizLayerMainMenuInit` and a `GVIZ_LAYER_MAIN_MENU_VTABLE`
- [ ] Implement `src/renderer/layers/gvizLayerMainMenu.c` with raygui buttons: "Load Scene", "Demo: GRIP Sierpinski", "Blank Scene"
- [ ] On click, the main menu requests scene transition via a callback pointer stored in the layer state
- [ ] Add main-menu source files to `CMakeLists.txt`

### Demo / Scene Builders
- [ ] Create `include/app/gvizSceneBuilders.h` with `gvizBuildBlankScene`, `gvizBuildGRIPSierpinskiScene`, `gvizBuildSceneFromTreeFile(const char *path)`
- [ ] Implement builders in `src/app/gvizSceneBuilders.c` — each returns a fully initialized `gvizScene*` using existing utilities in `src/utils/graphs.c` and tree loader
- [ ] Wire main-menu callbacks in `src/app/main.c` to swap active scene by releasing old and initializing new

### New Graph Layer (Interactive Creation)
- [ ] Add "New Graph" button / shortcut in a screen-space toolbar layer
- [ ] Extend `gvizLayerGraph` state with an `editing` flag and a pending-edge-from vertex id
- [ ] In `gvizLayerGraphHandleEvent`, on Cmd+Click empty space: call `gvizGraphAddVertex` and extend embedding array
- [ ] On Cmd+Click existing vertex: if no pending vertex, store it; else call `gvizGraphAddEdge` and clear pending
- [ ] While `editing`, step the Tutte embedding each frame in `gvizLayerGraphUpdate` for real-time relaxation
- [ ] Switch layer to default Tutte embedding on creation (see Embedding State Machine below)

### Tree Loading (JSON via jq)
- [ ] Create `include/utils/gvizTreeIO.h` declaring `gvizLoadTreeJSON(const char *path, gvizGraph *out, size_t *outRoot)` and `gvizSaveTreeJSON(const gvizGraph *g, size_t root, const char *path)`
- [ ] Implement in `src/utils/gvizTreeIO.c` — shell out to `jq` to parse/emit; build a directed tree with explicit root
- [ ] Add file-picker shim (raygui `GuiTextInputBox` or fixed path prompt) invoked from main menu "Load Scene"
- [ ] Add tree IO sources to `CMakeLists.txt`
- [ ] (Explicitly out of scope: any non-tree serialization)

### Embedding State Machine (per graph layer)
- [ ] Add an `enum gvizEmbeddingKind` in `include/renderer/embeddings/gvizEmbeddedGraph.h` covering Tutte, GRIP, Planar, Schnyder, ForceDirected, ReingoldTilford
- [ ] Extend `gvizLayerGraph` with current kind, a union/tagged struct holding the active algorithm state, and cached precomputed state (e.g. `gvizPlanarEmbeddingState::kuratowskiSubdivision`)
- [ ] Add `gvizLayerGraphSetEmbedding(layer, kind)` that releases the current algorithm state and initializes the new one; reuses cached planar kuratowski when switching back
- [ ] In `gvizLayerGraphUpdate`, step whichever algorithm is active (if iterative)
- [ ] Default new graphs to Tutte; when layer is loaded from a tree file, default to ReingoldTilford

### Right-Click Context Menu
- [ ] Create `include/renderer/layers/gvizLayerContextMenu.h` with `gvizLayerContextMenuInit(layer, worldPos, items, count)`
- [ ] Implement `src/renderer/layers/gvizLayerContextMenu.c` as a short-lived screen-space layer drawn with raygui at the click location
- [ ] `gvizLayerGraphHandleEvent` spawns a context menu on right-click over a graph layer, populated with embedding-switch actions
- [ ] Clicking outside the menu or selecting an item releases the context-menu layer via `gvizSceneRemoveLayer`

### HUD & Sidebar
- [ ] Create `include/renderer/layers/gvizLayerHUD.h` for a screen-space layer showing iteration/delta/mode text (generalize the HUD from `tests/renderer/tutteDemo.c`)
- [ ] Create `include/renderer/layers/gvizLayerSidebar.h` for a screen-space panel with the active graph layer's embedding controls (raygui sliders/toggles)
- [ ] Both layers mark themselves screen-space and render outside `BeginMode2D`

### Wiring & Entry Point
- [ ] `src/app/main.c`: open window, create initial scene containing only the main menu layer, run frame loop (`handleInput` → `update` → `draw`)
- [ ] On scene transition, release the old `gvizScene` and install the new one
- [ ] Confirm build produces `./build/grapher` and launches the main menu

### Documentation
- [ ] Document all new public APIs in their `.h` files, matching existing style
- [ ] Update `CLAUDE.md` GUI section once the core abstractions are settled (post-approval, not now)
