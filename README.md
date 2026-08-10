# gviz

A C++20 graph visualization and layout library. Graphs are built on
`gviz::Graph`/`gviz::Subgraph` (adjacency-list, degree-proportional
operations), and can be embedded into 2D/3D/4D coordinates using several
layout algorithms — from a large-graph GRIP embedder to a Barnes-Hut
force-directed layout with pluggable force models, planar straight-line
embeddings, and Reingold-Tilford/Tutte-style tree and barycentric layouts.

Everything lives in the `gviz` namespace; `#include "gviz.hpp"` pulls in the
whole public API, or include the individual headers under `include/`
piecemeal.

## Layers

| Layer | Path | Contents |
|-------|------|----------|
| `core` | `include/`, `src/` | `ThreadPool`, `Vec.hpp` (small vector-math helpers) |
| `ds` | `include/`, `src/` | `Graph` (directed/undirected adjacency-list graph), `Subgraph` (vertex/edge-induced views), `BitSet`, `QuadTree` (Barnes-Hut spatial index) |
| `search` | `include/`, `src/` | `BreadthFirst`, `DepthFirst`, `ConnectedComponents`, `IsTree`/`IsLeaf`/`CountLeaves`, `KNearest` |
| `layout` | `include/`, `src/` | See below |
| `io` / `graphs` | `include/`, `src/` | `GraphLoader` (`.edges`/`.gexf`/`.obj`), `Graphs` (synthetic test-graph generators) |

Each layer depends only on the layers above it — see `include/gviz.hpp`'s own
doc comment and `CLAUDE.md` for the full architecture write-up, including the
`Graph`/`Subgraph` performance design, the sync/commit machinery behind
dynamic (growing-while-rendering) graphs, and the actions/stat-series
interface every embedder exposes generically.

Every embedder publicly inherits from `gviz::layout::EmbeddedGraph` (a
`Subgraph` plus an n-dimensional position buffer, plus generic
actions/stat-series/draw-mask registries) so a front-end can drive any of
them — trigger a step, toggle a setting, plot a convergence series — without
knowing which algorithm is underneath.

## Embedding algorithms

- **`gviz::layout::GRIP`** — maximal-independent-set filtration + k-nearest-neighbor springs. Scales to large graphs by embedding a coarse MIS hierarchy first and refining layer by layer. 2D, 3D, or 4D.
- **`gviz::layout::ForceAtlas`** — general force-directed layout with Barnes-Hut quadtree repulsion (falls back to exact O(V²) all-pairs when disabled) and ForceAtlas2-style adaptive speed regulation. The force math itself is pluggable via `gviz::layout::ForceModel`:
  - `gviz::layout::FruchtermanReingold`
  - `gviz::layout::LinLog`

  Also supports optional gravity (constant pull toward the origin, useful for disconnected/unconnected graphs), and radius-aware "prevent overlap" repulsion that treats vertices as circles sized by degree and saturates at a bounded magnitude once they touch, instead of diverging.
