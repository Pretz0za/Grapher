# TASKS

Two epics. Planning only — do NOT implement until approved.

---

## Epic A: Fix 3D GRIP renders flat (Begin/EndMode mismatch)

Layer draw functions hardcode `EndMode2D()` before the HUD text and only
restart `BeginMode2D` if the camera is 2D. In 3D scenes the scene wraps the
draw in `BeginMode3D`/`EndMode3D`, so the hardcoded `EndMode2D()` corrupts
state and the matrix stack ends mismatched — the GRIP layer effectively
projects flat.

Fix: every layer that draws HUD text must end the *active* mode (matched to
camera kind) before the HUD and restart the matching mode after.

### Saga A.1: GRIPLive draw — kind-aware end/begin
- [x] In `src/app/gvizLayerGRIPLive.c::gvizLayerGRIPLiveDraw`, replace the
      unconditional `EndMode2D()` with:
      `if (camera->kind == GVIZ_CAMERA_2D) EndMode2D(); else EndMode3D();`
- [x] After the HUD `DrawText`, restart with the matching `BeginMode2D` /
      `BeginMode3D(camera->c3d)` branch.

### Saga A.2: PolyTutte draw — same fix
- [x] In `src/app/gvizLayerPolyTutte.c::gvizLayerPolyTutteDraw`, apply the
      same kind-aware end/begin pair around the HUD `DrawText`.

### Saga A.3: Tutte draw — same fix
- [x] In `src/app/gvizLayerTutte.c::gvizLayerTutteDraw`, apply the same
      kind-aware end/begin pair around the HUD `DrawText`.

### Saga A.4: Audit GRIP and OBJ for the same pattern
- [x] `src/app/gvizLayerGRIP.c::gvizLayerGRIPDraw` — confirmed no HUD/EndMode2D.
- [x] `src/app/gvizLayerOBJ.c::gvizLayerOBJDraw` — confirmed no HUD/EndMode2D.

### Saga A.5: Smoke
- [x] Build passes; runtime smoke deferred to user.

---

## Epic B: Enhanced demo panel + parametric graphs + N-D GRIP w/ PCA

Replace flat demo enum with a Demos sub-form (graph type + parameters +
embedding dimension). When `embDim > drawDim`, project via PCA before GPU
upload so high-dimensional GRIP runs render correctly in 2D/3D.

### Saga B.1: Extend `gvizLayerCreateParams` schema
- [x] In `include/app/gvizLayerCreatePanel.h`:
  - [x] Add `gvizDemoGraphType` enum: `SIERPINSKI_TRI`, `SIERPINSKI_TET`,
        `CARPET`, `RECT_MESH`, `TET_MESH`, `EQ_TRI_MESH`, `KNOTTED_RECT`,
        `MOBIUS`, `KLEIN`, `RANDOM_TREE`.
  - [x] Add fields to `gvizLayerCreateParams`: `gvizDemoGraphType graphType`,
        `int graphParam1`, `int graphParam2`, `int embDim`.
  - [x] Defaults: `graphType=SIERPINSKI_TRI`, `graphParam1=3`,
        `graphParam2=3`, `embDim` set by mode (2 for 2D, 3 for 3D) at
        panel init.
- [x] In `gvizLayerCreatePanelInit`, initialize the new fields.

### Saga B.2: Panel UI — Demos sub-form
- [x] In `src/app/gvizLayerCreatePanel.c`:
  - [x] Add a `graphTypeDropdownEdit` field to the panel struct (header).
  - [x] When source dropdown is "Demos" (renamed from per-demo entries),
        render a `GuiDropdownBox` for graph type with all 10 entries.
  - [x] Per type, render 0–2 `GuiSpinner` controls labelled appropriately
        (e.g. depth for Sierpinski; L, W for rect mesh; rows, cols for
        Möbius/Klein). Show only inputs relevant to the chosen type.
  - [x] Add an "Embedding dim" `GuiSpinner` (min 2, max 8). Clamp on edit.
  - [x] Drop the obsolete demo entries from the Source dropdown — collapse
        to: "Demos;From file".
- [x] Adjust `PANEL_H` upward to fit new rows; recompute layout offsets.

### Saga B.3: Dispatch demo graph type → generator
- [x] In `src/app/gvizLayerCreate.c::loadGraphForSource`:
  - [x] Replace the per-source switch arms for the old demo enums with a
        single `GVIZ_SRC_DEMO` arm that switches on `params->graphType` and
        calls the right `utils/graphs.h` generator with `graphParam1` /
        `graphParam2`.
  - [x] For `RANDOM_TREE`, keep the existing local `buildRandomTree`
        helper, parameterized by `graphParam1` (max depth).
