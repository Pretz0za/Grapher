# TASKS

## Epic 1: Highlights + dynamic mutation in gvizLayerGraph

Add per-vertex and per-edge red-highlight overlays driven by bitarrays, plus runtime add/remove of vertices and edges. Highlights must NOT trigger geometry rebuilds — they ride a parallel per-endpoint color stream. Mutations resize bitarrays and the edge-bit index.

### Highlight storage on gvizLayerGraph

- [x] Add `GVIZ_BIT_UNIT *vertexHighlight` and `size_t vertexHighlightBits` fields to `gvizLayerGraph` (`include/renderer/layers/gvizLayerGraph.h`); allocate to `GVIZ_ARRAY_UNITS(N)` in `gvizLayerGraphInit` and zero-fill.
- [x] Add `GVIZ_BIT_UNIT *edgeHighlight`, `size_t edgeHighlightBits`, and `size_t *edgeStartIdx` (length = N+1, last entry = total directed-edge count) to `gvizLayerGraph`; allocate and zero-fill in init.
- [x] Add helper `static size_t computeEdgeStartIdx(gvizGraph *g, size_t *out)` (in `src/renderer/layers/gvizLayerGraph.c`) that walks vertex neighbor counts to fill prefix sums and returns total directed-edge count.
- [x] Free `vertexHighlight`, `edgeHighlight`, and `edgeStartIdx` in `gvizLayerGraphRelease`.

### Highlight public API

- [x] Add `gvizLayerGraphSetVertexHighlight(gvizLayerGraph *, size_t v, int on)` and `gvizLayerGraphClearVertexHighlights(gvizLayerGraph *)` to header + impl.
- [x] Add `gvizLayerGraphSetEdgeHighlight(gvizLayerGraph *, size_t u, size_t v, int on)` that resolves the bit index via `edgeStartIdx[u] + indexOf(neighbors(u), v)`; for undirected graphs also sets the symmetric (v,u) bit.
- [x] Add `gvizLayerGraphClearEdgeHighlights(gvizLayerGraph *)` (memset edge bits to 0).
- [x] Add `gvizLayerGraphRebuildEdgeIndex(gvizLayerGraph *)` that recomputes `edgeStartIdx`, resizes `edgeHighlight` to the new bit count, and zeroes new bits — called after any topology mutation.

### VBO color-stream support

- [x] Extend `gvizGraphVBO` (`include/renderer/layers/gvizGraphVBO.h`, `src/renderer/layers/gvizGraphVBO.c`) with `unsigned int vboColors` (per-endpoint vec3 stream) and `float *colors` CPU mirror sized `2 * totalDirectedEdges * 3`.
- [x] In `rebuildEdges`, after creating the position VBO, allocate the color buffer (default = black per endpoint) and bind as vertex attribute 1 (`rlSetVertexAttribute(1, 3, RL_FLOAT, ...)`); enable attribute 1 in the VAO.
- [x] Replace the hardcoded black uniform draw path in `gvizGraphVBODraw` with a custom inline shader (singleton, in the style of the disc shader) that takes per-vertex color and emits it.
- [x] Add `gvizGraphVBOUploadEndpointColors(gvizGraphVBO *, const float *rgba2N)` that uploads via `rlUpdateVertexBuffer`. Call `glBufferSubData`-equivalent path so highlight changes do NOT require topology rebuild.
- [x] Release the colors VBO and CPU buffer in `gvizGraphVBORelease`.

### Wiring highlights into draw path

- [x] In `gvizLayerGraphDraw`, before calling `gvizGraphVBODraw`, walk `(u, j)` over directed neighbor pairs in the same order as `buildExpandedVerts` and write a color per endpoint: red `(1,0,0)` if `vertexHighlight[u]` set OR the edge bit at `edgeStartIdx[u]+j` is set; otherwise black. Upload via `gvizGraphVBOUploadEndpointColors`.
- [x] Track a `highlightDirty` flag on `gvizLayerGraph` so the color buffer is only rewritten when highlights or topology actually changed.

### Dynamic graph mutations

- [x] Add `gvizLayerGraphAddVertex(gvizLayerGraph *, const double *startPos)` — calls `gvizGraphAddVertex` on the underlying graph, grows the embedding's `vertexPositions` array, writes `startPos` into the new slot, grows `vertexHighlight` (one extra bit, zeroed), calls `gvizLayerGraphRebuildEdgeIndex`, and sets `gpuDirty = 2`.
- [x] Add `gvizLayerGraphRemoveVertex(gvizLayerGraph *, size_t v)` — removes incident edges, removes the vertex from `gvizGraph` (extend `gvizGraph` with a `gvizGraphRemoveVertex` if not present), compacts `vertexPositions` and `vertexHighlight`, rebuilds edge index, sets `gpuDirty = 2`.
- [x] Add `gvizLayerGraphAddEdge(gvizLayerGraph *, size_t u, size_t v)` — calls `gvizGraphAddEdge`, then `gvizLayerGraphRebuildEdgeIndex`, sets `gpuDirty = 2`.
- [x] Add `gvizLayerGraphRemoveEdge(gvizLayerGraph *, size_t u, size_t v)` — calls `gvizGraphRemoveEdge`, then `gvizLayerGraphRebuildEdgeIndex`, sets `gpuDirty = 2`.
- [x] After each mutation, request the owning embedding to incorporate the change: add a `void (*onTopologyChanged)(gvizEmbeddedGraph *)` hook on `gvizLayerGraph` (NULL = no-op). Tutte/PolyTutte layers set this to re-seed/re-fix; trees/GRIP can ignore.

