# TASKS — Graph Tree Panel + Split Graph/Layer Creation

## Epic A — Wire and style the graph tree panel

- [x] Instantiate `gvizLayerGraphTree` in `main.c` startup, hold pointer in `AppState`, add to scene at `z=0`
- [x] Confirm `GVIZ_LAYER_SCREEN_SPACE` flag keeps the panel out of slot layout (no scene changes if so)
- [x] Restyle rows in Windows-tree look in `gvizLayerGraphTreeDraw`: indent children by `GT_INDENT`, draw vertical guide line down left edge of children with horizontal stub line into each child row
- [x] Replace plain row rectangles with node icons: small folder/box glyph for graph row, leaf glyph for view row
- [x] Cap visual depth at 2 (graphs -> views) — no further nesting
- [x] Highlight the active view row when its `layer == scene->activeLayer`
- [x] Reset `app.tree = NULL` and re-instantiate the tree after the OBJ-modal scene rebuild path in `main.c`

## Epic B — "New Graph" modal (`gvizGraphCreatePanel`)

- [ ] Add `include/app/gvizGraphCreatePanel.h`: struct, init/draw/release/event vtable, result enum, params (graphType + param1 + param2)
- [ ] Add `src/app/gvizGraphCreatePanel.c`: modal UI with graph-type dropdown, param1 spinner, conditional param2 spinner, Create/Cancel buttons (reuse `GRAPH_TYPE_LIST`, `graphTypeUsesParam2`, `graphTypeParam1Label`, `graphTypeParam2Label`)
- [ ] Extract `loadDemoGraph` + `buildRandomTree` from `gvizLayerCreate.c` into shared `app/gvizDemoGraphLoad.h/.c` so both panels reuse it
- [ ] Add `gvizApplyGraphCreate(scene, params)` that builds the graph via the shared helper, heap-promotes via `graphToHeap`, calls `gvizSceneRegisterGraph`, returns the handle (no layer)
- [ ] Update `CMakeLists.txt` source list with new files

## Epic C — "New Layer from graph" flow

- [ ] Extend `gvizLayerCreateParams` with `gvizSceneGraphHandle existingGraph` (default `GVIZ_SCENE_GRAPH_INVALID`)
- [ ] Add `GVIZ_SLOT_FROM_EXISTING_GRAPH` slot kind for the append-layer-only path
- [ ] In `gvizLayerCreatePanel.c`: when `existingGraph` is valid, render only algorithm + embed dim + (default-Full) view section; hide source/graph-type/param/filepath rows; update title
- [ ] In `gvizLayerCreate.c` builders (Tutte / GRIP / PolyTutte / RT): if `params->existingGraph` is valid, resolve via `gvizSceneGetGraph`, retain the handle, skip `loadGraphForSource` + `gvizSceneRegisterGraph`
- [ ] In `gvizApplyLayerCreate`: when slot is `FROM_EXISTING_GRAPH`, append the layer via `gvizSceneAddLayer` (no split required)

## Epic D — Scene wiring and right-click dispatch

- [ ] Extend `AppState` with `gvizLayerGraphTree *tree`, `gvizGraphCreatePanel *graphPanel`, `gvizSceneGraphHandle pendingGraphForLayer`
- [ ] Add scene callbacks `onPanelAreaContextMenu(s, sx, sy, ud)` and `onGraphRowContextMenu(s, handle, sx, sy, ud)` to `gvizScene` in `include/core/gvizScene.h`
- [ ] In `gvizSceneHandleInput`, route right-clicks: if `sx < GVIZ_SCENE_MARGIN_L` dispatch panel-area or graph-row callback (graph-row when click hits a tree graph row); else existing layer/empty-area path
- [ ] Add hit-testing helper in `gvizLayerGraphTree` to map screen y to graph handle (returns 0 if not on a graph row); expose for the scene dispatch
- [ ] Wire `main.c` callbacks: panel-area menu shows `ACT_NEW_GRAPH`; graph-row menu shows `ACT_NEW_LAYER_FROM_GRAPH` (stash handle in `pendingGraphForLayer`)
- [ ] Add action ids `ACT_NEW_GRAPH`, `ACT_NEW_LAYER_FROM_GRAPH`; extend `drainContextMenu` to open the appropriate panel
- [ ] Add `drainGraphCreatePanel(scene, app)` mirroring `drainCreatePanel` — on confirm call `gvizApplyGraphCreate`, then remove
- [ ] When opening the layer-create panel from a graph row, set `params.existingGraph = pendingGraphForLayer` and `slotKind = GVIZ_SLOT_FROM_EXISTING_GRAPH`
