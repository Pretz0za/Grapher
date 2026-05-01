# TASKS — gvizGraphView refactor

Goal: introduce a non-materializing `gvizGraphView` over `gvizGraph`, route all
read access / embedding / traversal / GUI through views, and add a left-panel
graph/view tree to the scene.

Key files touched, by area:
- DSA: `include/dsa/gvizGraph.h`, `src/dsa/gvizGraph.c`,
        `include/dsa/gvizBitArray.h` (+ new `src/dsa/gvizBitArray.c`),
        `tests/dsa/gvizGraphTests.c`, `tests/dsa/gvizBitArrayTests.c`
- New view module: `include/dsa/gvizGraphView.h`,
        `src/dsa/gvizGraphView.c` (new), `tests/dsa/gvizGraphViewTests.c` (new)
- Embeddings: `include/renderer/embeddings/gvizEmbeddedGraph.h`,
        `src/renderer/embeddings/gvizEmbeddedGraph.c`,
        `gvivGRIPEmbedding.h` + `gvizGRIPEmbedding.c`,
        `gvizTutteEmbedding.h` + `gvizTutteEmbedding.c`,
        `gvizEmbeddedTree.h/.c`, `gvizForceDirected.h/.c`,
        `gvizPlanarEmbedding.h/.c`, `gvizSchnyderWood.h/.c`
- Renderer / layers: `include/renderer/layers/gvizLayerGraph.h`,
        `src/renderer/layers/gvizLayerGraph.c`,
        `gvizGraphVBO.h/.c`, `gvizVertexDiscVBO.h/.c`
- App layers: `src/app/gvizLayerGRIP.c`, `gvizLayerGRIPLive.c`,
        `gvizLayerTutte.c`, `gvizLayerPolyTutte.c`, `gvizLayerCreate.c`,
        `gvizSceneBuilders.c`
- Scene: `include/core/gvizScene.h`, `src/core/gvizScene.c`
- New scene panel: `include/app/gvizLayerGraphTree.h`,
        `src/app/gvizLayerGraphTree.c` (new)

---

## Epic 1: gvizBitArray iterator + ergonomics

- [x] Saga: Convert `gvizBitArray` from header-only macros to a real module —
      add `src/dsa/gvizBitArray.c`, keep existing macros, add helpers
      `gvizBitArrayAlloc(nbits)`, `gvizBitArrayFree`, `gvizBitArrayClone`,
      `gvizBitArrayCount`, `gvizBitArrayClearAll` so callers stop hand-rolling
      `GVIZ_ALLOC + memset`. Update CMake source list.
- [x] Saga: Add `gvizBitArrayIter` struct + API
      `gvizBitArrayIterInit(iter, src, nbits)` (copies the bit buffer),
      `gvizBitArrayIterNext(iter, size_t *out)` (find lowest set bit, clear it,
      return 1 or 0 when exhausted), `gvizBitArrayIterRelease(iter)`. Use
      `__builtin_ctz`/`ctzll` per word for O(set-bit) cost.
- [x] Saga: Tests in `tests/dsa/gvizBitArrayTests.c` — empty bitarray, sparse,
      dense, off-the-end nbits not multiple of 8, double-iteration on copy
      (source unchanged).

## Epic 2: gvizGraphView core

- [x] Saga: Define `gvizGraphView` in `include/dsa/gvizGraphView.h`:
      ```
      typedef struct gvizGraphView {
        gvizGraph *graph;            /* borrowed */
        GVIZ_BIT_ARRAY vertexMask;   /* NULL = all vertices */
        GVIZ_BIT_ARRAY edgeMask;     /* NULL = all edges */
        size_t *edgeStart;           /* len N+1, prefix sums into edgeMask */
        size_t count;                /* vertices currently in view */
      } gvizGraphView;
      ```
      Document the invariants: NULL/NULL = whole graph; vertex-only =
      induced subgraph; edge-only = edge-defined subgraph.
- [x] Saga: `gvizGraphViewInitFull(view, graph)` — both masks NULL,
      `count = graph->vertices.count`, `edgeStart` lazily NULL.
- [x] Saga: `gvizGraphViewInitFromVertices(view, graph, GVIZ_BIT_ARRAY mask)`
      — borrow graph, allocate `vertexMask` copy, derive `count`, leave
      `edgeMask` NULL. Build `edgeStart` prefix sums lazily on first edge query.
- [x] Saga: `gvizGraphViewInitFromEdges(view, graph, edgeMask)` — borrow
      edgeMask (clone), build `edgeStart`, derive `vertexMask` as union of
      endpoints (or leave NULL if edge-only semantics).
- [x] Saga: `gvizGraphViewRelease(view)` — frees masks and edgeStart only;
      never touches `graph`.

## Epic 3: gvizGraphView read interface

- [x] Saga: `gvizGraphViewVertexCount(view)`,
      `gvizGraphViewVertexInView(view, u)`,
      `gvizGraphViewEdgeInView(view, u, v)` — pure mask checks.
