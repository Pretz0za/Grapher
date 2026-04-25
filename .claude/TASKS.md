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
- [x] Add `grapher` executable target in `CMakeLists.txt` linking `graphvis`
- [x] Add `src/app/main.c` source file to the `grapher` target

### Event System Expansion
- [x] Expand `include/core/event.h` with event types: MOUSE_MOVE, MOUSE_DOWN/UP (with button + modifier), MOUSE_WHEEL, KEY_DOWN/UP, TEXT_INPUT, RESIZE
- [x] Add world-space and screen-space coordinates to mouse events in `gvizEvent` (plain floats, no raylib Vector2 in core/)
- [x] Add `accepted` / `propagate` flag convention for `EventHandler` return value, documented in `include/core/object.h` (1 = accepted/stop, 0 = continue)

### Camera Abstraction
- [x] Create `include/core/gvizCamera.h` tagged-union + `src/core/gvizCamera.c`
- [x] Refactor `DrawFunction` in `core/object.h` to take `const gvizCamera*`
- [x] Update `gvizLayerGraphDraw` signature accordingly

### Scene & Camera
- [x] Create `include/core/gvizScene.h` defining `gvizScene` struct
- [x] Declare scene lifecycle in header
- [x] Declare per-frame API
- [x] Implement `src/core/gvizScene.c`
- [x] Route camera pan/zoom through scene (2D pan/zoom inlined, 3D camera TBD)
- [x] Add scene source files to `CMakeLists.txt`

### Layer System
- [x] Extend `gvizLayerVTable` with `hitTest(self, wx, wy) -> int`
- [x] Add `gvizLayer` flags field (visible, screen-space, capturesInput)
- [x] Document draw callbacks in header; scene handles world/screen passes
- [x] gvizLayerGraph draw uses gvizCamera; hitTest against vertex positions

### Main Menu Layer
- [x] Create `include/renderer/layers/gvizLayerMainMenu.h` + vtable
- [x] Implement screen-space menu with raygui buttons
- [x] Menu exposes `requestedAction` enum for the scene owner to read
- [x] Add main-menu source files to `CMakeLists.txt`

### Demo / Scene Builders
- [x] Create `include/app/gvizSceneBuilders.h`
- [x] Implement `gvizBuildBlankScene`, `gvizBuildGRIPSierpinskiScene`, `gvizBuildSceneFromTreeFile`
- [x] main.c swaps scenes based on menu action

### New Graph Layer (Interactive Creation)
- [ ] Add "New Graph" button / shortcut in a screen-space toolbar layer
- [ ] Extend `gvizLayerGraph` state with an `editing` flag and a pending-edge-from vertex id
- [ ] In `gvizLayerGraphHandleEvent`, on Cmd+Click empty space: call `gvizGraphAddVertex` and extend embedding array
- [ ] On Cmd+Click existing vertex: if no pending vertex, store it; else call `gvizGraphAddEdge` and clear pending
- [ ] While `editing`, step the Tutte embedding each frame in `gvizLayerGraphUpdate` for real-time relaxation
- [ ] Switch layer to default Tutte embedding on creation (see Embedding State Machine below)

### Tree Loading (JSON, hand-rolled)
- [x] Create `include/utils/gvizTreeIO.h` with load + save APIs
- [x] Implement hand-rolled parser (no jq dependency) in `src/utils/gvizTreeIO.c`
- [ ] Add file-picker shim (raygui `GuiTextInputBox`) invoked from main menu "Load Scene"
- [x] Add tree IO sources to `CMakeLists.txt`
- [x] (Explicitly out of scope: any non-tree serialization)

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

## Epic: Disc-Rendered Vertices (GLSL Circle Shader)

Add a per-vertex disc renderer as an option on `gvizGraphVBO`. Each vertex is
drawn as a screen-aligned quad whose fragments are clipped to a circle in the
fragment shader. Per-vertex radius is uploaded as an instanced attribute.
Edges-only and discs-only are independently toggleable; both can render in
the same frame. Shaders are compiled once from inline C string literals
(no asset pipeline) and reused.