- **`gviz::layout::Tutte`** — real-time Tutte barycentric embedding (interior vertices relax toward their neighbors' barycenter, boundary pinned), Jacobi or Gauss-Seidel.
- **`gviz::layout::SpringTutte`** — second-order variant of the above driven by a damped harmonic oscillator instead of a direct position blend, so underdamped settings overshoot and settle rather than moving straight to equilibrium. Structurally mirrors `Tutte` — same method names, same call order — differing only where the physics genuinely differs.
- **`gviz::layout::Planar`** — Boyer-Myrvold planarity testing (third-party, `third-party/boyerMyrvold/`) plus CCW rotation-system construction and a straight-line embedding; throws `PlanarNotPlanarError` (carrying a Kuratowski subdivision witness) on non-planar input. Also owns the free-function planarity API used by any embedder: face walking (`FaceWalk`, a real C++ range), face enumeration (`FaceEnumerator`), triangulation, face-at-point picking.
- **`gviz::layout::SchnyderWood`** — Schnyder wood (realizer) decomposition of a triangulated planar graph into three directed trees; building block for planar straight-line drawings. Its `Embed()` ports a known-incomplete algorithm faithfully (see the header doc) — its construction (canonical ordering) is separately verified correct.
- **`gviz::layout::ReingoldTilford`** — classic tidy tree layout. One-shot, not iterative: `RTInit` → `CalculateOffsets` → `Embed`.

## Graph loading and generation

`include/GraphLoader.hpp` (`gviz::io`) loads graphs from:
- **`.edges`** — the Network Repository format (`u v` or `u v weight` per line, optional `%` comments, configurable directedness; external ids of any base are compacted to dense 0-based internal ids).
- **`.gexf`** — Graph Exchange XML Format; topology plus per-vertex JSON-string attributes (label + `<attvalues>`).
- **`.obj`** — Wavefront OBJ; `v` lines become vertices, `f` face loops become edges.

`include/Graphs.hpp` (`gviz::graphs`) has synthetic generators (Sierpinski
triangle/tetrahedron/carpet, tetrahedral/rectangular/triangular mesh, Möbius
strip, Klein bottle, random connected graphs) used throughout the test suite
and benches, plus `IsConnected(const Graph&)`.

Sample datasets live under `data/` (`data/sample`, `data/smoke`,
`data/les-miserables`, `data/human-jung-2015`, `data/dimacs10`,
`data/c-fat500`, `data/finan512`, `data/hubs`).

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

`gviz` (the static library) links against `planar` (the vendored
Boyer-Myrvold implementation, still C), `m`, and a system thread pool
(`Threads::Threads`) — no other external dependencies.

## Testing

```bash
# Run all tests
cd build && ctest

# Run a single test executable
./build/tests/ds/GraphTests
./build/tests/ds/SubgraphTests
./build/tests/search/KNearestTests
./build/tests/layout/GRIPTests
./build/tests/layout/ReingoldTilfordTests
./build/tests/layout/ReingoldTilfordTraceTests
./build/tests/layout/PlanarTests
./build/tests/layout/TutteTests
./build/tests/layout/SpringTutteTests
./build/tests/layout/ForceAtlasTests
./build/tests/utils/GraphLoaderTests
./build/tests/utils/GraphsTests
```

Tests use the Unity framework (`third-party/unity/`), compiled as C++.

### Benchmarks and debug tools

A handful of `tests/` targets are standalone benchmarking/probing tools
rather than Unity test suites (not registered with `ctest`, except
`TuttePick`, which returns a real pass/fail exit code):

- `GRIPKBench` — sweeps GRIP's k-nearest-neighbor policy/parameters.
- `GRIPLayerProbe` — inspects per-layer MIS filtration state.
- `GRIPMigrateBench` — benchmarks layer-to-layer vertex migration (production bounded-BFS vs. a historical O(V) baseline).
- `GRIPFiltrationDebug` — verbose filtration/BFS logging driver (see `GVIZ_GRIP_DEBUG_FILTRATION` below).
- `GRIPPlaceBench` — benchmarks placement + KNN update timing per stage.
- `TuttePick` — interactive/manual Tutte face-picking driver (uses `gviz::layout::FaceSubgraphAt`).

These drive `gviz::layout::GRIP`/`Tutte` through a small set of narrow,
clearly-scoped debug/introspection accessors (`GRIP::LayerBorder`,
`GRIP::FiltrationVertexAt`, `GRIP::Displacement`, `GRIP::DebugXxx` for
`GRIPMigrateBench`'s checkpoint/restore comparison) rather than reaching into
private state — see `include/GRIP.hpp`'s "DEBUG / INTROSPECTION" section.

## Debug and profiling environment variables

These variables are opt-in. Set them to any non-empty value before running a
GRIP embedder, demo, or benchmark. They add overhead when enabled and write
to `stderr` unless noted otherwise.

| Variable | Component | Description |
|----------|-----------|-------------|
| `GVIZ_GRIP_DEBUG_FILTRATION` | GRIP embedder | Logs MIS filtration and radius-BFS details during `GRIP::Begin()`. |
| `GVIZ_GRIP_STAGE_TIMING` | GRIP embedder | Times each `GRIP::NextStage()` call, split into `placeLayerVertices` and `updateKNNs`. |
| `GVIZ_KNN_PROFILE` | K-nearest search | Counts BFS nodes visited per `gviz::search::KNearest` query (placement and refinement). |

### `GVIZ_GRIP_DEBUG_FILTRATION`

Enables verbose MIS filtration logging in `src/GRIP.cpp`.

**Filtration layers**, when layer index `>= 6` or the layer has `<= 1100`
vertices:

```
[grip-filt] layer=<i> radius=<r> in=<n> picked=<p> skipped=<s> removed=<rm> out=<o> continue=<c>
```

**Radius BFS**, for the first three calls with `maxDepth >= 64`:

```
[grip-bfs] src=<v> maxDepth=<d> visited=<n> marked=<m> pushFail=<f> queuePeak=<cap>
```

`pushFail` is always `0` in the C++ port — `std::deque` throws `bad_alloc`
on allocation failure rather than returning a checked push failure, so
there's no partial-push state to report. `queuePeak` reports the real
maximum BFS-frontier size reached, rather than an internal deque's capacity
(`std::deque` doesn't expose one).

Example:

```bash
GVIZ_GRIP_DEBUG_FILTRATION=1 ./build/tests/layout/GRIPFiltrationDebug \
  data/human-jung-2015/data.edges 3
```

### `GVIZ_GRIP_STAGE_TIMING`

Prints wall-clock timing for each `GRIP::NextStage()` (finer GRIP layer
transition):

```
[grip-stage] placeLayerVertices: <seconds>
[grip-stage] updateKNNs: <seconds>
```

Example:

```bash
GVIZ_GRIP_STAGE_TIMING=1 ./build/tests/layout/GRIPPlaceBench sierpinski 3 2 9
```

### `GVIZ_KNN_PROFILE`

Instruments `gviz::search::KNearest` in `src/KNearest.cpp`. Each query
increments:

- **queries** — number of KNN searches
- **visited** — total BFS nodes touched (sum over queries)
- **maxVisited** — largest single-query visit count

Counters are thread-safe and accumulate until reset via
`gviz::search::KnnProfileReset()` (see `include/KNearest.hpp`). Read them
with `gviz::search::KnnProfileSnapshot()`.

The `GRIPPlaceBench` tool resets before each stage and prints a summary line
when this variable is set:

```
| knn q=<queries> avgVisit=<mean> maxVisit=<max>
```

Example:

```bash
GVIZ_KNN_PROFILE=1 GVIZ_GRIP_STAGE_TIMING=1 \
  ./build/tests/layout/GRIPPlaceBench edges 3 2 data/human-jung-2015/data.edges 64
```

### Combining variables

All three are independent and can be set together:

```bash
export GVIZ_GRIP_DEBUG_FILTRATION=1
export GVIZ_GRIP_STAGE_TIMING=1
export GVIZ_KNN_PROFILE=1
```

Useful when comparing graphs (e.g. Sierpinski vs a `.edges` dataset on the
largest connected component) to see whether time is spent in filtration,
layer placement, or per-query KNN BFS cost.