- [x] Saga: Neighbor iteration that respects masks —
      `gvizGraphViewNeighborsIter` struct + Init/Next, returning only
      neighbors `v` where both `vertexMask[v]` and the corresponding
      directed-edge bit (if edgeMask set) are present. Built on top of the
      underlying `gvizGraphGetVertexNeighbors`; never materializes a new array.
- [x] Saga: `gvizGraphViewVertexIter` — iterate vertex indices in the view
      using `gvizBitArrayIter` when mask present, else simple counter.
- [x] Saga: `gvizGraphViewDegree(view, u)` (counts iter), and
      `gvizGraphViewEdgeExists` for the masked sense.
- [x] Saga: Tests in `tests/dsa/gvizGraphViewTests.c` covering all four mask
      modes (full, vertex-only, edge-only, both).

## Epic 4: gvizGraphView mutation interface

- [x] Saga: `gvizGraphViewAddVertex(view, u)` /
      `gvizGraphViewRemoveVertex(view, u)` — flip bits in `vertexMask`
      (allocating it lazily if currently NULL), update `count`, mark
      `edgeStart` stale.
- [x] Saga: `gvizGraphViewAddEdge(view, u, v)` /
      `gvizGraphViewRemoveEdge(view, u, v)` — flip the directed-edge bit
      computed via `edgeStart[u] + indexOf(neighbors(u), v)`. Allocate
      `edgeMask` lazily.
- [x] Saga: `gvizGraphViewRebuildEdgeStart(view)` — recompute prefix sums
      after underlying graph topology change; called when stale flag set.
- [x] Saga: Tests covering add/remove flipping bits and `count`/`edgeStart`
      consistency.

## Epic 5: BFS / DFS returning views

- [x] Saga: Add `gvizGraphDFSView(graph, source, gvizGraphView *out)` to
      `gvizGraph.h/.c` — runs DFS, builds a vertex+edge mask of reached
      vertices and tree edges, returns the view (no new graph).
- [x] Saga: Add `gvizGraphBFSView(graph, source, maxDepth, gvizGraphView *out)`
      — same but BFS-tree edges only.
- [x] Saga: Mark `gvizGraphDFSTree` / `gvizGraphBFSTree` legacy in the header
      (keep them — Reingold-Tilford uses tree shape). Don't delete.
- [x] Saga: Tests for both view-producing variants.

## Epic 6: gvizEmbeddedGraph holds a view

- [x] Saga: Refactor struct in `gvizEmbeddedGraph.h`:
      ```
      typedef enum { GVIZ_EMBED_FULL_GRAPH, GVIZ_EMBED_VIEW_ONLY }
        gvizEmbeddingMode;
      typedef struct gvizEmbeddedGraph {
        gvizGraphView view;
        gvizEmbeddingMode mode;
        gvizEmbedding embedding;  /* sized to view (Mode B) or full N (Mode A) */
      } gvizEmbeddedGraph;
      ```
      Note: kept `gvizGraph *graph` as a borrowed alias of `view.graph` for
      backwards-compat with existing callers; will be removed in Epic 11
      cleanup once Epic 7+ has migrated the algorithms onto views.
- [x] Saga: New init signature
      `gvizEmbeddedGraphInitView(eg, view, mode, dim)`. Keep a thin compat
      `gvizEmbeddedGraphInit(eg, graph, dim)` that builds a Full view + Mode A.
- [x] Saga: Position accessors must respect mode. In Mode A,
      `vertexPositions[u * dim]` is indexed by raw vertex id. In Mode B, by
      view-local index obtained from a `view→local` lookup table built on
      init. Add `gvizEmbeddedGraphLocalIndex(eg, u)` helper.
- [x] Saga: Update `gvizEmbeddedGraphRelease` to release the view (which only
      frees masks, never the graph) and the local-index table if any.
- [x] Saga: Update serializers (`gvizEmbeddedGraphSaveEmbedding/Load`) to
      write/read view metadata so reload restores Mode A vs B correctly.

## Epic 7: Refactor embedding algorithms onto views

- [x] Saga: GRIP (`gvivGRIPEmbedding.h`, `gvizGRIPEmbedding.c`) — replace
      `gvizGraph *graph` parameter with `gvizGraphView *view`. Drop the
      stand-alone `dispCalculated` bitarray that mirrors view membership
      (subsumed by view masks). All neighbor walks go through
      `gvizGraphViewNeighborsIter`. `gvizGRIPPrepareLayerKNNs` and
      `placeLayerVertices` use view APIs instead of raw graph.
- [x] Saga: Tutte (`gvizTutteEmbedding.h/.c`) — accept view, build M_II /
      `interiorIdx` / `vertexToInterior` against the view's vertex space.
- [ ] Saga: Reingold-Tilford / `gvizEmbeddedTree.h/.c` — accept a view that
      represents the tree (root vertex still passed separately).
- [ ] Saga: Force-directed (`gvizForceDirected.h/.c`) — accept view; iterate
      vertices via view iter so excluded vertices don't contribute forces.
- [ ] Saga: Planar (`gvizPlanarEmbedding.h/.c`) — accept view; the cached
      Kuratowski subdivision continues to live in the planar state struct
      and is keyed off the same view.