### Shader Sources (inline C strings)
- [x] Create `include/renderer/layers/gvizDiscShader.h` declaring an opaque `gvizDiscShader` struct (program id + uniform locations) and `gvizDiscShaderGet(void)` returning a process-wide singleton (lazy init), plus `gvizDiscShaderRelease(void)` for shutdown
- [x] Create `src/renderer/layers/gvizDiscShader.c` containing two `static const char *` literals: `DISC_VS_SRC` (GLSL `#version 330 core`) and `DISC_FS_SRC`
- [x] Vertex shader inputs: `layout(location=0) in vec3 aCenter;` (per-instance), `layout(location=1) in float aRadius;` (per-instance), `layout(location=2) in vec2 aCorner;` (per-vertex of quad, values in {-1,+1}); uniforms: `uniform mat4 uMVP;`, `uniform mat4 uModelView;`, `uniform mat4 uProjection;`; outputs: `out vec2 vLocal;` (the corner in {-1,+1}, used by the FS to compute distance from disc center)
- [x] Vertex shader body: compute the world-space quad corner as `aCenter + cameraRight*aRadius*aCorner.x + cameraUp*aRadius*aCorner.y` where cameraRight/cameraUp are derived from `inverse(uModelView)` rows so the quad is screen-aligned (billboard) for both 2D and 3D cameras; pass `aCorner` through as `vLocal`
- [x] Fragment shader: compute `float d = length(vLocal); if (d > 1.0) discard;` then output `uColor` (uniform vec4); add a 1px AA edge via `smoothstep(1.0, 1.0 - fwidth(d), d)` on alpha
- [x] Implement `compileShader(GLenum type, const char *src)` and `linkProgram(vs, fs)` helpers in `gvizDiscShader.c`; on failure call `glGetShaderInfoLog` / `glGetProgramInfoLog` and `fprintf(stderr, ...)` then return 0
- [x] Cache uniform locations (`uMVP`, `uModelView`, `uProjection`, `uColor`) at link time; expose them via the `gvizDiscShader` struct
- [x] Add `src/renderer/layers/gvizDiscShader.c` to the `graphvis` target in `CMakeLists.txt`

### Per-Vertex Disc VBO
- [x] Create `include/renderer/layers/gvizVertexDiscVBO.h` declaring:
  ```c
  typedef struct gvizVertexDiscVBO {
      unsigned int vaoId;
      unsigned int vboCorners;   /* static: 4 vec2 corners {-1,-1},{+1,-1},{-1,+1},{+1,+1} */
      unsigned int vboCenters;   /* dynamic: float[3] per vertex */
      unsigned int vboRadii;     /* dynamic: float per vertex */
      int instanceCount;         /* number of vertices = number of instances */
  } gvizVertexDiscVBO;
  void gvizVertexDiscVBOInit(gvizVertexDiscVBO *vbo);
  void gvizVertexDiscVBORelease(gvizVertexDiscVBO *vbo);
  void gvizVertexDiscVBORebuild(gvizVertexDiscVBO *vbo, gvizEmbeddedGraph *eg, const float *radii);
  void gvizVertexDiscVBOUploadPositions(gvizVertexDiscVBO *vbo, gvizEmbeddedGraph *eg);
  void gvizVertexDiscVBOUploadRadii(gvizVertexDiscVBO *vbo, const float *radii, size_t n);
  void gvizVertexDiscVBODraw(const gvizVertexDiscVBO *vbo, const float color[4]);
  ```
- [x] Implement `src/renderer/layers/gvizVertexDiscVBO.c`:
  - [x] `Init` zeros the struct
  - [x] `Rebuild`: release any old buffers, allocate `rlLoadVertexArray`, upload the static 4-corner quad to `vboCorners` (attribute 2, `glVertexAttribDivisor(2, 0)`), upload centers to `vboCenters` (attribute 0, `glVertexAttribDivisor(0, 1)`), upload radii to `vboRadii` (attribute 1, `glVertexAttribDivisor(1, 1)`), set `instanceCount = N`
  - [x] `UploadPositions`: rebuild the float[3*N] center array from `eg` (cast double→float, fill z=0 when `dim<3`) and call `rlUpdateVertexBuffer(vboCenters, ...)`
  - [x] `UploadRadii`: `rlUpdateVertexBuffer(vboRadii, ...)`
  - [x] `Draw`: bind shader from `gvizDiscShaderGet()`, set `uMVP`/`uModelView`/`uProjection`/`uColor`, bind VAO, call `glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instanceCount)`, unbind
