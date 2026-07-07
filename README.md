C based Graph visualization

## Debug and profiling environment variables

These variables are opt-in. Set them to any non-empty value before running a GRIP
embedder, demo, or benchmark. They add overhead when enabled and write to
`stderr` unless noted otherwise.

| Variable | Component | Description |
|----------|-----------|-------------|
| `GVIZ_GRIP_DEBUG_FILTRATION` | GRIP embedder | Logs MIS filtration and radius-BFS details during `gvizGRIPEmbedderBegin` / `createMISFiltration`. |
| `GVIZ_GRIP_STAGE_TIMING` | GRIP embedder | Times each `beginNewStage` call, split into `placeLayerVertices` and `updateKNNs`. |
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

Prints wall-clock timing for each `beginNewStage` (finer GRIP layer transition):

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
