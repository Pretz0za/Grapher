# CLAUDE.md — gviz

Guidance for agents working in this repository's code (not just consuming it
as a library — see the `grapher`/`gviz-plumber` agent split below for that
use case).

## What this is

gviz is a C++20 graph **layout** library: classes for directed/undirected
graphs, search algorithms over them, and several embedding (layout)
algorithms that turn a graph into 2D/3D/4D vertex coordinates. It has no
rendering code and no notion of pixels, cameras, or GPUs — that lives in a
sibling project, **grender** (`~/grender`), a WebGPU renderer that consumes
gviz's public output types (`gviz::layout::EmbeddedGraph`, `gviz::Subgraph`)
and nothing else. The two are deliberately decoupled: grender never depends
on which embedder produced a set of positions, and gviz never depends on
grender. If you find yourself wiring renderer-specific state into an
embedder, or embedder-specific knowledge into the base `EmbeddedGraph`,
that's a layering violation — flag it before writing it.

Everything under `include/` is public API, all inside the `gviz` namespace
(nested `gviz::layout`/`gviz::search`/`gviz::io`/`gviz::graphs` for grouped
systems, per the class-vs-namespace convention below). Everything under
`src/` that isn't a public header's implementation is private and may change
without notice — `GRIP`'s MIS-filtration/KNN internals, for instance, are
private class members, not a separate internal header, precisely so nothing
outside `GRIP.cpp` can depend on them (the old C version's white-box test
access via a shared internal header was the thing being eliminated).

`include/gviz.hpp` is the umbrella header (`#include "gviz.hpp"` pulls in
the whole public API); individual headers under `include/` can also be
included piecemeal. `third-party/boyerMyrvold/` is vendored C — it stays C
and is linked into, not part of, gviz.

## Namespace and class conventions

A standalone object is a class directly in `gviz` (`Graph`, `Subgraph`,
`BitSet`, `QuadTree`, `ThreadPool`). A *grouped system* gets its own nested
namespace: `gviz::layout` (the embedders and their shared base/strategy
types), `gviz::search` (BFS/DFS/ConnectedComponents/KNearest/tree
predicates — free functions over `Subgraph`, not a class, since there's no
owned state), `gviz::io` (graph loaders), `gviz::graphs` (synthetic
generators).

