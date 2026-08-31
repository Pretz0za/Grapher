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

**As of the `concepts-refactor` work**, this architecture doc supersedes any
earlier description of `EmbeddedGraph`/`Subgraph`/dynamic-graph sync and
highlight bookkeeping — that machinery was removed. If you find stale
references to `EmbeddedGraph::Sync()`, `OutNeighbors`/`InNeighbors`,
`SetHighlight`/`GetHighlight`, or a `Subgraph` member on the base
`EmbeddedGraph` in prior notes, they describe the pre-refactor design; the
sections below describe what actually exists now.

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
`BitSet`, `QuadTree`, `ThreadPool`, `DenseIndex`). A *grouped system* gets
its own nested namespace: `gviz::layout` (the embedders and their shared
base/strategy types), `gviz::search` (BFS/DFS/ConnectedComponents/KNearest/
tree predicates — free function templates over `GraphLike`, not a class,
since there's no owned state), `gviz::io` (graph loaders), `gviz::graphs`
(synthetic generators).

**Fallible construction throws.** Anything that would have returned a
negative error code from an `Init`/`Begin`-style C function because the
operation fundamentally cannot proceed (non-planar graph handed to `Planar`,
wrong embedding dimension, not a tree, insufficient vertices for GRIP's
dimension, disconnected graph handed to KamadaKawai) throws directly from the
constructor — `gviz::LayoutError` (in `Error.hpp`) is the common base, with
`NotPlanarError`, `NotATreeError`, `NotConnectedError`, `DimensionError`,
`InsufficientVerticesError`, `NoLayoutError` as concrete subclasses (plus
`Planar.hpp`'s `PlanarNotPlanarError : public NotPlanarError`, which
additionally carries a Kuratowski subdivision witness graph). If a
constructor doesn't throw, the object is valid, full stop — no factory
function, no `std::optional`/`std::expected` indirection. Allocation failure
needs no bespoke type: `std::vector`/`new` already throw `std::bad_alloc`.
Pure I/O/parse failures (`gviz::io`'s file loaders) throw plain
`std::runtime_error` instead — `LayoutError` is semantically for
graph-structural/dimensional failures, not "the file doesn't exist."

**Tutte/SpringTutte are a deliberate, narrow exception to "fallible
construction throws" for planarity specifically**: they no longer test
planarity at all (see their own section below) — a non-planar structure
handed to them simply produces a broken/overlapping layout when relaxed,
not an exception. This is an intentional post-refactor behavior change, not
an oversight.

**Lookup-style queries stay pointer-based, not `std::optional`.** Things
like `FindAction`/`FindStatSeries` (not found → `nullptr`) keep returning a
nullable pointer — "not found" is a routine, common query outcome, not a
constructional failure. `InvokeAction` on an unknown name is a silent no-op
(returns `bool` found-and-ran) — this is documented, depended-upon behavior
("safe to bind actions before an embedder exists"), not a bug.

**Unchecked/UB hot-path contract**: functions like `Graph::Degree`/
`Neighbor`/`NeighborWeight`, `BitSet::Test`, `DenseIndex::ToLocal`/`ToRaw`,
etc. do zero bounds validation, matching `std::vector::operator[]` —
reserved for genuinely hot inner-loop paths. Bounds-checked mutators
(`RemoveEdge`, `SetEdgeWeight`, etc.) return `bool` for routine checkable
outcomes instead of throwing.

## Layering

Each layer depends only on the layers above it:

| Layer | Contents |
|---|---|
| `core` | `ThreadPool` (raw `pthread_t` workers, 8MiB stacks, `Submit`/`Wait`/`ForRange`), `Vec.hpp` (small, dimension-unrolled vector-math free functions: `VecAccFRAttForce`, `VecAccLinLogRepForce`, `VecAccGravityForce`, etc. — all the pairwise force math every force-based embedder calls into). Knows nothing about graphs. |
| `ds` | `Graph`, `Subgraph`, `BitSet`, `QuadTree`, `GraphLike` (the traversal concept), `DenseIndex` (the raw-handle <-> dense-local-index adapter built on it). Knows `core` only. |
| `search` | `gviz::search::BreadthFirst`/`BreadthFirstTree`, `DepthFirst`, `ConnectedComponents`, `IsTree`/`IsLeaf`/`CountLeaves`, `KNearest`/`KNearestScratch`/`KNearestFromVisibleBatch` — free function templates over `GraphLike`. Knows `ds` + `core`. |
| `layout` | The layout algorithms (see below). Knows everything above; nothing above knows it. |
| `io` / `graphs` | File loaders (`gviz::io`), synthetic graph generators (`gviz::graphs`). Knows `ds`; **never** `layout`. |

## `gviz::GraphLike` — the shared traversal concept (`include/GraphLike.hpp`)

A C++20 concept capturing exactly what every traversal algorithm and every
embedder in this library actually needs from "a graph": vertex
range/iteration (`begin()`/`end()`), `HasVertex(u)`, `Degree(u)`, and
`Neighbors(u)` yielding something usable as a `size_t` vertex handle.
Nothing about density, mutability, or ownership is part of the contract —
`GraphLike` promises identity and iterability, **never density**. `Graph`
happens to have dense `[0, Size())` handles (see `DenseIndex` below for the
zero-overhead special case this enables); `Subgraph`'s handle is its
existing raw parent-graph vertex id, generally non-contiguous within the
view, and that is equally valid.

Both `Graph` and `Subgraph` satisfy `GraphLike` with essentially no change
to their pre-existing `HasVertex`/`Neighbors`/`Degree`/iteration semantics.
`Subgraph` already had all four. `Graph` gained a few small, genuinely new
members purely to round out the same shape:
- `HasVertex(size_t u)` — O(1), `u < Size()`.
- `begin()`/`end()` — iterate `[0, Size())` (an `iota_view`, no storage of
  its own).
- `static constexpr bool kDenseVertexHandles = true;` — an opt-in
  customization point `DenseIndex` checks (see below); `Subgraph` doesn't
  declare it, so defaults to `false` for it.
- `Edge::operator size_t()` (non-explicit) — lets
  `Graph::Neighbors(u)`'s existing, **unchanged**
  `const std::vector<Edge>&` return type be iterated as plain vertex handles
  the same way `Subgraph::NeighborRange` already can (`for (size_t nb :
  g.Neighbors(u))` works uniformly for both types). This was a deliberate,
  narrow addition, not a reason to widen `Edge`'s role generally — it exists
  solely for `GraphLike` conformance.

`GraphLikeVertexCount(g)` / `GraphLikeVertexCapacity(g)` / `GraphLikeIsDirected(g)`
(free function templates in the same header) pick whichever O(1) accessor a
given `G` actually has (`Graph::Size()`/`Subgraph::VertexCount()`,
`Subgraph::VertexCapacity()`, `Graph::IsDirected()`/`Subgraph::ParentIsDirected()`)
so generic code doesn't need to special-case each concrete type by name.
`VertexCount` is the view's actual size (what per-vertex storage should be
sized to); `VertexCapacity` is an upper bound on the *handle range* (what a
scratch structure addressed directly by native handle, e.g. a BFS
visited-bitset, needs to cover) — these are NOT the same number for a small
`Subgraph` view of a huge `Graph`, and conflating them is exactly the bug
class this refactor exists to fix (see `DenseIndex` below).

Algorithms and embedders should be templated as `template <GraphLike G>
... (G structure, ...)` rather than hard-committing to `Subgraph` or
`Graph`. `Planar`, `SchnyderWood`, and `ReingoldTilford` are the deliberate
exceptions — see their own sections below for why each one stays
Graph-specific instead.

## `gviz::DenseIndex<G>` — the dense-index adapter (`include/DenseIndex.hpp`)

The fix for the root bug this refactor came out of: several classes used to
size per-vertex storage off a `Subgraph`'s **parent graph's** raw id range
(`Subgraph::VertexCapacity()`) instead of the view's actual vertex count —
so a 50-vertex `Subgraph` view of a 100K-vertex `Graph` could allocate
storage sized for 100K vertices. `KamadaKawai::distances_` (`n_ * n_` with
`n_ = Structure().VertexCapacity()`) and `EmbeddedGraph::positions_` (sized
to `Graph::Size()` regardless of the view) were the two concretely named
instances; `GRIP`'s decorator arrays (`dec_`, `dispCalculated_`) were a
third, and `ForceAtlas` had already worked around the same problem privately
with its own hand-rolled compact-index array (`vertices_`) before this
refactor generalized that trick.

`DenseIndex<G>` builds a raw-handle <-> dense-local-index bijection by
walking `G`'s vertex range exactly **once**, at construction — no
rebuild-on-mutation logic, since dynamic graph growth during an active
embedding is out of scope for this library (see `EmbeddedGraph` below).

- **Zero-overhead identity case**: when `G::kDenseVertexHandles` is `true`
  (`Graph` opts in), `ToLocal`/`ToRaw` are the identity function and no
  array is ever allocated — `Size()` is just the view's own vertex count.
  This is a real zero-overhead case, not just uniformity theater: a
  `DenseIndex<Graph>` costs nothing beyond what the embedder would have
  paid anyway.
- **General case** (`Subgraph`, or any future `GraphLike` view without dense
  handles): `ToLocal` is an O(1) array lookup sized to
  `GraphLikeVertexCapacity(g)` when `G` exposes a capacity bound (as
  `Subgraph` does); otherwise falls back to a hash map, so the utility stays
  correct (if slower) for a hypothetical capacity-less `GraphLike` type.

`ToLocal`/`ToRaw` are unchecked/UB on an unknown handle or out-of-range
local index — same hot-path contract as everything else in this library.

**This is what every embedder's per-vertex storage is built on**:
`EmbeddedGraph`'s position buffer is sized from a plain vertex count each
concrete embedder computes via `DenseIndex`; `ForceAtlas<G>`'s
`mass_`/`degree_`/`disp_`/etc. (its old private `vertices_` compact-index
machinery is gone, superseded by this); `GRIP<G>`'s `dec_`/`dispCalculated_`;
`KamadaKawai<G>`'s `distances_` (now `index_.Size() ^ 2`, not
`VertexCapacity() ^ 2`); `Tutte<G>`/`SpringTutte<G>`'s `isBoundary_`/
`scratch_`/`velocity_`.

