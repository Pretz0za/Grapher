# gviz

A C11 graph visualization and layout library. Graphs are built on hand-rolled
data structures (no external containers), and can be embedded into 2D
coordinates using several layout algorithms — from a large-graph GRIP
embedder to a Barnes-Hut force-directed layout with pluggable force models,
planar straight-line embeddings, and Reingold-Tilford-style barycentric
(Tutte) layouts.

## Layers

| Layer | Path | Contents |
|-------|------|----------|
| Data structures | `src/ds/`, `include/ds/` | `gvizArray` (type-erased dynamic array), `gvizGraph` (directed/undirected adjacency-list graph), `gvizDeque`, `gvizBitArray`, `gvizTree`, `gvizSubgraph` (vertex/edge-induced subgraph views), `gvizQuadtree` (Barnes-Hut spatial index) |
| Search algorithms | `src/algorithms/search/`, `include/algorithms/search/` | BFS, DFS, connected components, k-nearest-neighbor search |
| Embedding algorithms | `src/embedders/`, `include/embedders/` | See below |
| Utilities | `src/utils/`, `include/utils/` | Graph loading (`.edges`, `.gexf`, `.obj`), synthetic test graph generation, element serializers |
| Core | `src/core/`, `include/core/` | Allocation macros (`GVIZ_ALLOC`/`GVIZ_REALLOC`/`GVIZ_DEALLOC`), thread pool, small self-contained vector-math header |

Every embedding algorithm wraps a `gvizEmbeddedGraph` (a `gvizSubgraph` plus
an n-dimensional `double` position buffer) and exposes a common
action/stat-series interface (`gvizActionRegistry`, stat charts) so a
front-end can drive any embedder — trigger a step, toggle a setting, plot a
convergence series — without knowing which algorithm is underneath.

## Embedding algorithms

- **`gvizGRIPEmbedder`** — GRIP (maximal-independent-set filtration + k-nearest-neighbor springs). Scales to large graphs by embedding a coarse MIS hierarchy first and refining layer by layer.
- **`gvizForceEmbedder`** — general force-directed layout with Barnes-Hut quadtree repulsion (falls back to exact O(V²) all-pairs when disabled) and ForceAtlas2-style adaptive speed regulation. The force math itself is pluggable via `gvizForceModel`:
  - `GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD`
  - `GVIZ_FORCE_MODEL_LINLOG`

  Also supports optional gravity (constant pull toward the origin, useful for disconnected/unconnected graphs), and radius-aware "prevent overlap" repulsion that treats vertices as circles sized by degree and saturates at a bounded magnitude once they touch, instead of diverging.
