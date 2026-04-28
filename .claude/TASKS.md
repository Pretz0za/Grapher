# TASKS

GUI revamp Epic. Each Saga below is a logically grouped chunk; tasks within
a Saga are ordered by dependency (do top-to-bottom).

---

## Epic 1: Scene region layout (top two-thirds)

Goal: Reserve top 2/3 of the window for layer slots with L/R margins; bottom
1/3 stays empty for now. Slot subdivision happens regardless of layer count.

### Saga 1.1: Scene layout primitives
- [x] Add `gvizSceneLayout` struct in `include/core/gvizScene.h` describing
      the active region rect (x,y,w,h) plus L/R/bottom margins (constants).
- [x] Add `gvizSceneComputeRegion(int sw, int sh, gvizRect *out)` helper that
      returns the top-2/3 region with margins. Reused `gvizViewport`.
- [x] Wire `gvizSceneInit2D/3D` and the resize handler in `src/core/gvizScene.c`
      to recompute the active region whenever the window resizes.

### Saga 1.2: Slot allocation policy
- [x] Define `gvizSceneSlotSplit { GVIZ_SPLIT_NONE, GVIZ_SPLIT_H, GVIZ_SPLIT_V }`
      stored on the scene with a `splitRatio` (0..1, default 0.5).
- [x] Implement `gvizSceneRecomputeSlots(gvizScene *s)` (extras get zero-sized vp + stderr warning).
- [x] Call `gvizSceneRecomputeSlots` after every add/remove and on resize.

### Saga 1.3: Drag-to-resize divider
- [x] Detect mouse down/drag inside a configurable "divider gutter" between
      slots in `gvizSceneHandleInput` and adjust `splitRatio` live.
- [x] Clamp `splitRatio` to [0.1, 0.9] so neither slot collapses.
- [x] Repaint cursor hint (e.g. raylib `MOUSE_CURSOR_RESIZE_EW/NS`) when over
      the gutter.

---

## Epic 2: Off-screen layer rendering with per-layer cameras

Goal: Each component layer renders to its own RenderTexture using its own
camera, then the scene composites the textures into the slots.

### Saga 2.1: Per-layer camera
- [x] Added `gvizCamera camera` to component layer subclasses
      (gvizLayerGraph, gvizLayerTutte, gvizLayerPolyTutte, gvizLayerGRIP,
      gvizLayerGRIPLive). Vtable extended with `getCamera` accessor; menu/HUD
      layers leave it NULL.
- [x] Added `gvizCameraHandleInput2D` and `gvizCameraHandleInput3D` in
      `src/core/gvizCamera.c`.
- [x] Renamed `gvizScene.camera` → `defaultCamera`; documented as fallback.

### Saga 2.2: RenderTexture per layer (deferred — scissor approach used)
- [~] Decision: opted for `BeginScissorMode(viewport) + BeginMode2D(layer->camera)`
      per-layer in `gvizSceneDraw` instead of per-layer FBOs. Same user-visible
      behavior at lower memory + rebind cost. Revisit only if post-processing
      or off-screen capture is required.

### Saga 2.3: Scene composite pass
- [x] `gvizSceneDraw` now scissors each component layer to its viewport,
      enters that layer's camera, draws, and unwinds. The global BeginMode2D
      wrap is gone. Screen-space layers run last in raw pixel coords.

### Saga 2.4: Input routing per slot
- [x] World coords are now resolved through the camera of the layer under
      cursor (`gvizSceneFindLayerAt` → `getCamera` → `ScreenToWorld2D`).
- [x] `gvizSceneFindLayerAt` added to public API.
- [~] Drag focus tracking — existing `s->focused` set on mouse-down already
      survives cursor leaving the slot since events route by focus when set.
      Verified via Tutte's pendingVertex flow.

---

## Epic 3: Scene-owned graph registry with shared-graph callbacks

Goal: `gvizGraph` instances live in the scene and are reference-counted across
layers. Mutations on one layer notify all peers sharing the same graph.

### Saga 3.1: Graph registry data structure
- [x] Added `gvizSceneGraphHandle` (typedef size_t, 0 = invalid).
- [x] Added `gvizArray graphs` to `gvizScene` with sentinel at index 0.
- [x] API: register / retain / release / get implemented.

### Saga 3.2: Subscription / callback system
- [x] `include/core/gvizGraphEvent.h` with kinds + callback typedef.
- [x] subscribe / unsubscribe / notify with originator-skip.