### gvizGraph removal primitive (only if missing)

- [x] Verify `gvizGraphRemoveVertex` exists in `include/dsa/gvizGraph.h`; if not, add it (removes from vertex array, drops all incident edges, decrements neighbor indices that referenced shifted vertices). Implement in `src/dsa/gvizGraph.c`.

## Epic 2: Polyhedral Tutte re-embedding layer

New layer that runs Tutte on a 3-connected planar mesh, then on user input scans all faces (highlighting each as it goes) and re-embeds with the largest face as the outer boundary. Lives in `include/app/gvizLayerPolyTutte.h` and `src/app/gvizLayerPolyTutte.c`.

### Layer scaffolding

- [ ] Create `include/app/gvizLayerPolyTutte.h` defining `gvizLayerPolyTutte` (first member `gvizLayer layer`, then owned `gvizGraph graph`, `gvizTutteEmbedding tutte`, `gvizGraphVBO vbo`, plus state fields: `int phase` = {INITIAL, SCANNING, FINAL}, `size_t scanFaceIdx`, `size_t bestFaceIdx`, `double bestFaceArea`, `gvizArray faces` of `gvizArray<size_t>` per face).
- [ ] Declare `gvizLayerPolyTutteInit(gvizLayerPolyTutte *, gvizGraph *mesh, const size_t *outerTriangle, size_t z)`, `Draw`, `Update`, `Release`, `HandleEvent`, and `GVIZ_LAYER_POLY_TUTTE_VTABLE`.
- [ ] Implement init: clone graph, init Tutte with `outerTriangle` as the fixed boundary (radius matching existing demo), seed interior, init VBO with `EDGES | DISCS`, set `phase = INITIAL`.
- [ ] Implement Update: while `phase == INITIAL`, run `gvizTutteEmbeddingStep` until converged; while `phase == SCANNING`, advance one face per Update tick (so the user sees each highlighted face for one frame).
- [ ] Implement Draw mirroring `gvizLayerTutteDraw` (rebuild VBO on `gpuDirty >= 2`, upload positions on `== 1`, then draw).
- [ ] Implement Release: free faces array + inner arrays, release Tutte, VBO, graph.

### Rotation system extraction

- [ ] Add `void gvizComputeRotationSystem(gvizEmbeddedGraph *eg)` to a shared header (e.g. `include/renderer/embeddings/gvizPlanarRotation.h`) and `.c` file. For each vertex u, compute `atan2(p[v].y - p[u].y, p[v].x - p[u].x)` for each neighbor v, then sort u's `neighbors` array (in `gvizGraph`) clockwise (descending angle, or ascending depending on screen-y convention — pick clockwise relative to +x axis with y-down screen).
- [ ] Sort in place using `qsort` with a thread-unsafe context global or a comparator that reads positions from a file-static pointer set just before the call (consistent with project's no-`qsort_r` style).

### Face iteration

- [ ] Locate existing face-iteration utility in the codebase (search `include/renderer/embeddings/` and `src/renderer/embeddings/` for "face" / "rotation" — likely tied to planar embeddings). If one exists, use it. If not, add `gvizEnumerateFaces(gvizGraph *g, gvizArray *outFaces)` that, given a graph whose adjacency lists are already in clockwise rotation order, walks each directed half-edge once via the "next around face" rule (`next(u→v) = v→prev_in_rotation(v, u)`), accumulating vertex-cycles into `outFaces`.
- [ ] Add `static double polygonArea2D(const size_t *faceVerts, size_t n, gvizEmbeddedGraph *eg)` (shoelace formula) to `gvizLayerPolyTutte.c`.

### User-triggered largest-face scan

- [ ] In `HandleEvent`, on `KEY_SPACE` while `phase == INITIAL`: call `gvizComputeRotationSystem` on the embedded graph, run face enumeration into `self->faces`, set `phase = SCANNING`, `scanFaceIdx = 0`, `bestFaceArea = -inf`.
- [ ] In `Update` while `phase == SCANNING`: clear vertex/edge highlights via the new `gvizLayerGraph`-style API (Poly-Tutte will use the same highlight buffers — either inherit by reusing a `gvizLayerGraph` sub-struct, or duplicate the minimal highlight fields here). Set highlights for the current face's vertices and edges. Compute area; if larger than `bestFaceArea`, update best. Increment `scanFaceIdx`. When done, transition to FINAL.
- [ ] On `phase == FINAL`: clear highlights, release current Tutte state, re-init Tutte with `bestFace` vertices as the new outer boundary (use boundary radius matching original), re-seed interior, set `gpuDirty = 2`.

### Integration / scene builder

- [ ] Add a scene builder `gvizBuildPolyTutteDemoScene(gvizScene *out)` in `src/app/gvizSceneBuilders.c` and declare in `gvizSceneBuilders.h`. Build a small polyhedral mesh (e.g. octahedron or icosahedron edge-graph from existing utils, or a hand-coded 3-connected planar graph) and pass an initial outer triangle.
- [ ] Add a `GVIZ_MENU_DEMO_POLY_TUTTE` entry to the main menu enum and route it through `main.c`'s switch.
- [ ] Update `CMakeLists.txt` to include the new source files (`src/app/gvizLayerPolyTutte.c`, plus the rotation-system helper if placed in its own file).
