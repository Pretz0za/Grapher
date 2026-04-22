# Epic: Real-time Tutte Barycentric Embedding

Implement Tutte's barycentric algorithm as an iterative, force-directed-style solver.
For a planar graph with a fixed convex outer face, interior vertices are relaxed to
the arithmetic mean of their neighbors. Each iteration step is exposed so the
renderer can visualize live convergence, and a blocking `Run` API is provided for
tests and offline use.

Files created:

- `include/renderer/embeddings/gvizTutteEmbedding.h`
- `src/renderer/embeddings/gvizTutteEmbedding.c`
- `tests/renderer/embeddings/gvizTutteTests.c`
- `tests/renderer/tutteDemo.c`

## Data Structures

- [x] Define `gvizTutteState` with `gvizEmbeddedGraph` as first field
- [x] `boundary`, `boundaryCount`, `isBoundary` bit array
- [x] `scratch` Jacobi double-buffer, `iteration`, `lastMaxDelta`, `converged`
- [x] `useGaussSeidel`, `epsilon`, `relaxationRate` (dt-blend factor)
- [x] `#define GVIZ_TUTTE_DEFAULT_EPSILON 1e-5`

## Core Algorithm

- [x] `gvizTutteEmbeddingInit` — allocates isBoundary + scratch, sets defaults
- [x] `gvizTutteEmbeddingRelease` — frees all owned memory
- [x] `gvizTutteEmbeddingSetBoundary` — pins vertices, validates count ≥ 3 and indices
- [x] `gvizTutteEmbeddingSeedInterior` — seeds interior to boundary centroid
- [x] `gvizTutteEmbeddingStep(s, dt)` — dt-weighted relaxation; Jacobi snapshots old
      positions into scratch before the loop; Gauss-Seidel updates in-place
- [x] `gvizTutteEmbeddingRun` — loops Step until converged or maxIters

## Outer Face Handling

- [x] `gvizTutteFixConvexPolygon` — places boundary on regular polygon of given radius

## Renderer Integration

- [x] `tutteDemo.c` calls `gvizTutteEmbeddingStep(&s, dt)` (GetFrameTime()) each frame
- [x] Edge VBO rebuilt and re-uploaded via `rlUpdateVertexBuffer` every frame

## Tests & Demo

- [x] 5 Unity tests — all passing (K4 centroid, boundary pinned, convergence,
      Jacobi vs GS fixed point, input validation)
- [x] `tutteDemo.c` — 12x12 grid, rim pinned to circle, dt-driven loop,
      SPACE/S/R/G keys, pan/zoom
- [x] CMake: `gvizTutteEmbedding.c` in `graphvis`, `gvizTutteTests` + `tutteDemo` targets