- [ ] Saga: Schnyder wood (`gvizSchnyderWood.h/.c`) — accept view (stub-level
      change; algorithm is in progress upstream).
- [ ] Saga: Update `gvizGraphKNearestNeighbors` to take a view (replaces the
      `filter` bitarray parameter — view encodes both membership and graph).

## Epic 8: Render layer wired through views (two modes)

- [ ] Saga: Update `gvizLayerGraph` to consume `gvizEmbeddedGraph` whose
      embedding may be view-sized (Mode B) or full-graph (Mode A). Edge VBO
      build (`gvizGraphVBO.c::buildExpandedVerts`) walks
      `gvizGraphViewNeighborsIter` and emits only edges in the view.
- [ ] Saga: Vertex disc VBO (`gvizVertexDiscVBO.c`) — emit one disc per
      vertex in the view, looking up positions through
      `gvizEmbeddedGraphGetVPosition` (mode-aware).
- [ ] Saga: Edge highlight bit indexing in `gvizLayerGraph` — replace the
      hand-rolled `edgeStartIdx` / `edgeHighlight` pair with the view's
      `edgeStart` + a parallel highlight bit array (separate concern from
      view membership). `gvizLayerGraphRebuildEdgeIndex` becomes
      `gvizGraphViewRebuildEdgeStart` + reallocation of highlight buffer.
- [ ] Saga: `gvizLayerGraphAddVertex/RemoveVertex/AddEdge/RemoveEdge` keep
      mutating the underlying registered graph (scene-shared) but now must
      also update or invalidate any views attached. Notify subscribers via
      existing `gvizSceneNotifyGraphChanged` so other layers refresh their
      views.

## Epic 9: Scene graph/view registry + left panel GUI

- [ ] Saga: Extend `gvizSceneGraphEntry` with `gvizArray views` (each entry:
      `{ gvizGraphView *view; gvizLayer *layer; const char *name; }`).
      Add registry API: `gvizSceneRegisterView(scene, handle, view, layer,
      name)`, `gvizSceneUnregisterView`, `gvizSceneGetGraphViews(handle)`.
- [ ] Saga: Bump `GVIZ_SCENE_MARGIN_L` to make room (e.g. 240px) and
      teach `gvizSceneComputeRegion` so component-layer slot layout starts
      after the panel.
- [ ] Saga: New `gvizLayerGraphTree` screen-space layer
      (`include/app/gvizLayerGraphTree.h`, `src/app/gvizLayerGraphTree.c`):
      full-height left strip, raygui-driven collapsible tree. Top level =
      registered graphs (name + vertex count). Children = views of that
      graph (name + vertex count + which `gvizLayer*` they're rendered on).
- [ ] Saga: Hook the tree layer into scene init (`gvizSceneInit2D/3D/Empty`)
      and re-layout on graph register/unregister + view register/unregister.
- [ ] Saga: When layer creation panel (`gvizLayerCreate.c`) builds a new
      graph layer, auto-register a default Full-graph view for the new
      `gvizLayerGraph` so the panel sees it immediately.
- [ ] Saga: Click handling in the tree — selecting a view sets the matching
      `gvizLayer` as the scene's `activeLayer` (existing API). Right-click
      a view: stub menu hooks (rename / delete / change embedding) — wire
      "delete view" only; embedding swap goes through existing context menu.

## Epic 10: Codebase sweep — replace ad-hoc graph+bitarray patterns

- [ ] Saga: Audit. Grep for callers passing both a `gvizGraph *` and a
      `GVIZ_BIT_ARRAY` filter side-by-side. Known sites:
      - `gvizGRIPState.dispCalculated`
      - `gvizGraphKNearestNeighbors(... filter)`
      - `gvizGRIPPrepareLayerKNNs(state, layer, placed)`
      - `placeLayerVertices(state, layer, placedVertices)`
      Produce a checklist of every additional site found.
- [ ] Saga: Replace each found site with a `gvizGraphView` parameter. Where
      the bitarray meant "scratch / progress" (e.g. `placedVertices`) and
      isn't really a view, leave it but document why; the test is whether
      its membership is queried as part of a graph walk.
- [ ] Saga: Final compile + test sweep. Run `ctest`, `gripDemo`, `treeDemo`
      and confirm the Tutte live-create flow still works end to end.

## Epic 11: Documentation + cleanup

- [ ] Saga: Update `CLAUDE.md` "Key types" block to reflect
      `gvizGraphView` and the view-aware `gvizEmbeddedGraph`.
- [ ] Saga: Delete dead helpers replaced by views (any `gvizGraphCopy*` /
      filter-driven helpers that no longer have callers — confirm via grep
      first; do not refactor speculatively).

## Out of scope

- No new embedding algorithms.
- Not introducing tests beyond the ones explicitly listed above for the new
  view + bitarray-iter modules (those are required to lock in the contract).
- Not touching planar third-party internals beyond the wrapper signature.
- Not changing tree (de)serialization formats.