**Ergonomics, not just memory**: most of an embedder's inner loop is pure
per-vertex work with no adjacency crossing at all (e.g. `ForceAtlas`'s
`ComputeSwingTractionRange`/`ApplySpeedRange`/`UpdateGlobalSpeed`) — that
code loops `for (size_t i = 0; i < n; i++)` directly against dense arrays,
no handles involved at all, calling `EmbeddedGraph`'s own local-index
`GetVPosition(i)`/`SetVPosition(i, ...)` directly. The only place a native
`G` handle appears is where an edge is actually crossed (e.g.
`ForceAtlas::ComputeForceRange`'s attraction loop) — and there, each
concrete embedder defines its **own** same-named `GetVPosition(handle)`/
`SetVPosition(handle, ...)`/`AddVPosition(handle, ...)` methods that shadow
(not override — see `EmbeddedGraph` below) the base class's local-index
versions, translating via `index_.ToLocal(handle)` internally. No algorithm
author writes a manual `ToLocal`/`ToRaw` call themselves at an actual
physics/algorithm call site — it's centralized in exactly one place per
class.

**Deliberately not extended everywhere**: `GRIP<G>`'s MIS-filtration
machinery (`misFiltration_`/`misBorder_` and the various scratch `BitSet`s
`IterMISFiltration`/`MakeFirstMISPartition` build) stays addressed by
native/raw handle, sized against `GraphLikeVertexCapacity` — this is
legitimate, algorithm-owned complexity (the MIS coarsening permutation
order), not part of the sizing bug; only `misFiltration_`'s overall *size*
shrank (parent capacity → view count), not its per-slot addressing.
Likewise `GRIP`'s `radiusBfsScratch_`/`radiusBfsDepth_`/`knnScratch_` and
`gviz::search::KNearestScratch` in general stay capacity-addressed by
design (see `KNearest.hpp`'s design note) — a deliberate, judgment-call
scope boundary for this refactor: KNN's own BFS walks native handles
directly, and re-deriving it to route every visited-neighbor lookup through
a `DenseIndex` translation was judged out of proportion to the benefit for
what remains a *transient*, freed-after-use scratch buffer (as opposed to
the *persistent* per-vertex tables the fix above targets). Flag this if you
hit a case where it actually matters (e.g. a very sparse `Subgraph` view
driving heavy KNN traffic) — it's a real, known, not-yet-addressed gap, not
a settled design decision.

## Core data structures

### `gviz::Graph` — adjacency-list graph (`include/Graph.hpp`)

A `Graph` holds `std::vector<Vertex>`, each `Vertex` owning its own
`std::vector<Edge>` (`Edge{ size_t idx; double weight; operator size_t()
const; }` — see the `GraphLike` section above for why `Edge` has that
conversion operator) — a direct translation of the classic adjacency-list
design, chosen so that degree-proportional operations (which dominate every
embedder here) are O(degree) rather than O(V) as a dense adjacency matrix
would force. Undirected graphs store each edge symmetrically on both
endpoints' lists rather than de-duplicating, which keeps every read path
(`Degree`, `Neighbor`, `NeighborWeight`) identical for directed and
undirected graphs — no "is this directed?" branch on the hot path.

Structural mutation (`AddVertex`, `AddEdge`, `RemoveEdge`,
`InsertNeighborAt`) bumps a `MutationCount()` counter. This counter is what
lets a derived view (`Subgraph`'s full-mode edge bitset) answer "has the
graph changed since I last looked?" with one integer compare. Vertex removal
does not exist anywhere in this stack (documented as unsupported) — only
growth and edge removal are.

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
against a graph with no layout yet throws `NoLayoutError`.

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
  **preferred kind everywhere, including whole-graph views**.
- **Full** (`Subgraph::CreateFull`/`CreateEmpty`) additionally materializes
  an explicit edge bitset addressed through the parent's `Layout`. Throws
  `NoLayoutError` if the parent has none. Categorically less convenient for
  anything dynamic — doesn't auto-include edges added after creation, and
  any structural mutation requires rebuilding both the layout and the
  subgraph.

`Subgraph` holds a `const Graph&` **reference** member — move-constructible
but **not assignable** (a reference can't be reseated), and copy is
deliberately deleted. This propagates to every embedder built over it:
holding a `G structure_` member (where `G` might be `Subgraph`) makes that
embedder copy-disabled, move-only, for the same reason.

**`Subgraph` deliberately never exposes its parent `Graph&`** — a real,
load-bearing design boundary. It exposes only three narrow, read-only
scalar passthroughs: `ParentMutationCount()`/`ParentSize()`/
`ParentIsDirected()`. Anything needing more (mutable adjacency access,
positional neighbor queries) holds its own `Graph&` alongside whatever
`Subgraph`/other structure it owns — `Tutte<G>`/`SpringTutte<G>` do **not**
need this anymore (see their section below, this was the pre-refactor
design); `Planar` and `ReingoldTilford` do, for reasons specific to each
(see their own sections). If you're adding a new embedder/algorithm and
want a `Graph&` out of a `Subgraph`, this is the fork to make deliberately —
extend the narrow whitelist only for a justified scalar, or hold your own
`Graph&` like `Planar` does; don't punch a general `const Graph&` getter
into `Subgraph` itself.

### `gviz::BitSet` (`include/BitSet.hpp`)

Backed by `std::vector<uint64_t>` word storage — **not** `std::vector<bool>`
— specifically so `Subgraph`'s vertex/edge iteration (the hottest path in
the library) can use `std::countr_zero` word-skipping to jump over runs of
unset bits instead of testing bit-by-bit.

### `gviz::QuadTree` (`include/QuadTree.hpp`)

Barnes-Hut spatial index for `ForceAtlas`'s repulsion approximation. Uses a
block arena (`std::vector<std::unique_ptr<Node[]>>`) rather than a flat
`std::vector<Node>`, specifically so `Node*` pointers stay valid across the
thousands of `Rebuild()` calls a force-layout simulation makes per second.

## `gviz::layout::EmbeddedGraph` — the shared, GraphLike-agnostic base (`include/EmbeddedGraph.hpp`)

`EmbeddedGraph` is a plain, **non-template**, polymorphic base holding only
`G`-agnostic state: dimension, an n-dimensional position buffer (indexed by
LOCAL/compact index, `[0, PositionCount())`), and three generic registries
(actions, stat series, draw mask). It knows nothing about `GraphLike`, holds
no `Subgraph`/`Graph`/any structure at all, and is constructed from a plain
`size_t vertexCount` — the caller (a concrete embedder) computes that count
however it likes (typically `DenseIndex<G>::Size()`).

**Why non-template**: `EmbeddedGraph`'s whole point is that a front-end
(including grender) can hold any embedder polymorphically through
`EmbeddedGraph&`/`EmbeddedGraph*` without caring which concrete embedder —
or which `GraphLike` type that embedder was built over — produced it. If
`EmbeddedGraph` itself were templated over `G`, `ForceAtlas<Graph>` and
`ForceAtlas<Subgraph>` would be unrelated types and that polymorphism would
break. So genericity lives one level down: every concrete embedder that
needs it is `template <GraphLike G> class Foo : public EmbeddedGraph`, owns
its own strongly-typed `G structure_` plus a `gviz::DenseIndex<G> index_`
built from it, and passes this base constructor just the resulting vertex
count.

**Per-vertex accessors are local-index only, by design.** `GetVPosition`/
`SetVPosition`/`AddVPosition(size_t i)` here take a LOCAL index — the base
has no notion of what a native `G` handle even looks like. Each concrete
embedder that wants to accept a native handle at its own public API surface
defines its **own** same-named methods that *shadow* (hide, not override —
`EmbeddedGraph`'s methods are ordinary non-virtual functions) these,
translating via `index_.ToLocal(handle)` and delegating to
`EmbeddedGraph::GetVPosition(local)` (explicitly qualified). This is the
same shadowing pattern `ForceAtlas::Sync()` used to use to layer physics
catch-up on top of the (now-removed) base `Sync()`, generalized to
positions. Internal per-vertex-only code (no edge crossed) should call the
base local-index accessors directly (`EmbeddedGraph::GetVPosition(i)`),
never round-trip through the handle-taking wrapper for a `i` you already
know is local.

Virtual destructor only — there is nothing here a derived embedder needs to
override, only state and bookkeeping it builds on top of, but code owning
an embedder polymorphically must be able to destroy through a base pointer.

**Removed outright in this refactor, not relocated:**
- **The Sync/commit machinery** (`Sync()`, `OutNeighbors`/`InNeighbors`/
  `OutDegree`/`InDegree`, the cached CSR snapshot). Dynamic graph growth
  during an active embedding is out of scope for this library now — every
  per-vertex array here and in every concrete embedder is built exactly
  once, at construction, from `structure_` as it stood then. `ForceAtlas`
  specifically lost `GrowPerVertexArraysTo`, `PlaceGrownVertex`, its own
  `Sync()` wrapper, and `physicsSyncedMutationCount_`; it does build its own
  build-once in/out adjacency CSR at construction (for directed structures
  needing an in-neighbor pass GraphLike's `Neighbors()` alone can't give —
  see `ForceAtlas`'s own section) instead of reading a cached snapshot that
  no longer exists.
- **The highlight subgraph** (`SetHighlight`/`ClearHighlight`/
  `HasHighlight`/`GetHighlight`, the `highlight_` member). This was
  frontend/presentation "which vertices/edges are currently selected"
  bookkeeping, not this library's concern. A front-end that wants it should
  keep its own selection state (grender now keeps this on its own
  `grRenderer`, alongside `backingGraph` — see grender's own CLAUDE.md).

**`IsEdgeVisible` changed shape** as a consequence of the base losing
structural knowledge: it now takes `(size_t iu, size_t iv, bool
edgeExists)` — the caller (a concrete embedder or a renderer walking its
`Structure()`) supplies whether the edge actually exists, since the base
can no longer look it up itself. Only the `DrawEdgePolicy` decision itself
(which never depended on `G`) stays centralized here.

**Draw mask** is sized once, at construction, to the embedding's vertex
count, and stays that size — "grown in lockstep by `Sync()`" is gone along
with `Sync()` itself.

## Search algorithms (`gviz::search`, templated over `GraphLike`)

`BreadthFirst`, `DepthFirst`, `ConnectedComponents`, `IsTree`/`IsLeaf`/
`CountLeaves`, `KNearest`/`KNearestScratch`/`KNearestFromVisibleBatch` are
all `template <GraphLike G>` now, rather than hard-committing to a single
concrete type. Most are header-only templates (`DepthFirst.hpp`,
`ConnectedComponents.hpp` for its templated function, `Tree.hpp`,
`KNearest.hpp`); `BreadthFirst`'s primary form and `KNearest`'s profiling
globals keep a `.cpp` for what doesn't need to be a template.

**`BreadthFirst` was split in two**, since the pre-refactor version forced
its `out` parameter to be a *full* `Subgraph` purely so it could double as
tree-recording — which forced `GRIP::VerticesWithinRadius` and
`KamadaKawai::ComputeDistancesFrom` to each hand-roll an identical
neighbor-queue BFS instead of reusing it.
- **`BreadthFirst(const G &g, size_t source, std::vector<size_t>
  &distances, size_t maxDepth = 0)`** — the primary primitive now. Just
  fills a distance array (sized to `GraphLikeVertexCapacity(g)`, addressed
  by raw handle, SIZE_MAX = unreachable); no tree, no full-`Subgraph`
  requirement. `KamadaKawai<G>::Begin()` calls this once per vertex, then
  translates each result into its own LOCAL-indexed `distances_` table via
  `DenseIndex` (see `KamadaKawai`'s section) — the fix for the motivating
  `O(parent V^2)` bug lives in that translation step, not in this function.
- **`BreadthFirstTree(const Subgraph &sg, Subgraph &out, size_t source,
  size_t maxDepth = 0, std::vector<size_t> *distances = nullptr)`** — the
  old behavior, kept under a distinct name, for callers that actually want
  the tree edges recorded (only meaningful for `Subgraph`, since only its
  full mode can hold an edge bitset). `GRIPFiltrationDebug`/
  `GRIPLayerProbe`/`GRIPMigrateBench`/`SearchTests` use this form.

`GRIP<G>::VerticesWithinRadius` deliberately still hand-rolls its own BFS
rather than calling the shared primitive — see `GRIP`'s section below for
why (it needs live depth/visited state mid-traversal for
`PickFarCandidate`, a shape the shared primitive doesn't (and shouldn't)
expose).

`IsTree`/`IsLeaf`/`CountLeaves` are templated over `GraphLike` for
consistency, but in practice only ever instantiated over `Graph`
(`ReingoldTilford`'s sole caller needs positional adjacency `Subgraph`
can't provide, so it always holds a `const Graph&` directly) — `IsTree`
specifically requires `graph.IsDirected()`, which `GraphLike` itself does
not guarantee, so instantiating it over a type without that (e.g.
`Subgraph`) simply won't compile.

## Embedders

| Embedder | Header | Generic over `GraphLike`? | Dims | Structural requirement | What it's for |
|---|---|---|---|---|---|
| `gviz::layout::ForceAtlas<G>` | `ForceAtlas.hpp` | Yes | 2 only | any graph, directed OK | General-purpose force-directed layout. Barnes-Hut quadtree repulsion by default, exact O(V²) fallback. `std::unique_ptr<ForceModel>` picks `FruchtermanReingold` or `LinLog` at construction. Step length uses ForceAtlas2's adaptive global-speed algorithm (Jacomy et al. 2014). Supports optional gravity and radius-aware "prevent overlap" repulsion. No dynamic-graph growth support anymore (see `EmbeddedGraph` above) — its own build-once in/out adjacency CSR (needed for directed structures' attraction pass; an undirected structure's out-adjacency already holds both directions) is built once at construction, not read from a cached snapshot. |
| `gviz::layout::GRIP<G>` | `GRIP.hpp` | Yes | 2, 3, or 4 | any graph; needs ≥ dim+1 active vertices | Large-graph layout via maximal-independent-set filtration: coarse hierarchy, place coarsest layer as a simplex, refine layer by layer with KNN-spring relaxation. Drivable one-shot (`Embed()`) or manually (`Begin`/`RefineRound`/`NextStage`). In 4D, net rotation is projected out each round. Its per-vertex decorator state (`dec_`, `dispCalculated_`) is `DenseIndex`-backed (LOCAL-indexed); its MIS-filtration machinery stays raw/capacity-addressed by design (see `DenseIndex`'s section above). |
| `gviz::layout::Tutte<G>` | `Tutte.hpp` | Yes | 2 | none enforced (see below) | Real-time barycentric embedding: interior vertices relax toward the barycenter of their neighbors every step, boundary vertices pinned (Jacobi by default, `SetGaussSeidelEnabled` for the alternative). No longer holds a `Graph&`, no longer depends on `Planar.hpp`, no longer tests planarity or auto-detects a boundary — `SetBoundary()`/`FixConvexPolygon()` are the only way to pin one (a successful call also marks `Begun()` true), and `Run()` throws `std::logic_error` if none has ever been pinned. A genuinely non-planar structure just relaxes into a possibly-overlapping layout; this is accepted, intentional behavior now, not an error. `Begin()` and `FixOuterFace()` no longer exist (see `Tutte`'s section below for the full removal rationale). |
| `gviz::layout::SpringTutte<G>` | `SpringTutte.hpp` | Yes | 2 | none enforced | Same fixed point as `Tutte<G>`, reached via a damped harmonic oscillator (`a = stiffness*(barycenter - P) - damping*v`) instead of a direct position blend. Kept structurally parallel to `Tutte<G>` — same method names/order/doc voice, same planarity-agnostic contract, same removal of `Begin()`/`FixOuterFace()`/`Graph&`/`Planar.hpp` — diverging only where the physics genuinely differs (velocity state, `Configure(stiffness, damping)`, explicit `dt` in `Run()`). |
| `gviz::layout::ReingoldTilford` | `ReingoldTilford.hpp` | **No** (`const Graph&`) | 2 | **directed tree**, rooted; constructor throws `NotATreeError` otherwise | Classic tidy tree layout. Workflow is fixed and sequential: construct → `CalculateOffsets(root, 0)` → `Embed(root, position)`. One-shot, not iterative. Takes `const Graph&` directly: needs `gviz::search::IsTree`'s live `parents` output as ongoing state (not just a check) and positional adjacency (`Graph::Neighbor(v, i)`, `Graph::NeighborPosition`) that neither `Subgraph` nor `GraphLike`'s traversal-only contract can answer ("child i of v" isn't well-defined under a filtered view). Treats `graph`'s own dense `[0, Size())` ids as the position buffer's local index directly — no `DenseIndex`/`Subgraph` constructed at all. Known ported bug, deliberately preserved: `CalculateOffsets` never increments `level`, so `Height()` is always 0. |
| `gviz::layout::Planar` | `Planar.hpp` | **No** (`Graph&` only, no `Subgraph` parameter) | 2 | planar | Boyer-Myrvold planarity testing (vendored, `third-party/boyerMyrvold/`) plus CCW rotation-system construction and a straight-line embedding. Throws `PlanarNotPlanarError` (carrying a Kuratowski subdivision witness) on non-planar input. **Always embeds the whole graph** — `explicit Planar(Graph &g)`, the same shape `ReingoldTilford` already correctly used before this refactor. No `DenseIndex`/shadowed position accessors either: a whole `Graph`'s ids are already dense from 0 (`Graph::kDenseVertexHandles`), so `EmbeddedGraph`'s local-index `GetVPosition`/`SetVPosition` are exactly right unshadowed. Internally builds a throwaway `Subgraph::CreateFull(g)` purely to satisfy `ApplyPlanarRotation`/`FaceEnumerator`/`Triangulate`'s existing `Subgraph`-taking signatures — never exposed on `Planar`'s own public API. Also owns every planarity-specific free function that operates on *any* embedded graph regardless of producer — `ApplyPlanarRotation`, `FaceWalk`, `FaceEnumerator`, `Triangulate`, `FaceSubgraphAt`, `LargestFaceBoundary` — deliberately kept out of the base `EmbeddedGraph`, and deliberately still `Subgraph`-taking themselves (general rotation-system/face utilities usable independent of the `Planar` class). `FaceSubgraphAt` takes `const EmbeddedGraph&` (not `const Planar&`): since `Planar` now always embeds the whole graph, its local position index and the raw `Graph`'s vertex ids coincide exactly, so the GraphLike-agnostic base's `GetVPosition` is correct without needing a concrete, `DenseIndex`-aware type — this is also structurally no loss of generality, since `Planar` is the only embedder that ever sets `IsPlanarEmbedded()` true (`Tutte`/`SpringTutte` no longer do). |
| `gviz::layout::SchnyderWood` | `SchnyderWood.hpp` | **No** (`const Graph&`) | — | triangulated planar | Schnyder wood (realizer) decomposition into three directed trees. Construction (canonical ordering) is verified correct; `Embed()` faithfully ports a *known-incomplete* algorithm from the old C (an unresolved `// TODO: fix this` in the region-counting logic). `Embed()` takes a plain `EmbeddedGraph&` and writes positions via its LOCAL-index `SetVPosition` directly, under the standing assumption (already implicit in "vertex 0 is Root(0)") that the embedding's local index coincides with the graph's raw id 1:1 — true for every real caller (`Planar`, which always embeds the whole graph now) but an explicit precondition worth knowing if you ever call `Embed()` against something else. Throws `DimensionError` if the target embedding isn't 2D. |
| `gviz::layout::KamadaKawai<G>` | `KamadaKawai.hpp` | Yes | any (≥ 1) | **connected** (`Begin()` throws `NotConnectedError` otherwise) | Kamada & Kawai (1989) graph-theoretic-distance layout: minimizes a global energy over all-pairs unweighted BFS hop distances by repeatedly Newton-Raphson-refining whichever single vertex currently has the largest energy gradient. `Begin()` now calls the shared `gviz::search::BreadthFirst` primitive once per vertex (see the search-algorithms section above) instead of hand-rolling a BFS, translating each result into its own `DenseIndex`-backed, LOCAL-indexed `distances_` table (`index_.Size() ^ 2`, not `Structure().VertexCapacity() ^ 2` — the fix for the bug that motivated this whole refactor). |
| Manual positions | bare `EmbeddedGraph` + `SetVPosition`/`AddVPosition` | — | 2, 3, 4 | any | No algorithm at all — just the shared base (constructed from a plain vertex count now, not a `Subgraph`), for callers computing positions themselves while still getting actions/stats/draw-mask for free. |

`include/leiden/leiden.h` is a design-notes stub (community-detection
partitioning), never had a C implementation, and is not pulled in by
`gviz.hpp` — don't treat it as available API.

### Explicit template instantiation, not header-only, for the sizeable embedders

`ForceAtlas<G>`, `GRIP<G>`, `KamadaKawai<G>`, `Tutte<G>`, `SpringTutte<G>`
are declared as templates in their headers but *defined* out-of-line in
their `.cpp` files, with `template class Foo<Graph>;` / `template class
Foo<Subgraph>;` explicit instantiations at the bottom — not turned fully
header-only. Every real consumer in this codebase (tests, benches, grender)
only ever instantiates over exactly these two `GraphLike` types, so this
gets the same benefits a non-template class gets (implementation hidden
from downstream compile units, compiled once per translation unit only
here) without the header-only bloat a fully generic library would need. If
you introduce a third `GraphLike` type that needs one of these embedders,
add its explicit instantiation line rather than making the whole class
header-only.

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
  `README.md` — they're opt-in, add overhead, and write to stderr. All
  survived this refactor unchanged (they're about `GRIP`'s own MIS-
  filtration/KNN internals, not the removed Sync machinery).
- Conventions: RAII throughout (construct-or-throw, destructor frees —
  no manual Init/Release lifecycle). Real C++ iterators satisfying
  `std::input_iterator` wherever a class is naturally iterable (`BitSet`,
  `Subgraph`, `Planar`'s `FaceWalk`). Never hand-roll a growable array,
  bitset, deque, or tree in new code — `std::vector`/`gviz::BitSet`/
  `std::deque` already cover everything this codebase needs.
- **Comment style — keep it minimal.** Headers (`include/*.hpp`): a short
  doc comment on each public class/function stating what it does, its
  parameters, its return value, and what it throws — nothing more. If a
  comment states a real behavioral contract (e.g. "unchecked: caller must
  ensure X < Size()"), keep that sentence; cut everything padding around
  it. Do not write design rationale ("chosen because..."), historical/
  porting narration, alternatives-considered discussion, or cross-
  references to other design decisions — none of that belongs in the code.
  `.cpp` files: comments only where something genuinely non-obvious is
  happening that a reader couldn't figure out from the code itself (a
  subtle invariant, a non-obvious bug workaround) — a real "why", not a
  "what". Well-named code should already say what it does; don't narrate
  it line by line.
- **No dynamic-graph support currently**: don't mutate a `Graph` that
  backs an active embedding and expect the embedding to notice — there is
  no `Sync()` anymore. This capability may return in a future, separate
  pass; treat any request to re-add it as a real design discussion, not a
  quick patch.
- **Grender compatibility**: this refactor is a breaking change for the
  sibling `grender` project, which has not been updated to match (out of
  scope for this pass). In particular, grender's `grTopology.cpp` walks
  `EmbeddedGraph::Structure()` (now gone from the base) and depends on
  `OutNeighbors`/`InNeighbors`/`Sync`/the highlight subgraph (all removed);
  its `backingGraph`/highlight-adjacent plumbing will need a follow-up port
  before grender builds against a post-refactor gviz again.