- [x] Add `src/renderer/layers/gvizVertexDiscVBO.c` to the `graphvis` target in `CMakeLists.txt`

### gvizGraphVBO Option Flags
- [x] Add an enum to `include/renderer/layers/gvizGraphVBO.h`:
  ```c
  enum gvizGraphVBOMode {
      GVIZ_GRAPH_VBO_EDGES = 1 << 0,
      GVIZ_GRAPH_VBO_DISCS = 1 << 1,
  };
  ```
- [x] Extend `gvizGraphVBO` with `gvizVertexDiscVBO discs;`, `unsigned int mode;`, `float *radii;` (CPU-side, length N), `size_t radiiCount;`
- [x] Update `gvizGraphVBOInit` to zero the new fields and default `mode = GVIZ_GRAPH_VBO_EDGES`
- [x] Update `gvizGraphVBORelease` to `GVIZ_DEALLOC` `radii` and call `gvizVertexDiscVBORelease`
- [x] Update `gvizGraphVBORebuild` to also (re)allocate `radii` to length `N`, fill with `GVIZ_GRAPH_VBO_DEFAULT_RADIUS` (define in header, e.g. `5.0f` in world units), and call `gvizVertexDiscVBORebuild` when `mode & GVIZ_GRAPH_VBO_DISCS`
- [x] Update `gvizGraphVBOUploadPositions` to also call `gvizVertexDiscVBOUploadPositions` when discs are enabled
- [x] Add `void gvizGraphVBOSetMode(gvizGraphVBO *vbo, unsigned int mode);` — flips bits and lazily builds disc buffers if `DISCS` becomes set after a prior `Rebuild`
- [x] Add `void gvizGraphVBOSetVertexRadius(gvizGraphVBO *vbo, size_t idx, float radius);` — writes into the CPU-side `radii` array and calls `gvizVertexDiscVBOUploadRadii`
- [x] Add `void gvizGraphVBOSetAllRadii(gvizGraphVBO *vbo, float radius);` — bulk setter
- [x] Update `gvizGraphVBODraw`: if `mode & GVIZ_GRAPH_VBO_EDGES` draw lines (existing path); if `mode & GVIZ_GRAPH_VBO_DISCS` call `gvizVertexDiscVBODraw(&vbo->discs, ...)` after edges so discs paint on top

### gvizLayerGraph Integration
- [x] Add `unsigned int vboMode;` field to `gvizLayerGraph` in `include/renderer/layers/gvizLayerGraph.h`, defaulted to `GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS` in `gvizLayerGraphInit` (so the default look has visible vertices)
- [x] In `gvizLayerGraphInit` (in `src/renderer/layers/gvizLayerGraph.c`), call `gvizGraphVBOSetMode(&layer->vbo, layer->vboMode)` after the initial `Rebuild`
- [x] Add `void gvizLayerGraphSetVBOMode(gvizLayerGraph *layer, unsigned int mode);` that updates the field and forwards to `gvizGraphVBOSetMode`
- [x] Confirm `gvizLayerGraphDraw` requires no changes (single `gvizGraphVBODraw` call already)

### Shader Singleton Lifecycle
- [x] `gvizDiscShaderGet` lazily compiles+links on first call; safe to call before any window exists? — document that it MUST be called only after `InitWindow`/GL context exists; first call site is `gvizVertexDiscVBORebuild`
- [x] `gvizDiscShaderRelease` called once at shutdown — add a hook from `gvizRenderer` shutdown path (or document that the OS reclaims GL state and skip)

### Verification (manual, no tests written)
- [x] Build with `cmake .. && make` — confirm clean compile of new files
- [ ] Run `./build/grapher`, load a tree scene, confirm discs appear at vertex positions and scale with the per-vertex radius
- [ ] Toggle `vboMode` to `GVIZ_GRAPH_VBO_EDGES` only and confirm discs disappear; toggle to `GVIZ_GRAPH_VBO_DISCS` only and confirm edges disappear
- [ ] Confirm 3D scenes (GRIP demo path) render screen-aligned discs that face the camera under rotation