- **`gvizTutteEmbedder`** — real-time Tutte barycentric embedding (interior vertices relax toward their neighbors' barycenter, boundary pinned), Jacobi or Gauss-Seidel.
- **`gvizSpringTutteEmbedder`** — second-order variant of the above driven by a damped harmonic oscillator instead of a direct position blend, so underdamped settings overshoot and settle rather than moving straight to equilibrium.
- **`gvizPlanarEmbedder`** — Boyer-Myrvold planarity testing (third-party, `third-party/boyerMyrvold/`) plus CCW rotation-system construction and a straight-line embedding; returns a Kuratowski subdivision witness on non-planar input.
- **`gvizSchnyderWood`** — Schnyder wood (realizer) decomposition of a triangulated planar graph into three directed trees; building block for planar straight-line drawings (in progress).
- **`gvizEmbeddedTree`** — Reingold-Tilford-style tree layout.

## Graph loading

`include/utils/graphLoader.h` loads graphs from:
- **`.edges`** — the Network Repository format (`u v` or `u v weight` per line, optional `%` comments, configurable directedness; external ids of any base are compacted to dense 0-based internal ids).
- **`.gexf`** — Graph Exchange XML Format; topology only (node/edge attributes and labels are ignored, edges treated as undirected).

Sample datasets live under `data/` (`data/sample`, `data/smoke`,
`data/les-miserables`, `data/human-jung-2015`, `data/dimacs10`,
`data/c-fat500`, `data/finan512`).

## Public interface

`#include "gviz.h"` pulls in the entire public API; the individual headers
under `include/` can also be included piecemeal. Everything declared under
`include/` is public; internal headers living next to the sources under
`src/` (e.g. `src/embedders/gvizGRIPInternal.h`, the GRIP MIS-filtration
internals used by white-box tests and benches) are private and may change
without notice.

Layering rules (each layer may only depend on layers above it):

| Layer | Depends on | Contents |
|-------|-----------|----------|
| `core/` | — | alloc macros, vector math, thread pool |
| `ds/` | core | array, bitarray, deque, graph, subgraph, tree, quadtree |
| `algorithms/` | ds, core | BFS, DFS, connected components, KNN over `gvizSubgraph` |
| `embedders/` | all above | the layout algorithms; `gvizEmbeddedGraph` is the shared base and knows nothing about any specific embedder |
| `utils/` | ds | file loaders, synthetic graph builders, serializers — never embedders |

Planar-specific queries on an embedded graph (planarity + rotation-system
install, incremental face search, face-at-point picking) live in
`embedders/gvizPlanarEmbedder.h` (`gvizPlanarApplyRotationToEmbedding`,
`gvizPlanarFaceSearch*`, `gvizPlanarFaceSubgraphAt`), not on the base
`gvizEmbeddedGraph`. GRIP is driven either one-shot
(`gvizGRIPEmbedderEmbed`) or manually
(`gvizGRIPEmbedderBegin` / `gvizGRIPEmbedderRefineRound` /
`gvizGRIPEmbedderNextStage`).

## Build

```bash
mkdir -p build && cd build
cmake ..
make
```

Enable AddressSanitizer for memory debugging:
```bash
cmake .. -DENABLE_ASAN=ON
```

`graphvis` (the static library) links against `planar` (the vendored
Boyer-Myrvold implementation), `m`, and a system thread pool
(`Threads::Threads`) — no other external dependencies.

## Testing

```bash
# Run all tests
cd build && ctest

# Run a single test executable
./build/tests/ds/gvizArrayTests
./build/tests/ds/gvizGraphTests
./build/tests/ds/gvizDequeTests
./build/tests/ds/gvizBitArrayTests
./build/tests/ds/gvizSubgraphTests
./build/tests/ds/gvizQuadtreeTests
./build/tests/algorithms/search/gvizSearchTests
./build/tests/embedders/gvizGRIPTests
./build/tests/embedders/gvizPlanarTests
./build/tests/embedders/gvizTutteTests
./build/tests/embedders/gvizSpringTutteTests
./build/tests/embedders/gvizForceEmbedderTests
```

Tests use the Unity framework (`third-party/unity/`).

### Benchmarks and debug tools

A handful of `tests/embedders/` targets are standalone benchmarking/probing
tools rather than Unity test suites (not registered with `ctest`):

- `gvizGRIPKBench` — sweeps GRIP's k-nearest-neighbor policy/parameters.
- `gvizGRIPLayerProbe` — inspects per-layer MIS filtration state.
- `gvizGRIPMigrateBench` — benchmarks layer-to-layer vertex migration.
- `gvizGRIPFiltrationDebug` — verbose filtration/BFS logging driver (see `GVIZ_GRIP_DEBUG_FILTRATION` below).
- `gvizGRIPPlaceBench` — benchmarks placement + KNN update timing per stage.
- `gvizTuttePickTests` — interactive/manual Tutte picking driver.

## Debug and profiling environment variables

These variables are opt-in. Set them to any non-empty value before running a GRIP
embedder, demo, or benchmark. They add overhead when enabled and write to
`stderr` unless noted otherwise.

| Variable | Component | Description |
|----------|-----------|-------------|
| `GVIZ_GRIP_DEBUG_FILTRATION` | GRIP embedder | Logs MIS filtration and radius-BFS details during `gvizGRIPEmbedderBegin`. |
| `GVIZ_GRIP_STAGE_TIMING` | GRIP embedder | Times each `gvizGRIPEmbedderNextStage` call, split into `placeLayerVertices` and `updateKNNs`. |
| `GVIZ_KNN_PROFILE` | K-nearest search | Counts BFS nodes visited per `gvizSearchKNearestScratch` query (placement and refinement). |

### `GVIZ_GRIP_DEBUG_FILTRATION`

Enables verbose MIS filtration logging in `src/embedders/gvizGRIPEmbedder.c`.

**Filtration layers** (`iterMISFiltration`), when layer index `>= 6` or the layer
has `<= 1100` vertices:

```
[grip-filt] layer=<i> radius=<r> in=<n> picked=<p> skipped=<s> removed=<rm> out=<o> continue=<c>
```

**Radius BFS** (`verticesWithinRadius`), for the first three calls with
`maxDepth >= 64`:

```
[grip-bfs] src=<v> maxDepth=<d> visited=<n> marked=<m> pushFail=<f> queuePeak=<cap>
```

On deque push failure:

```
[grip-bfs] push fail src=<v> depth=<d> queue=<count> cap=<capacity>
```

Example:

```bash
GVIZ_GRIP_DEBUG_FILTRATION=1 ./build/tests/embedders/gvizGRIPFiltrationDebug \
  data/human-jung-2015/data.edges 3
```

### `GVIZ_GRIP_STAGE_TIMING`

Prints wall-clock timing for each `gvizGRIPEmbedderNextStage` (finer GRIP layer transition):

```
[grip-stage] placeLayerVertices: <seconds>
[grip-stage] updateKNNs: <seconds>
```

Example:

```bash
GVIZ_GRIP_STAGE_TIMING=1 ./build/tests/embedders/gvizGRIPPlaceBench sierpinski 3 2 9
```

### `GVIZ_KNN_PROFILE`

Instruments `gvizSearchKNearestScratch` in `src/algorithms/search/gvizKNearest.c`.
Each query increments:

- **queries** — number of KNN searches
- **visited** — total BFS nodes touched (sum over queries)
- **maxVisited** — largest single-query visit count

Counters are thread-safe and accumulate until reset via `gvizKNNProfileReset()`
(see `include/algorithms/search/gvizKNearest.h`). Read them with
`gvizKNNProfileSnapshot()`.

The `gvizGRIPPlaceBench` tool resets before each stage and prints a summary line
when this variable is set:

```
| knn q=<queries> avgVisit=<mean> maxVisit=<max>
```

Example:

```bash
GVIZ_KNN_PROFILE=1 GVIZ_GRIP_STAGE_TIMING=1 \
  ./build/tests/embedders/gvizGRIPPlaceBench edges 3 2 data/human-jung-2015/data.edges 64
```

### Combining variables

All three are independent and can be set together:

```bash
export GVIZ_GRIP_DEBUG_FILTRATION=1
export GVIZ_GRIP_STAGE_TIMING=1
export GVIZ_KNN_PROFILE=1
```

Useful when comparing graphs (e.g. Sierpinski vs a `.edges` dataset on the
largest connected component) to see whether time is spent in filtration, layer
placement, or per-query KNN BFS cost.