- [x] Remove `GVIZ_SRC_DEMO_SIERPINSKI`, `GVIZ_SRC_DEMO_OCTAHEDRON`,
      `GVIZ_SRC_DEMO_RANDOM_TREE` from the source enum (header).
- [x] Drop `buildOctahedronGraph` (replaced by parametric generators) or
      keep it only if a test still references it (verify via grep first).

### Saga B.4: Plumb `embDim` into GRIP layer init
- [x] In `include/app/gvizLayerGRIPLive.h`, expose
      `gvizLayerGRIPLiveInitWithDim` (currently file-static) so the mux can
      call it with arbitrary dim.
- [x] In `src/app/gvizLayerCreate.c::buildGRIPLayer`, when algo is GRIP
      pass `params->embDim` into `gvizLayerGRIPLiveInitWithDim`. Camera
      branch: if `embDim >= 3` use 3D camera (already handled inside the
      init); otherwise 2D.
- [x] Note: only GRIP supports `embDim > 3`. Tutte / PolyTutte / RT keep
      hardcoded 2D init; panel should grey-out the dim spinner for those
      algos (or just ignore the value).

### Saga B.5: VBO `drawDim` field + scratch buffer
- [x] In `include/renderer/layers/gvizGraphVBO.h`:
  - [x] Add `int drawDim` field to `gvizGraphVBO` (2 or 3; defaults to 2).
  - [x] Add `double *projScratch` and `size_t projScratchCap` for PCA
        scratch (interleaved `drawDim`-vectors per vertex).
  - [x] Add setter `gvizGraphVBOSetDrawDim(gvizGraphVBO *, int)`.
- [x] In `gvizGraphVBOInit` / `gvizGraphVBORelease`, init/free new fields.
- [x] Caller plumb: `gvizLayerGRIPLiveInitWithDim` calls
      `gvizGraphVBOSetDrawDim(&self->vbo, dim==3 ? 3 : 2)` after init.

### Saga B.6: PCA projection helper
- [x] New file `src/utils/gvizPCA.c` + `include/utils/gvizPCA.h`:
  - [x] `int gvizPCAProject(const double *X, size_t N, size_t embDim,
                            size_t drawDim, double *Y);`
        — center X (N x embDim row-major), build embDim x embDim
          covariance, call LAPACK `dsyev_` for eigen-decomposition, take
          top `drawDim` eigenvectors, project X onto them into Y.
- [x] CMake: confirm `LAPACK::LAPACK` (or whatever existing target the GRIP
      planar code uses for `dsyev`) is on `graphvis`'s link line; add to
      target_link_libraries if PCA TU isn't covered.

### Saga B.7: Use PCA in `buildExpandedVerts` / disc upload
- [x] In `src/renderer/layers/gvizGraphVBO.c`:
  - [x] Modify `buildExpandedVerts` (and the analogous disc-position path
        in `gvizVertexDiscVBOUploadPositions` / `Rebuild`) so that when
        `eg->embedding.dim > vbo->drawDim`, it:
        1. resizes `projScratch` to `N * drawDim` doubles,
        2. runs `gvizPCAProject` on `eg->embedding.vertexPositions`,
        3. reads endpoint coords from the projected scratch instead of `p`.
  - [x] When `embDim <= drawDim`, keep the current direct-read fast path.
- [x] `gvizVertexDiscVBO` similarly needs to read projected positions when
      higher-dim. Easiest: have GraphVBO pre-project once per upload and
      pass the projected buffer down via a new helper, rather than
      duplicating PCA in DiscVBO. Add internal helper
      `gvizGraphVBOPrepareProjectedPositions(vbo, eg)` returning a
      `double *` (= `eg->embedding.vertexPositions` if no projection
      needed, else `vbo->projScratch`). Use it in both rebuild and upload.
- [x] DiscVBO change: extend its position-upload entry points to accept a
      pre-projected `double *` + `dim` instead of pulling from `eg`
      directly; or add a sibling entry that does. Keep existing entry as a
      thin wrapper for non-projected callers.

### Saga B.8: Refresh during live GRIP refinement
- [x] No extra work — `gvizLayerGRIPLiveUpdate` already bumps `gpuDirty=1`,
      which triggers `gvizGraphVBOUploadPositions` next draw. PCA runs
      inside that path so projection refreshes per-round automatically.

### Saga B.9: Smoke
- [x] Build passes.
- [x] Manual: open Demos panel, pick Möbius strip 8x16 with embDim=5 in a
      3D scene; confirm GRIP renders as a 3D PCA projection that converges.
- [x] Manual: pick rect mesh with embDim=2 in a 2D scene; confirm fast
      path (no PCA) still works.

---

## Out of scope

- No new tests (tests skill not requested).
- No refactors beyond what each saga's surface area requires.
- Not touching the planar / Schnyder / Reingold-Tilford embeddings.
- Old "Issue A–D" entries above are dropped.
