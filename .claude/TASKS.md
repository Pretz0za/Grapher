# TASKS

## Epic 1: Switch PolyTutte to exact (non-realtime) solver

The current layer uses `gvizTutteState` (iterative Jacobi, stepped each frame). Replace it with
`gvizTutteSolveState` (Cholesky exact solve — one call produces final positions).

- [x] In `include/app/gvizLayerPolyTutte.h`:
  - Replace `#include "renderer/embeddings/gvizTutteEmbedding.h"` with `#include "renderer/embeddings/gvizTutteSolveEmbedding.h"`.
  - Replace `gvizTutteState tutte` field with `gvizTutteSolveState tutte`.
  - Remove the `hasTutte` field comment note about relaxation (it no longer applies); keep `hasTutte int` flag.
- [x] In `src/app/gvizLayerPolyTutte.c`:
  - In `gvizLayerPolyTutteInit`: replace `gvizTutteEmbeddingInit` → `gvizTutteSolveEmbeddingInit`, replace `gvizTutteFixConvexPolygon` → `gvizTutteSolveFixConvexPolygon`, replace `gvizTutteEmbeddingSeedInterior` call with `gvizTutteSolveEmbeddingStep(&layer->tutte, 0)` (one-shot solve; positions are exact after this call). Remove the `layer->tutte.relaxationRate = 10.0` line.
  - In `gvizLayerPolyTutteUpdate` INITIAL branch: remove the `gvizTutteEmbeddingStep` call entirely. Since the solve runs in Init, by the time Update is called the positions are already exact — just mark `gpuDirty = 1` once to upload positions and return. Set a flag or check `self->tutte.converged` to avoid re-uploading every frame.
  - In the FINAL branch: replace `gvizTutteEmbeddingRelease` + `gvizTutteEmbeddingInit` + `gvizTutteFixConvexPolygon` + `gvizTutteEmbeddingSeedInterior` with `gvizTutteSolveEmbeddingRelease` + `gvizTutteSolveEmbeddingInit` + `gvizTutteSolveFixConvexPolygon` + `gvizTutteSolveEmbeddingStep(&self->tutte, 0)`.
  - Fix the `eg` cast: `(gvizEmbeddedGraph *)&self->tutte` still works because `gvizTutteSolveState` has `gvizEmbeddedGraph graph` as its first member (same layout). Verify the field name is `graph` (not `embedded` or similar) in the header.

## Epic 2: Keybindings HUD in every demo layer

Read `src/renderer/core/gvizScene.c` (or wherever `gvizSceneDraw` is implemented) to understand whether layer Draw callbacks are called inside or outside a camera mode. Then add a keybinding overlay to each demo layer's Draw function using raylib `DrawText` in screen coordinates.

- [ ] Read `src/core/gvizScene.c` to check if BeginMode2D/EndMode2D wraps layer draws. If it does, each layer's Draw must call `EndMode2D()` before `DrawText`, then `BeginMode2D(camera)` after. If not, just call `DrawText` directly.
- [ ] In `gvizLayerPolyTutteDraw` (`src/app/gvizLayerPolyTutte.c`), after `gvizGraphVBODraw`, render a keybinding string at screen position (10, 10) in font size 18, dark gray:
  - Phase INITIAL (not yet scanned): `"SPACE  scan all faces   R  random face   scroll/drag  pan+zoom"`
  - Phase SCANNING: `"scanning faces...   R  stop & pick random"`
  - Phase FINAL (re-running): `"embedding...   SPACE  scan again   R  new random face"`
- [ ] In `gvizLayerTutteDraw` (`src/app/gvizLayerTutte.c`), after the VBO draw, render at (10, 10):
  - `"SPACE  add vertex   R  reset   S  save   scroll/drag  pan+zoom"` (or whatever keys exist — read `gvizLayerTutteHandleEvent` first and list only real keys).
- [ ] In `gvizLayerGRIPLiveDraw` (or wherever the GRIP live layer draws, `src/app/gvizLayerGRIPLive.c`), add: `"scroll/drag  pan+zoom"` (or relevant keys found in its HandleEvent).

## Epic 3: R key picks a random face and re-embeds

- [ ] In `gvizLayerPolyTutteHandleEvent`, add a `KEY_R` case that works in ANY phase:
  1. If `self->faces.count == 0` (faces not yet enumerated): call `gvizComputeRotationSystem`, `pt_rebuildIndex`, enumerate faces into `self->faces` using `gvizPlanarEmbeddingFaces` (same code as the SPACE handler). Set `gpuDirty = 2`.
  2. If `self->faces.count == 0` after enumeration: return 1 (no faces, nothing to do).
  3. Pick `size_t randIdx = (size_t)rand() % self->faces.count`.
  4. Get the face verts. If face has < 3 verts, return 1.
  5. Release and re-init Tutte with that face as boundary: `gvizTutteSolveEmbeddingRelease`, `gvizTutteSolveEmbeddingInit`, `gvizTutteSolveFixConvexPolygon`, `gvizTutteSolveEmbeddingStep`. Set `hasTutte = 1`, `gpuDirty = 2`.
  6. `pt_clearHighlights`, set `phase = GVIZ_POLY_TUTTE_INITIAL`, `scanFaceIdx = 0`, `bestFaceArea = -DBL_MAX`, `scanTimer = 0`.
  7. Return 1.
- [ ] Seed the random number generator once in `main.c` with `srand((unsigned)time(NULL))` (add `#include <time.h>`).

## Build
- [ ] Build with `cd build && cmake .. && make` and fix any errors.