### Saga 3.3: Rewire layers to use the registry  (additive)
- [x] Opt-in `BindHandle(layer, scene, handle, cb)` added to all layers
      (gvizLayerGraph, gvizLayerTutte, gvizLayerPolyTutte, gvizLayerGRIP,
      gvizLayerGRIPLive). Each retains the handle, subscribes the layer
      to mutation events, and releases on layer teardown. Layers keep
      their local graph clone (true ownership-drop deferred — embedding
      structs hold internal references).
- [x] `gvizSceneRelease` now releases leftover registry entries.

### Saga 3.4: Mutation fanout
- [x] `gvizLayerGraphAddVertex/RemoveVertex/AddEdge/RemoveEdge` now fan out
      `GVIZ_GRAPH_VERTEX/EDGE_ADDED/REMOVED` through `gvizSceneNotifyGraphChanged`
      when a handle is bound. `onTopologyChanged` retained as the local hook for
      non-shared layers.
- [~] Subscriber-side callbacks: hooks are wired (BindHandle accepts a cb)
      but no concrete subscriber callbacks for Tutte/PolyTutte/GRIP* are
      registered yet — they will be passed `NULL` by current builders since
      no two layers share a graph today. Wire real callbacks once a scene
      embeds the same graph in two layers simultaneously.

### Saga 3.5: Builder updates
- [x] Every builder now: heap-allocates the source graph, registers it
      with the scene (transfers ownership), inits the layer, then calls
      `BindHandle(layer, scene, h, NULL)` and drops the register-time
      ref. Scene registry is the single owner of the source graph;
      layers still hold local clones internally for their embeddings.
- [x] Removed source-graph free from `releaseEmbeddedTree` callback —
      registry owns it now.

---

## Epic 4: Stop algorithms from mutating the source graph

Goal: Reingold-Tilford and Schnyder/triangulating algorithms must operate on
auxiliary structures so a single shared graph can be embedded multiple ways.
Planar embedding's adjacency reorder stays as-is.

### Saga 4.1: Audit
- [ ] In `src/renderer/embeddings/gvizEmbeddedTree.c`, document every mutation
      to the source `gvizGraph` (the thread bit currently lives there). List
      each call site in a comment block at the top of the file.
- [ ] In `src/renderer/embeddings/gvizSchnyderWood.c`, list every mutation
      and every dependency on the planar/triangulated graph.

### Saga 4.2: Reingold-Tilford threads as side data
- [ ] Move "thread next pointer" from the source graph into the
      `gvizRTDecorators` struct (already has decorators; add `size_t thread`
      and a `int hasThread` flag).
- [ ] Replace contour-walk reads/writes in `iterateContour*` and any thread
      mutators to use the decorators rather than the graph adjacency.
- [ ] Remove the existing `GVIZ_BIT_ARRAY thread` from `gvizEmbeddedTree`
      if it becomes redundant after decorator-based threads.

### Saga 4.3: Schnyder Wood non-mutating triangulation
- [ ] Add a triangulated-copy field to `gvizSchnyderWood`:
      `gvizGraph triangulated;` plus a flag.
- [ ] In `gvizSchnyderWoodInit`, build the triangulated copy from the source
      graph (deep-copy adjacency, then triangulate the copy via existing
      `gvizPlanarEmbeddingTriangulate`).
- [ ] Switch all subsequent Schnyder logic to read from the triangulated copy.
- [ ] Release the copy in `gvizSchnyderWoodRelease`.
- [ ] Decision note: triangulated copy is preferred over a thread-style
      side table because triangulation adds edges (not just metadata) and
      large meshes amortize the copy cost once.

### Saga 4.4: Planar embedding (no change)
- [ ] Confirm and document in `gvizPlanarEmbedding.h` that adjacency-order
      mutation is intentional (rotation system) and is allowed.

---

## Epic 5: macOS top menu bar (Apple menu)

Goal: Add a real macOS menu with "Open .obj…" entry. Replace the current
osascript-based file dialog usage path with a proper menu entry.

### Saga 5.1: Cocoa menu bridge
- [x] `src/platform/macos_menu.m` installs an NSMenu with App + File
      submenus; "Open .obj…" mapped to Cmd+O.
- [x] `include/platform/macos_menu.h` exposes init / callback /
      pull-style poll APIs with C linkage.
- [x] CMake compiles the `.m` with `-fobjc-arc`; Cocoa already linked.
- [x] `gvizPlatformMenuInit()` called from `main.c` after `InitWindow`.