**Fallible construction throws.** Anything that would have returned a
negative error code from an `Init`/`Begin`-style C function because the
operation fundamentally cannot proceed (non-planar graph handed to Tutte,
wrong embedding dimension, not a tree, insufficient vertices for GRIP's
dimension) throws directly from the constructor — `gviz::LayoutError` (in
`Error.hpp`) is the common base, with `NotPlanarError`, `NotATreeError`,
`DimensionError`, `InsufficientVerticesError`, `NoLayoutError` as concrete
subclasses (plus `Planar.hpp`'s `PlanarNotPlanarError : public
NotPlanarError`, which additionally carries a Kuratowski subdivision witness
graph). If a constructor doesn't throw, the object is valid, full stop — no
factory function, no `std::optional`/`std::expected` indirection.
Allocation failure needs no bespoke type: `std::vector`/`new` already throw
`std::bad_alloc`. Pure I/O/parse failures (`gviz::io`'s file loaders) throw
plain `std::runtime_error` instead — `LayoutError` is semantically for
graph-structural/dimensional failures, not "the file doesn't exist."

**Lookup-style queries stay pointer-based, not `std::optional`.** Things
like `FindAction`/`FindStatSeries` (not found → `nullptr`) keep returning a
nullable pointer — "not found" is a routine, common query outcome, not a
constructional failure. `InvokeAction` on an unknown name is a silent no-op
(returns `bool` found-and-ran) — this is documented, depended-upon behavior
("safe to bind actions before an embedder exists"), not a bug.

**Unchecked/UB hot-path contract**: functions like `Graph::Degree`/
`Neighbor`/`NeighborWeight`, `BitSet::Test`, etc. do zero bounds validation,
matching `std::vector::operator[]` — reserved for genuinely hot inner-loop
paths. Bounds-checked mutators (`RemoveEdge`, `SetEdgeWeight`, etc.) return
`bool` for routine checkable outcomes instead of throwing.

## Layering

Each layer depends only on the layers above it:

| Layer | Contents |
|---|---|
| `core` | `ThreadPool` (raw `pthread_t` workers, 8MiB stacks, `Submit`/`Wait`/`ForRange`), `Vec.hpp` (small, dimension-unrolled vector-math free functions: `VecAccFRAttForce`, `VecAccLinLogRepForce`, `VecAccGravityForce`, etc. — all the pairwise force math every force-based embedder calls into). Knows nothing about graphs. |
| `ds` | `Graph`, `Subgraph`, `BitSet`, `QuadTree`. Knows `core` only. |
| `search` | `gviz::search::BreadthFirst`, `DepthFirst`, `ConnectedComponents`, `IsTree`/`IsLeaf`/`CountLeaves`, `KNearest`/`KNearestScratch`/`KNearestFromVisibleBatch` — free function templates over `Subgraph`. Knows `ds` + `core`. |
| `layout` | The layout algorithms (see below). Knows everything above; nothing above knows it. |
| `io` / `graphs` | File loaders (`gviz::io`), synthetic graph generators (`gviz::graphs`). Knows `ds`; **never** `layout`. |

## Core data structures

### `gviz::Graph` — adjacency-list graph (`include/Graph.hpp`)

A `Graph` holds `std::vector<Vertex>`, each `Vertex` owning its own
`std::vector<Edge>` (`Edge{ size_t idx; double weight; }`) — a direct
translation of the classic adjacency-list design, chosen so that
degree-proportional operations (which dominate every embedder here) are
O(degree) rather than O(V) as a dense adjacency matrix would force.
Undirected graphs store each edge symmetrically on both endpoints' lists
rather than de-duplicating, which keeps every read path (`Degree`,
`Neighbor`, `NeighborWeight`) identical for directed and undirected graphs —
no "is this directed?" branch on the hot path.

Structural mutation (`AddVertex`, `AddEdge`, `RemoveEdge`,
`InsertNeighborAt`) bumps a `MutationCount()` counter. This counter is the
backbone of the whole dynamic-graph story: derived views (subgraphs,
embedded-graph snapshots) don't re-walk the graph to notice a change, they
compare one integer. Vertex removal does not exist anywhere in this stack
(documented as unsupported at the `EmbeddedGraph` layer) — only growth and
edge removal are.

Weights are a first-class part of `Edge`, not bolted on: every edge carries
a `double weight`, settable/gettable/mirrored automatically for undirected
edges (`SetEdgeWeight`). This is what feeds `.edges`/`.gexf` weighted
loading (`gviz::io::LoadFromEdgesFile` currently parses-but-doesn't-apply
`.edges` weight tokens — every edge gets weight 1.0, a faithful port of the
old C's actual, undocumented-as-such behavior).

`BuildLayout()`/`EnsureLayout()` are `const` (via a `mutable
std::unique_ptr<Layout> layout_` member) — a deliberate design so read-only-
looking callers (e.g. `gviz::graphs::IsConnected`) don't need a non-const
`Graph&`. `Graph`'s private `Layout` struct (per-vertex prefix sums over
adjacency, for flat edge-bit addressing) is reachable from `Subgraph` via
`friend class Subgraph;` — the chosen mechanism for the deliberate,
hot-path-justified internal coupling between the two, replacing "everything
is public because it's all header structs" with a proper, narrow C++
access-control boundary. It is **not automatically kept up to date** —
`AddEdge`/`RemoveEdge` invalidate it — and must be rebuilt before anything
that depends on it is used again. Creating a full subgraph or edge subset
against a graph with no layout yet throws `NoLayoutError` (a load-bearing
improvement over the old C's silent-empty-view-then-crash-later failure
mode).

### `gviz::Subgraph` — vertex-induced vs. full (`include/Subgraph.hpp`)

A `Subgraph` is a view over a parent `Graph`: a vertex `BitSet`, plus
optionally an edge `BitSet`. Every downstream consumer (embedders, search
algorithms) addresses vertices by **parent-graph (raw) id**, and the read
API (`HasVertex`/`HasEdge`/`Degree`/vertex and neighbor iterators, all real
`std::input_iterator`-satisfying types supporting range-for) is identical
regardless of which kind of subgraph it is. The two kinds:

- **Vertex-induced** (`Subgraph::CreateVertexInduced`) owns only the vertex
  bitset; edge membership is implicit. Never throws, never needs a built
  `Graph::Layout`, and growing it is amortized O(1). This is the
  **preferred kind everywhere, including whole-graph views** — it's the
  only kind cheap enough to keep in sync with a graph that's mutating while
  being simulated/rendered.
- **Full** (`Subgraph::CreateFull`/`CreateEmpty`) additionally materializes
  an explicit edge bitset addressed through the parent's `Layout`. Throws
  `NoLayoutError` if the parent has none. Categorically less convenient for
  anything dynamic — doesn't auto-include edges added after creation, and
  any structural mutation requires rebuilding both the layout and the
  subgraph. Use it only when explicit per-edge membership is actually the
  point.

`Subgraph` holds a `const Graph&` **reference** member — move-constructible
but **not assignable** (a reference can't be reseated), and copy is
deliberately deleted. This propagates to every embedder: `EmbeddedGraph`
and its subclasses are copy-disabled, move-only, for the same reason.

**`Subgraph` deliberately never exposes its parent `Graph&`** — this is a
real, load-bearing design boundary, not an oversight, and it created real
friction during the port that's worth understanding before you add a new
consumer:

- `EmbeddedGraph::Sync()` needs three scalar facts about the parent graph
  (mutation count, size, directedness) without a raw `Graph&`. `Subgraph`
  gained three narrow, read-only passthroughs for exactly this —
  `ParentMutationCount()`, `ParentSize()`, `ParentIsDirected()` — a
  deliberate, reviewed exception to the "never expose the parent" rule,
  chosen over just handing out a `const Graph&` getter.
- Several embedders (`ReingoldTilford`, `Planar`, `Tutte`, `SpringTutte`)
  need *mutable* adjacency access that genuinely can't be answered through
  `Subgraph` at all — `ReingoldTilford` needs `gviz::search::IsTree`'s live
  `parents` output plus positional adjacency (`Graph::Neighbor`/
  `NeighborPosition`); the planar-rotation-system embedders need to
  *reorder* adjacency lists in place. These classes hold their own `Graph&`
  member **in addition to** the `Subgraph` they hand to `EmbeddedGraph`'s
  base constructor — e.g. `Tutte::Tutte(Graph &g, Subgraph subgraph, ...)`,
  not `Tutte::Tutte(Subgraph subgraph, ...)` as the original port plan
  assumed. Both parameters must refer to the same graph; passing a
  `subgraph` not derived from `g` is an unchecked precondition violation,
  matching every other cross-object consistency assumption in this
  library.

If you're adding a new embedder or algorithm and find yourself wanting a
`Graph&` out of a `Subgraph`, this is the fork to make deliberately: either
it's a narrow, justified scalar passthrough (extend `Subgraph`'s existing
whitelist), or it's real adjacency mutation/positional access (hold your
own `Graph&` alongside the `Subgraph`, like `Planar`/`Tutte`/`SpringTutte`/
`ReingoldTilford` do) — don't quietly punch a `const Graph&` getter into
`Subgraph` itself.

### `gviz::BitSet` (`include/BitSet.hpp`)

Backed by `std::vector<uint64_t>` word storage — **not** `std::vector<bool>`
— specifically so `Subgraph`'s vertex/edge iteration (the hottest path in
the library) can use `std::countr_zero` word-skipping to jump over runs of
unset bits instead of testing bit-by-bit. `std::vector<bool>`'s proxy
interface can't expose word-level access, so this overrides the general
"prefer STL containers" default; the tradeoff is explicitly judged worth it
given the standing priority on `Subgraph` hot-path fidelity.

### `gviz::QuadTree` (`include/QuadTree.hpp`)

Barnes-Hut spatial index for `ForceAtlas`'s repulsion approximation. Uses a
block arena (`std::vector<std::unique_ptr<Node[]>>`) rather than a flat
`std::vector<Node>`, specifically so `Node*` pointers stay valid across the
thousands of `Rebuild()` calls a force-layout simulation makes per second —
a flat vector would invalidate every outstanding pointer on reallocation.

## `gviz::layout::EmbeddedGraph` — the shared embedder base (`include/EmbeddedGraph.hpp`)

Every embedder publicly inherits from `EmbeddedGraph`, which owns a
`Subgraph`, an n-dimensional position buffer, and three generic registries
(actions, stat series, draw mask) plus, for dynamic graphs, a synced-topology
snapshot. Real C++ inheritance replaces the old C convention of "cast the
embedder struct's first member to the base type" — a front-end can hold an
`EmbeddedGraph&`/`EmbeddedGraph*` uniformly regardless of which concrete
embedder is underneath. Virtual destructor (embedders are commonly destroyed
through a base pointer); no other virtual methods (embedder-vs-embedder
dispatch doesn't exist — a front-end picks a concrete embedder type once at
construction and holds that type, or holds `EmbeddedGraph&` purely for the
generic actions/stats/positions surface).

Positions are indexed by parent-graph (raw) vertex id, vertex-major,
contiguous (`Positions()` returns `std::span<const double>` for bulk read) —
so a renderer can upload the whole buffer in one shot. `OutNeighbors`/
`InNeighbors` (the synced adjacency, see below) also return `std::span`.

**Highlight** is `std::optional<Subgraph>`, not a raw `Subgraph` value —
`Subgraph`'s non-assignable `const Graph&` member means a bare `Subgraph`
field can't be cleared/replaced by assignment; `std::optional` gives
`SetHighlight`/`ClearHighlight` a clean `.emplace()`/`.reset()`
implementation instead.

`IsPlanarEmbedded()`/the underlying flag has a `protected` setter
(`SetPlanarEmbedded`) — only `Planar` (and anything else installing a
rotation system) needs to set it, and that's exactly the kind of narrow
internal coupling between a base class and its planarity-requiring
subclasses that a `protected` member is for, mirroring `Graph`'s `friend
class Subgraph` in spirit.

## Sync/commit machinery (dynamic graphs)

A graph can grow while an embedder is actively running and being rendered —
a first-class, tested capability. The design is a commit model:

- Application code mutates the parent `Graph` directly
  (`AddVertex`/`AddEdge`/`RemoveEdge`) at any time. `EmbeddedGraph` exposes
  **no mutation proxies**.
- Nothing downstream reflects a mutation until `EmbeddedGraph::Sync()`
  (or an embedder's own `Sync` wrapper) commits it — this is why the CSR
  adjacency fields (`outNeighborOffsets`/`outNeighbors`, `in*` for directed
  graphs) exist as a snapshot rather than reading the live graph directly.
- `Sync()` returns `bool` (true = a commit happened, false = no-op) — the
  old C's tri-state `int` (0/1/-1) collapses once allocation failure is
  `std::bad_alloc` instead of a checked return.
- Vertices added since the last `Sync` are auto-admitted; a strict subset
  chosen at construction is preserved.
- `ForceAtlas` is the only embedder with its own `Sync` wrapper layering
  physics catch-up on top of the structural commit: it places each newly
  admitted vertex near the centroid of its already-synced neighbors
  (jittered), recomputes degree/mass for *every* vertex (a new edge between
  two old vertices changes their degree too), and leaves existing vertices'
  positions/physics history untouched. Calling the *base*
  `EmbeddedGraph::Sync()` directly on a `ForceAtlas` skips that catch-up.
- This is why dynamic embeddings need a **vertex-induced** subgraph at
  construction, not a full one — see the `Subgraph` section above.

## Actions and stat series

- **Actions**: an embedder registers named handlers (e.g.
  `"grip.refineRound"`, `"forceEmbedder.step"`,
  `"forceEmbedder.toggleOverlapPrevention"`, `"tutte.step"`,
  `"tutte.fixOuterFace"`) via `EmbeddedGraph::AddAction`. A front-end
  invokes them by name with a generic `ActionPayload` (cursor position,
  delta time, a generic `int64_t`/`double`). Invoking an unregistered name
  is a no-op returning `false` — never throws, since call sites are
  designed around binding actions before an embedder necessarily exists.
  Naming convention: `"component.verb"`.
- **Stat series**: an embedder appends named scalar time series during its
  inner loop (`StatAppend`, e.g. `"forceEmbedder.maxDisp"`/`"speed"`/
  `"attractiveForce"`, or GRIP's per-round heat/force stats). A front-end
  enumerates and charts every non-empty series with zero semantic knowledge
  of what the numbers mean. A `revision` counter lets a front-end re-draw
  only when something changed.

Both registries store **borrowed** name pointers (never copied) — only pass
string literals or otherwise-static storage.

## Draw mask

`DrawMask` (visible-vertex `BitSet` + an edge-drawing policy: all / none /
"only if both endpoints visible") is presentation filtering, layered on top
of a subgraph that still fully participates in physics/adjacency — hiding
something is a rendering concern, not a structural one. A `revision`
counter lets a renderer detect mask changes without diffing the bitset every
frame.

## Embedders

| Embedder | Header | Dims | Structural requirement | What it's for |
|---|---|---|---|---|
| `gviz::layout::ForceAtlas` | `ForceAtlas.hpp` | 2 only | any graph, directed OK | General-purpose force-directed layout. Barnes-Hut quadtree repulsion by default, exact O(V²) fallback. `std::unique_ptr<ForceModel>` picks `FruchtermanReingold` or `LinLog` at construction; everything else (speed regulation, Barnes-Hut traversal, actions/stats) is shared. Step length uses ForceAtlas2's adaptive global-speed algorithm (Jacomy et al. 2014). Supports optional gravity and radius-aware "prevent overlap" repulsion. The only embedder with first-class dynamic-graph `Sync` support. |
| `gviz::layout::GRIP` | `GRIP.hpp` | 2, 3, or 4 | any graph; needs ≥ dim+1 active vertices | Large-graph layout via maximal-independent-set filtration: coarse hierarchy, place coarsest layer as a simplex, refine layer by layer with KNN-spring relaxation (`gviz::search::KNearestScratch`). Drivable one-shot (`Embed()`) or manually (`Begin`/`RefineRound`/`NextStage`). In 4D, net rotation is projected out each round. Config split between constructor-time `GRIPConfig` (KNN capacity, stats-enabled — genuinely construction-only in the old C) and post-construction `ConfigureK` (the old C silently reset K-policy at `Init` regardless of pre-`Init` calls, so it has no construction-time constraint to preserve). |
| `gviz::layout::Tutte` | `Tutte.hpp` | 2 | **planar** (`Begin()` throws `PlanarNotPlanarError` otherwise) | Real-time barycentric embedding: interior vertices relax toward the barycenter of their neighbors every step, boundary vertices pinned on a convex polygon (Jacobi by default, `SetGaussSeidelEnabled` for the alternative). `Begin()` calls `ApplyPlanarRotation` + `LargestFaceBoundary` (from `Planar.hpp`) and auto-pins the largest combinatorial face; `FixOuterFace()` re-pins to the highlight subgraph's boundary (via `FaceWalk`). Holds its own `Graph&` — see the `Subgraph` section above. |
| `gviz::layout::SpringTutte` | `SpringTutte.hpp` | 2 | planar | Same fixed point as `Tutte`, reached via a damped harmonic oscillator (`a = stiffness*(barycenter - P) - damping*v`) instead of a direct position blend. Deliberately kept structurally parallel to `Tutte` — same method names/order/doc voice — diverging only where the physics genuinely differs: velocity state, `Configure(stiffness, damping)` in place of `SetGaussSeidelEnabled`, and `Run(maxIters, dt)` taking an explicit `dt` (Tutte's blend has no natural time unit, so its `Run` implicitly uses `dt=1.0`). |
| `gviz::layout::ReingoldTilford` | `ReingoldTilford.hpp` | 2 | **directed tree**, rooted; constructor throws `NotATreeError` otherwise | Classic tidy tree layout. Workflow is fixed and sequential: construct → `CalculateOffsets(root, 0)` → `Embed(root, position)`. One-shot, not iterative. Takes `const Graph&` (not `Subgraph`) — see the `Subgraph` section above for why. |
| `gviz::layout::Planar` | `Planar.hpp` | 2 | planar | Boyer-Myrvold planarity testing (vendored, `third-party/boyerMyrvold/`) plus CCW rotation-system construction and a straight-line embedding. Throws `PlanarNotPlanarError` (carrying a Kuratowski subdivision witness) on non-planar input. Also owns every planarity-specific free function that operates on *any* embedded graph regardless of producer — `ApplyPlanarRotation`, `FaceWalk` (a real range/iterator replacing the old step-function pattern), `FaceEnumerator`, `Triangulate`, `FaceSubgraphAt`, `LargestFaceBoundary` — deliberately kept out of the base `EmbeddedGraph`, since they're planarity's concern. Constructor is `(Graph&, Subgraph)`, not `(Subgraph)` alone. |
| `gviz::layout::SchnyderWood` | `SchnyderWood.hpp` | — | triangulated planar | Schnyder wood (realizer) decomposition into three directed trees. Construction (canonical ordering) is verified correct; `Embed()` faithfully ports a *known-incomplete* algorithm from the old C (an unresolved `// TODO: fix this` in the region-counting logic) rather than guessing at a fix — documented prominently in the header. `Embed()` throws `DimensionError` if the target embedding isn't 2D (the old C silently over-read a fixed-size stack buffer for any other dimension). |
| Manual positions | bare `EmbeddedGraph` + `SetVPosition`/`AddVPosition` | 2, 3, 4 | any | No algorithm at all — just the shared base, for callers computing positions themselves while still getting actions/stats/draw-mask for free. |

`include/leiden/leiden.h` is a design-notes stub (community-detection
partitioning), never had a C implementation, and is not pulled in by
`gviz.hpp` — don't treat it as available API.

## Loading and generating graphs

`include/GraphLoader.hpp` (`gviz::io`) loads `.edges` (Network Repository
format, optional weights parsed-but-not-applied, external ids compacted to
dense 0-based internal ids), `.gexf` (topology + per-vertex JSON-string
attributes — GEXF vertex data is stored as `new std::string(json)` cast to
`Graph`'s `void*` vertex-data field; call `FreeVertexDataStrings(Graph&)`
before the graph is destroyed, since `Graph`'s destructor stays agnostic
about what its `void*` vertex data points to), and `.obj` (Wavefront,
vertices + face-loop edges). All three throw `std::runtime_error` on
I/O/parse failure and take `const std::filesystem::path&`.

`include/Graphs.hpp` (`gviz::graphs`) has synthetic generators (Sierpinski
triangle/tetrahedron/carpet, meshes, Möbius strip, Klein bottle, random
connected graphs) returning `Graph` by value, used throughout the test suite
and benches. `IsConnected(const Graph&)` is genuinely `const`, built on
`gviz::search::ConnectedComponents` over a vertex-induced `Subgraph` rather
than a hand-rolled BFS.

`utils/serializers.h` from the old C codebase was **not ported** — it
existed solely to feed the type-erased `gvizArray`'s debug printer, and
`gvizArray` itself has no C++ equivalent (replaced everywhere by
`std::vector<T>`, which is already templated and doesn't need a type-erased
serializer to print).

Neither loader nor generator ever touches embedder code — see the layering
table above.

## Working in this repo

- Build: `cmake -B build && cmake --build build -j` (links `planar` — the
  vendored Boyer-Myrvold implementation — `m`, and `Threads::Threads`; no
  other external deps). `-DENABLE_ASAN=ON` for AddressSanitizer.
- Tests use Unity (`third-party/unity/`, compiled as C++), run via `ctest`
  from `build/`, or individually (`./build/tests/ds/GraphTests`, etc. — see
  `README.md` for the full list). `GRIPKBench`/`GRIPLayerProbe`/
  `GRIPMigrateBench`/`GRIPFiltrationDebug`/`GRIPPlaceBench`/`TuttePick`
  under `tests/` are standalone benchmarking/probing tools, not registered
  with `ctest` (except `TuttePick`, which does return a real pass/fail exit
  code) — check `README.md`'s "Benchmarks and debug tools" section before
  assuming a `tests/` binary is a real test.
- Debug/profiling env vars (`GVIZ_GRIP_DEBUG_FILTRATION`,
  `GVIZ_GRIP_STAGE_TIMING`, `GVIZ_KNN_PROFILE`) are documented in detail in
  `README.md` — they're opt-in, add overhead, and write to stderr.
- Conventions: RAII throughout (construct-or-throw, destructor frees —
  no manual Init/Release lifecycle). Real C++ iterators satisfying
  `std::input_iterator` wherever a class is naturally iterable (`BitSet`,
  `Subgraph`, `Planar`'s `FaceWalk`). Every public function has a doc
  comment stating its exact contract, ownership rules, and failure modes —
  read it before guessing a signature or behavior. Never hand-roll a
  growable array, bitset, deque, or tree in new code — `std::vector`/
  `gviz::BitSet`/`std::deque` already cover everything this codebase needs.
