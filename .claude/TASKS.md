# TASKS

## Epic 1: .obj mesh loader -> PolyTutte scene

### OBJ parser (new utility module)
- [x] Create `include/utils/gvizOBJLoader.h` declaring:
  - `int gvizLoadOBJAsGraph(const char *path, gvizGraph *outGraph, size_t outerTriangle[3]);`
  - Returns 0 on success, -1 on parse/IO/alloc failure. On success, `outGraph` is initialized as an undirected `gvizGraph` and `outerTriangle` is filled with the first three vertex indices of face 0 (1-based -> 0-based).
- [x] Create `src/utils/gvizOBJLoader.c`:
  - Open the file with `fopen`; read line-by-line into a fixed buffer (`MAX_LINE_SIZE`).
  - Skip blank lines and comments (`#`).
  - For each `v ` line, increment a vertex counter (positions are not stored; PolyTutte computes its own embedding).
  - Pre-pass to count `v` lines, then `gvizGraphInit(outGraph, 0)` and `gvizGraphAddVertex` for each vertex.
  - For each `f ` line, parse face indices. Tokens may be `idx`, `idx/uv`, `idx/uv/n`, or `idx//n`; take the integer before the first `/`. Convert from 1-based to 0-based.
  - For each consecutive pair `(face[i], face[(i+1) % faceLen])`, call `gvizGraphAddEdge` only if not already present (use `gvizGraphEdgeExists` to dedupe).
  - On the FIRST face encountered, save its first three indices into `outerTriangle` before processing edges.
  - Free the line buffer; return 0. On any failure, `gvizGraphRelease` and return -1.
- [x] Add `src/utils/gvizOBJLoader.c` to the `graphvis` static library sources list in `CMakeLists.txt`.

### Scene builder
- [x] Add to `include/app/gvizSceneBuilders.h`:
  - `int gvizBuildPolyTutteFromOBJScene(gvizScene *out, const char *objPath);`
- [x] Add implementation in `src/app/gvizSceneBuilders.c` mirroring `gvizBuildPolyTutteDemoScene`:
  - `gvizSceneInit2D`, call `gvizLoadOBJAsGraph` to build the graph + outer triangle.
  - Allocate `gvizLayerPolyTutte`, call `gvizLayerPolyTutteInit` with the graph + outer triangle.
  - On any failure, release intermediate state and return -1; on success `gvizGraphRelease` (the layer cloned it) and `gvizSceneAddLayer`.
- [x] `#include "utils/gvizOBJLoader.h"` at the top of `gvizSceneBuilders.c`.

### Main menu wiring
- [x] Add `GVIZ_MENU_LOAD_OBJ_TUTTE` to the `gvizMainMenuAction` enum in `include/renderer/layers/gvizLayerMainMenu.h`.
- [x] In `src/renderer/layers/gvizLayerMainMenu.c`, add a "Load OBJ mesh (Tutte)" button that sets `requestedAction = GVIZ_MENU_LOAD_OBJ_TUTTE` and writes the chosen path into `layer->loadPath` (use the same path-input pattern already used by `GVIZ_MENU_LOAD_SCENE`; if a raygui text-input is not yet implemented there, prompt via `stdin` like other path-loading entry points).
- [x] In `src/app/main.c`, handle the new case: call `gvizBuildPolyTutteFromOBJScene(&scene, menu->loadPath)`; on failure fall back to `gvizBuildBlankScene`.

## Epic 2: Fix SCANNING-phase highlight bug

### Diagnose
- [ ] Confirm root cause in `src/app/gvizLayerPolyTutte.c`:
  - `gvizLayerPolyTutteHandleEvent` calls `gvizComputeRotationSystem` (which mutates `self->tutte.graph` / `&self->graph` by adding any missing reverse edges to form a proper rotation system) and then `pt_rebuildIndex(self)` to recompute `edgeStartIdx` against the now-doubled adjacency.
  - The VBO's color buffer was sized for the ORIGINAL adjacency and is not rebuilt (no `gpuDirty = 2` is set in HandleEvent), so `edgeStartIdx` no longer aligns with the VBO's iteration order. As a result `pt_writeColors` walks the new (doubled) graph and falls into the `fi + 6 > colorsCount` early-return path, leaving stale or mis-mapped color entries -> "all edges red, no vertices".
- [ ] Confirm `gvizLayerGraphSetEdgeHighlight` / `findEdgeBit` index arithmetic is correct: bit = `edgeStartIdx[u] + indexOf(neighbors(u), v)` matches the VBO's iteration `for u in 0..N: for j in nbrs(u): write 2 endpoints`. No fix required there; the bug is the synchronization with topology mutations.

### Fix
- [ ] In `gvizLayerPolyTutteHandleEvent`, after `gvizComputeRotationSystem` and `pt_rebuildIndex`, set `self->gpuDirty = 2` so the next `Draw` rebuilds the VBO (and thus the colors buffer) to match the new edge index.
- [ ] In `gvizLayerPolyTutteUpdate`'s SCANNING branch, do NOT call `pt_clearHighlights` followed by per-face highlight setting on the same frame for ALL faces in one tick. Current loop is correct (one face per frame), but make highlights visible: gate scanning by a small accumulator (e.g. `scanTimer += dt; if scanTimer < 0.2f return;`) so each face is shown for ~200ms before advancing. Add a `float scanTimer;` field in `gvizLayerPolyTutte` and reset it to 0 when entering SCANNING and after each advance.
- [ ] Ensure `pt_clearHighlights` is called exactly once at the start of each SCANNING frame BEFORE setting that frame's face highlights (already done) — verify no other code path leaves stale bits when the phase transitions to FINAL: the FINAL branch already calls `pt_clearHighlights` before re-init; keep it.
- [ ] When SCANNING completes (`scanFaceIdx >= faces.count`), call `pt_clearHighlights` before transitioning to `GVIZ_POLY_TUTTE_FINAL` (current code transitions without clearing — add the clear).
- [ ] Verify `pt_setVertexHL` and `pt_setEdgeHL` operate against the rebuilt index after the rotation system was added (now correct because `pt_rebuildIndex` ran in HandleEvent and the VBO will be rebuilt this frame thanks to `gpuDirty = 2`).

### Verify
- [ ] Build with `cmake --build build` and run `./build/grapher`; pick the PolyTutte demo, press SPACE, observe each face turning red one-at-a-time (vertices + edges of the current face only), then the largest face becoming the new outer boundary on FINAL.