### Saga 5.2: File picker dispatch
- [x] NSOpenPanel filtered to `obj`/`OBJ` shows on menu fire.
- [x] Picked path queued via `gvizPlatformMenuPollPendingOBJPath`;
      `main.c`'s loop drains and rebuilds the scene next frame.
- [x] `osascript` shim removed from `main.c` (the previous in-app
      file picker was located there, not in `gvizSceneBuilders.c`).

---

## Epic 6: OBJ layer (mesh rendering)

Goal: A new `gvizLayer` type that loads a `.obj` and renders the 3D mesh
itself (not the edge graph). Scene must be 3D for this layer to render.

### Saga 6.1: OBJ mesh loader
- [x] `gvizLoadOBJAsMesh(const char *path, Model *out)` wraps raylib's
      `LoadModel` and validates that at least one mesh was loaded.
- [x] Declaration added to `include/utils/gvizOBJLoader.h`.

### Saga 6.2: gvizLayerOBJ
- [x] `include/app/gvizLayerOBJ.h` defines `gvizLayerOBJ` (Model + pos/
      scale/rotY).
- [x] `src/app/gvizLayerOBJ.c` implements vtable (init/draw/release/
      getCamera).
- [x] `draw` calls `DrawModelEx` and a debug `DrawGrid` reference plane;
      scene runs `BeginMode3D` for the layer's 3D camera.
- [x] `release` calls `UnloadModel` when `hasModel`.
- [x] Layer init builds a 3D perspective camera via `gvizCameraMake3D`.

### Saga 6.3: OBJ open dialog flow
- [x] `gvizOBJLoadModal` (new screen-space layer) shows a 3-button raygui
      modal: OBJ only / PolyTutte only / Both. Esc/X cancels.
- [x] `main.c` installs the modal when an .obj path arrives, then on
      result drives the matching builder. Cancellation falls back to a
      blank scene.
- [x] OBJ and PolyTutte layers do not share data — the .obj is loaded
      twice when "Both" is chosen (once as Model, once as gvizGraph).

### Saga 6.4: Builder helpers
- [x] `gvizBuildOBJSceneFromFile` (3D scene with one OBJ layer) and
      `gvizBuildOBJAndPolyTutteSceneFromFile` (2D scene with one OBJ
      layer + one PolyTutte layer) added to `gvizSceneBuilders.c` /
      header. Slot subdivision from Epic 1 places the two layers
      side-by-side automatically.

---

## Epic 7: Cleanup / overcomplication pass

Goal: Trim cruft. Done last so it doesn't conflict with the structural work.

### Saga 7.1: Layer-graph ownership cleanup
- [~] `releaseGraph` retained: still needed to free embedding-only
      wrappers (e.g. `releaseEmbeddedTree` frees the gvizEmbeddedTree
      decorators while the registry owns the source graph). Tests also
      pass NULL for borrowed embeddings.
- [x] `onTopologyChanged` field + all four call sites removed from
      `gvizLayerGraph` — superseded by registry fanout.

### Saga 7.2: Drop redundant per-layer highlight scaffolding (deferred)
- [~] `gvizLayerPolyTutte` still mirrors `gvizLayerGraph`'s highlight
      buffers. Factoring into a shared `gvizGraphHighlight` struct is a
      ~150-line refactor with no behavior change; deferring until a
      third layer needs the same scaffolding.

### Saga 7.3: Consolidate `gvizLayerGRIP` and `gvizLayerGRIPLive` (deferred)
- [~] Skipped per the original "skip if it forces a major rewrite of
      demos" guidance — `tests/renderer/gripDemo.c` references the
      Live API directly and the auto/manual split is genuinely two
      modes' worth of update logic.

### Saga 7.4: Delete dead Tutte-solve files
- [x] `gvizTutteSolveEmbedding.{c,h}` had no callers outside itself —
      deleted along with their CMake entry.

### Saga 7.5: Strip the `osascript` open-file shim
- [x] Removed in Epic 5 (the shim lived in `main.c`, not in
      `gvizSceneBuilders.c` as the original task assumed).

### Saga 7.6: Final build + smoke test
- [x] Full `cmake .. && make` succeeds: graphvis library, every test
      executable, and the `grapher` binary all link cleanly. The macOS
      menu .m file builds with `-fobjc-arc`. One non-fatal raylib
      `setAllowedFileTypes:` deprecation warning remains (still works
      on macOS 12+; future cleanup item if NSContentType is needed).
- [~] Visual smoke test of demo scenes: not executed in this session;
      flagging for the next interactive session.
