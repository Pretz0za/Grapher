# TASKS

## Epic: Disc Shader Style

- **Shader uniform for ring vs filled**:
  - [x] Add `int locFill;` field to `gvizDiscShader` struct in `include/renderer/layers/gvizDiscShader.h`.
  - [x] In `src/renderer/layers/gvizDiscShader.c`, add `uniform float uFill;` to `DISC_FS_SRC` and modify the body so when `uFill < 0.5` (ring) the FS discards fragments where `d < 0.65` in addition to `d > 1.0`; when `uFill >= 0.5` keep existing filled behavior.
  - [x] In `gvizDiscShaderGet`, query the `uFill` location into `g_shader.locFill`.

- **Pass uFill through draw path**:
  - [x] Update `gvizVertexDiscVBODraw` signature in `include/renderer/layers/gvizVertexDiscVBO.h` to add a trailing `float fill` parameter.
  - [x] In `src/renderer/layers/gvizVertexDiscVBO.c` `gvizVertexDiscVBODraw`, set `uFill` via `rlSetUniform` (RL_SHADER_UNIFORM_FLOAT) when `sh->locFill >= 0` before the instanced draw.
  - [x] In `src/renderer/layers/gvizGraphVBO.c` `gvizGraphVBODraw`, pass `0.0f` for `fill` so `GVIZ_GRAPH_VBO_DISCS` defaults to ring mode.

- **Color change**:
  - [x] In `src/renderer/layers/gvizGraphVBO.c` `gvizGraphVBODraw`, change `float discColor[4] = {0.10f, 0.20f, 0.55f, 1.0f}` to `{0.0f, 0.0f, 0.0f, 1.0f}`.

## Epic: Blank Scene Graph Creation

- **Empty Tutte layer initialization**:
  - [ ] In `include/app/gvizLayerTutte.h`, add fields to `gvizLayerTutte`: `size_t pendingVertex;` (sentinel `SIZE_MAX`) and `int hasTutte;` (1 once `tutte` matrix is built).
  - [ ] In `include/app/gvizLayerTutte.h`, declare `int gvizLayerTutteInitEmpty(gvizLayerTutte *layer, size_t z);`.
  - [ ] In `src/app/gvizLayerTutte.c`, implement `gvizLayerTutteInitEmpty`: `gvizLayerInit` with vtable, `flags = GVIZ_LAYER_VISIBLE`, `gvizGraphInit(&self->graph, 0)`, `gvizGraphVBOInit(&self->vbo)`, `gvizGraphVBOSetMode(..., EDGES|DISCS)`, `gpuDirty = 2`, `paused = 0`, `pendingVertex = SIZE_MAX`, `hasTutte = 0`.
  - [ ] In `gvizLayerTutteRelease`, only call `gvizTutteEmbeddingRelease` when `hasTutte` is set.

- **Blank scene wiring**:
  - [ ] In `src/app/gvizSceneBuilders.c` `gvizBuildBlankScene`, after `gvizSceneInit2D`, allocate a `gvizLayerTutte`, call `gvizLayerTutteInitEmpty`, and `gvizSceneAddLayer`.

- **Hit testing**:
  - [ ] In `src/app/gvizLayerTutte.c`, add static helper `findVertexAtWorld(self, wx, wy, radius)` that scans `self->graph.vertices`, reads each position via `gvizEmbeddedGraphGetVPosition` on `&self->tutte.graph` (or directly off the embedding once present), returns the index within `radius`, else `SIZE_MAX`. For empty/unbuilt embedding, fall back to a side-table of positions stored on the layer (see next task).
  - [ ] Add a `double *positions;` and `size_t positionsCap;` to `gvizLayerTutte` so newly added vertices have a position before the Tutte embedding is (re)built. Free in `gvizLayerTutteRelease`.

- **Right-click vertex / edge creation**:
  - [ ] In `gvizLayerTutteHandleEvent`, add a `case GVIZ_EVENT_MOUSE_DOWN:` branch guarded by `event->mouse.button == GVIZ_MOUSE_RIGHT`.
  - [ ] On miss: `gvizGraphAddVertex(&self->graph, NULL, NULL, NULL)`, grow `positions` and write `(wx, wy)` for the new vertex, mark `gpuDirty = 2`, set `hasTutte = 0`.
  - [ ] On hit `v`: if `pendingVertex == SIZE_MAX` set `pendingVertex = v`; else if `v != pendingVertex` call `gvizGraphAddEdge(&self->graph, pendingVertex, v)`, clear `pendingVertex = SIZE_MAX`, set `gpuDirty = 2` and `hasTutte = 0`.
  - [ ] Return 1 from the handler when a click is consumed.

- **Per-frame Tutte re-relaxation**:
  - [ ] In `gvizLayerTutteUpdate`, when `!hasTutte` and `self->graph.vertices.count >= 1`: if a previous `tutte` exists release it; call `gvizTutteEmbeddingInit(&self->tutte, &self->graph, 2, 1e-5)`, copy current `positions` into the embedding via `gvizEmbeddedGraphGetVPosition`, call `gvizTutteEmbeddingBuildMatrix`, set `hasTutte = 1`.
  - [ ] When `hasTutte && !paused && self->tutte.numInterior > 0 && !self->tutte.converged`, call `gvizTutteEmbeddingStep(&self->tutte, dt)`, sync positions back into the layer's `positions` table, and set `gpuDirty = 1`.
  - [ ] In `gvizLayerTutteDraw`, when `hasTutte` use `&self->tutte` as the `gvizEmbeddedGraph *`; when not, build a temporary `gvizEmbeddedGraph` view over `self->graph` + `positions` so the VBO can rebuild from current positions even before Tutte exists.
