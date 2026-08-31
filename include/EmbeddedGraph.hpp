#ifndef GVIZ_EMBEDDEDGRAPH_HPP
#define GVIZ_EMBEDDEDGRAPH_HPP

#include "BitSet.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace gviz::layout {

class EmbeddedGraph;

/**
 * Payload delivered to an action handler. Front-ends (renderers, scripts,
 * tests) fill in whichever fields make sense for the triggering event and
 * leave the rest zeroed (the default-constructed value).
 */
struct ActionPayload {
  /** Pointer/cursor location in embedding coordinates, if any. */
  double worldX = 0.0, worldY = 0.0;
  /** Seconds elapsed since the previous frame/tick, 0 if unknown. */
  double deltaTime = 0.0;
  /** Generic integer argument (e.g. repeat count, button id). */
  int64_t iarg = 0;
  /** Generic floating-point argument (e.g. scroll delta, strength). */
  double darg = 0.0;
};

/**
 * An action subroutine attached to an embedded graph. @p embedding is the
 * graph the action was registered on; @p userData is the pointer supplied at
 * registration time. Plain function pointer, not std::function: actions are
 * invoked on user interaction (not a hot loop), but every embedder that
 * registers one does so with a free function/static method, never a
 * capturing closure, so the extra generality (and heap allocation risk) of
 * std::function buys nothing here -- same shape as the old C
 * gvizActionHandler typedef.
 */
using ActionHandler = void (*)(EmbeddedGraph &embedding, void *userData,
                                const ActionPayload &payload);

struct Action {
  /** Stable identifier, e.g. "grip.refineRound". Not copied; the string must
   *  outlive the embedded graph (string literals are the expected usage). */
  const char *name;
  ActionHandler handler;
  void *userData;
};

/** How a front-end should chart a stat series. Chosen by the embedder. */
enum class StatChartKind {
  /** Line chart, linear y axis (default). */
  Line = 0,
  /** Line chart, logarithmic y axis (for quantities spanning decades). */
  LineLog = 1,
};

/**
 * A named time series recorded by the creator of an embedded graph (typically
 * an embedder) and charted by front-ends. Samples are appended one double at
 * a time; the sample index is the x axis (round, iteration, ...).
 */
struct StatSeries {
  /** Stable identifier, e.g. "grip.heat". Not copied; the string must outlive
   *  the embedded graph (string literals are the expected usage). */
  const char *name;
  StatChartKind kind = StatChartKind::Line;
  std::vector<double> samples;
  /** Incremented on every append/clear; front-ends compare it to re-chart
   *  without polling the data. */
  uint64_t revision = 0;
};

/** Controls which vertices and edges front-ends (renderers) should draw. */
enum class DrawEdgePolicy {
  /** Draw every edge in the subgraph (default). */
  All = 0,
  /** Draw no edges. */
  None = 1,
  /** Draw edge (u, v) only when both endpoints pass the vertex filter. */
  IfBothVisible = 2,
};

/** Presentation filter layered on top of a subgraph: which vertices/edges a
 *  renderer should draw, independent of what the subgraph considers
 *  structurally present. See EmbeddedGraph's class doc for the distinction. */
struct DrawMask {
  /** A vertex is visible when its bit is set and it is in the subgraph.
   *  Initialized to every subgraph vertex on construction, and grown (never
   *  shrunk) in lockstep with the subgraph's vertex capacity by Sync(). */
  BitSet visibleVertices;
  DrawEdgePolicy edgePolicy = DrawEdgePolicy::All;
  /** Incremented when the mask is reset, the edge policy changes, or
   *  DrawMaskNotifyChanged() is called; renderers may compare this to detect
   *  mask changes without polling the bitset. */
  uint64_t revision = 0;
};

/**
 * Shared base of every layout algorithm in this library (ForceAtlas, GRIP,
 * Tutte, SpringTutte, ReingoldTilford, Planar): an n-dimensional position
 * buffer over a Subgraph's vertices, plus three generic registries (actions,
 * stats, draw mask) that are the entire interface between an embedder and
 * whatever is driving it -- a renderer, a script, a test -- so neither side
 * needs to know about the other. Direct port of the old C gvizEmbeddedGraph,
 * with the "cast the derived struct's first member" fake-inheritance hack
 * replaced by real public inheritance: every concrete embedder derives from
 * this class instead of embedding it as a first field.
 *
 * Virtual destructor: this is a genuine base class meant to be inherited
 * from and, unlike ForceModel's per-call virtual dispatch, most of
 * EmbeddedGraph's own methods are ordinary (non-virtual) -- there is
 * nothing here that a derived embedder needs to override, only state and
 * bookkeeping it builds on top of. But code that owns an embedder
 * polymorphically (e.g. a front-end holding a std::unique_ptr<EmbeddedGraph>
 * without caring which concrete layout algorithm produced it, or a
 * container of several different embedders) must be able to destroy through
 * a base pointer safely, so the destructor is virtual. The cost (one vtable
 * pointer per object, once, for a type that already isn't part of any hot
 * per-vertex loop) is negligible next to the safety it buys.
 *
 * Ownership: positions, actions, stats, and the draw mask are all owned
 * value members (std::vector/std::vector, no manual Release()) -- RAII
 * handles teardown, including for a derived embedder's own members, without
 * this class needing a destructor body. Allocation failure propagates as
 * std::bad_alloc from whichever std::vector operation triggered it; there
 * are no manual -1 return codes anywhere in this class.
 *
 * Presentation vs. structure: hiding a vertex/edge via the draw mask is a
 * rendering concern layered on top of a subgraph that still fully
 * participates in physics/adjacency queries; removing something from the
 * subgraph changes what the layout algorithm itself considers. Keep the two
 * separate -- an embedder's physics should never consult the draw mask, and
 * a renderer should never mutate subgraph membership to hide something.
 */
class EmbeddedGraph {
public:
  /**
   * Takes ownership of @p subgraph (moved in) and allocates a
   * @p dimension-dimensional, zero-initialized position slot for every
   * vertex in @p subgraph's parent graph (by raw id, vertex-major).
   *
   * @throws std::bad_alloc on allocation failure (propagated naturally from
   * std::vector; there is no separate failure-return path).
   */
  EmbeddedGraph(Subgraph subgraph, size_t dimension);

  // Polymorphic base: copying would slice a derived embedder's own state,
  // so copy is disabled outright rather than left to silently compile a
  // partial copy. Move is fine (Subgraph itself is move-constructible) and
  // is what a derived embedder's own defaulted/custom move constructor
  // relies on; move ASSIGNMENT stays disabled because Subgraph's `const
  // Graph&` member can't be reseated (Subgraph::operator=(Subgraph&&) is
  // itself deleted for the same reason -- see Subgraph.hpp).
  EmbeddedGraph(const EmbeddedGraph &) = delete;
  EmbeddedGraph(EmbeddedGraph &&) noexcept = default;
  EmbeddedGraph &operator=(const EmbeddedGraph &) = delete;
  EmbeddedGraph &operator=(EmbeddedGraph &&) = delete;
  virtual ~EmbeddedGraph() = default;

  // BULK READ ACCESS (for renderers and other consumers): ---------------------

  /** Returns the dimension of the embedding. */
  size_t Dim() const noexcept { return dim_; }

  /**
   * Returns the number of position slots in the embedding: the parent
   * graph's vertex count as of the last Sync() (a vertex added to the graph
   * gets its slot when the next Sync() commits it, not before). Positions
   * are indexed by parent-graph vertex id; use Structure() to determine
   * which slots are live.
   */
  size_t PositionCount() const noexcept { return syncedGraphSize_; }

  /**
   * Returns the contiguous position buffer: PositionCount() * Dim() doubles,
   * vertex-major. Valid until the embedding is destroyed or the next call
   * that changes PositionCount(). Intended for bulk readers (e.g. renderers)
   * that would otherwise call GetVPosition per vertex.
   */
  std::span<const double> Positions() const noexcept {
    return std::span<const double>(positions_.data(), syncedGraphSize_ * dim_);
  }

  /** The subgraph describing the structure of the embedded graph. */
  Subgraph &Structure() noexcept { return subgraph_; }
  const Subgraph &Structure() const noexcept { return subgraph_; }

  // GROWTH & SYNC (dynamic graphs): --------------------------------------------
  //
  // Mutate the parent Graph directly (AddVertex/AddEdge/RemoveEdge); this
  // class deliberately has no mutation proxies. Nothing about the embedding
  // -- subgraph membership, positions, draw mask, adjacency accessors --
  // reflects a mutation until Sync() commits it, so the embedding always
  // describes one coherent moment: the last commit. The intended loop is
  // mutate freely during a frame, then Sync() once at the frame boundary (a
  // no-op Sync is one integer compare).
  //
  // Sync() assumes the subgraph is meant to track the whole growing graph:
  // every vertex added to the graph since the embedding last synced is
  // admitted (shown in the subgraph and the draw mask). A strict subset
  // chosen at construction is preserved -- only vertices newer than the
  // embedding's last sync are auto-admitted. Edge removal commits like any
  // other mutation; vertex removal is unsupported (nothing in this stack
  // removes vertices).
  //
  // Use a VERTEX-INDUCED subgraph (Subgraph::CreateVertexInduced) for a
  // dynamic embedding: its Sync-time rebuild is amortized O(1) bit-capacity
  // growth. A full subgraph's rebuild remaps the whole edge bitset (O(V+E)
  // and worse, see Subgraph::Rebuild) and its explicitly-managed edge subset
  // does NOT auto-include edges added after creation -- full subgraphs are
  // for static layouts; structurally hiding edges for presentation is the
  // draw mask edge policy's job.

  /**
   * Commits the parent graph's mutations since the last Sync into the
   * embedding: admits vertices added since the embedding last synced (marks
   * them present in the subgraph and visible in the draw mask, with zeroed
   * positions), grows the position buffer and draw mask to match, rebuilds
   * the out/in adjacency CSRs from the subgraph as it now stands, and bumps
   * the draw mask revision so renderers re-read geometry.
   *
   * A derived embedder that needs its own catch-up on top of this (e.g. a
   * force layout placing newly admitted vertices before they're ever
   * observed at their zeroed placeholder position) should override this
   * behavior by providing its own Sync-like entry point that calls this
   * base Sync() first, exactly as the old gvizForceEmbedderSync layered
   * physics catch-up on top of gvizEmbeddedGraphSync.
   *
   * @return true if a commit happened, false for the no-op (nothing changed
   * since the last Sync). Allocation failure propagates as std::bad_alloc;
   * per Graph::MutationCount()'s contract this class's own state remains
   * exactly as before the throwing operation (std::vector's strong
   * exception guarantee on the growth path), so a caller that catches and
   * retries later resumes cleanly -- newly admitted subgraph/draw-mask bits
   * may already be set by the time an exception is thrown, which the retry
   * re-commits harmlessly, same as the old C contract.
   */
  bool Sync();

  /**
   * Out-degree of raw vertex @p v in the synced snapshot -- for undirected
   * graphs simply v's degree, since undirected adjacency is stored on both
   * endpoints. 0 for a vertex the snapshot doesn't know: one added after the
   * last Sync, or any vertex before the first Sync. Unchecked in the sense
   * that any @p v is accepted (out-of-range or never-synced ids simply
   * report 0), not a precondition violation.
   */
  size_t OutDegree(size_t v) const noexcept;

  /** Out-neighbors of raw vertex @p v in the synced snapshot (raw ids).
   *  Empty for a vertex the snapshot doesn't know (see OutDegree). Valid
   *  until the next Sync() or destruction. */
  std::span<const size_t> OutNeighbors(size_t v) const noexcept;

  /**
   * In-degree of raw vertex @p v in the synced snapshot: how many synced
   * edges u -> v exist. 0 when the graph is undirected (those edges are
   * already counted by OutDegree) or the snapshot doesn't know @p v.
   */
  size_t InDegree(size_t v) const noexcept;

  /** In-neighbors of raw vertex @p v in the synced snapshot -- every u with
   *  a synced edge u -> v. Empty when the graph is undirected or the
   *  snapshot doesn't know @p v. Valid until the next Sync() or
   *  destruction. */
  std::span<const size_t> InNeighbors(size_t v) const noexcept;

  // DRAW MASK (for renderers): -------------------------------------------------
  //
  // The embedder creator sets which vertices and edges should be drawn.
  // Defaults show the full subgraph. Front-ends read the mask each frame (or
  // watch DrawMaskRevision()) and filter geometry accordingly.

  /** Sets the edge filter policy and bumps the draw mask revision. */
  void SetDrawMaskEdgePolicy(DrawEdgePolicy edgePolicy);

  /** Marks vertex @p u visible in the draw mask (does not bump the
   *  revision). Unchecked: @p u must be within the mask's current capacity
   *  (Structure().VertexCapacity()). */
  void DrawMaskShowVertex(size_t u) noexcept;

  /** Marks vertex @p u hidden in the draw mask (does not bump the
   *  revision). Same unchecked contract as DrawMaskShowVertex. */
  void DrawMaskHideVertex(size_t u) noexcept;

  /** Clears every vertex in the draw mask (does not bump the revision). */
  void DrawMaskClearVertices() noexcept;

  /** Bumps the draw mask revision after a batch of Show/Hide calls so
   *  renderers rebuild filtered geometry. */
  void DrawMaskNotifyChanged() noexcept { drawMask_.revision++; }

  /** Resets the mask to the default (all subgraph vertices, all edges). */
  void ResetDrawMask();

  /** The current draw mask (read-only). */
  const DrawMask &GetDrawMask() const noexcept { return drawMask_; }

  /** Monotonic counter; changes whenever the mask is set or reset. */
  uint64_t DrawMaskRevision() const noexcept { return drawMask_.revision; }

  /** Whether vertex @p u should be drawn under the current mask and
   *  subgraph. */
  bool IsVertexVisible(size_t u) const noexcept;

  /** Whether edge (@p u, @p v) should be drawn under the current mask (both
   *  endpoints must be in the subgraph when iterating). */
  bool IsEdgeVisible(size_t u, size_t v) const noexcept;

  // ACTIONS: --------------------------------------------------------------------
  //
  // The creator of an embedded graph (typically an embedder) may register
  // named actions on it. Front-ends discover actions by name or enumeration
  // and invoke them with a payload; the front-end needs no knowledge of the
  // embedder and the embedder needs no knowledge of the front-end.

  /**
   * Registers an action. @p name is not copied and must outlive this object
   * (string literals are the expected usage). If an action with the same
   * name already exists, its handler and userData are replaced.
   *
   * @return false (no-op) if @p name or @p handler is null, true otherwise.
   * Allocation failure propagates as std::bad_alloc.
   */
  bool AddAction(const char *name, ActionHandler handler,
                 void *userData = nullptr);

  /** Removes the action named @p name. Returns true if removed, false if
   *  not found or @p name is null. Does not preserve the relative order of
   *  the remaining actions (swap-with-last removal), matching ActionAt()'s
   *  contract of "some registered action at this index", not a stable one. */
  bool RemoveAction(const char *name);

  /** Returns the action named @p name, or nullptr if none is registered. */
  const Action *FindAction(const char *name) const noexcept;

  /** Number of registered actions. */
  size_t ActionCount() const noexcept { return actions_.size(); }

  /** The @p idx-th registered action; nullptr if out of bounds. */
  const Action *ActionAt(size_t idx) const noexcept;

  /**
   * Invokes the action named @p name with @p payload (nullptr passes a
   * default-constructed/zeroed payload to the handler).
   *
   * @return true if the action ran, false if no such action is registered.
   * This is a deliberate, depended-upon no-op (not an exception): it must
   * stay safe to invoke actions by name before an embedder that implements
   * them exists, or to bind more front-end actions than any one embedder
   * implements.
   */
  bool InvokeAction(const char *name, const ActionPayload *payload = nullptr);

  // STATS: ------------------------------------------------------------------
  //
  // The creator of an embedded graph may record named time series ("how did
  // the mean heat evolve per round?") on it. Front-ends discover the series
  // by enumeration and chart them; the front-end needs no knowledge of the
  // embedder and the embedder needs no knowledge of the front-end.

  /**
   * Registers a stat series and returns it. If a series with the same name
   * already exists, its kind is updated and it is returned unchanged.
   * @p name is not copied and must outlive this object (string literals are
   * the expected usage).
   *
   * @return the series, or nullptr if @p name is null. Allocation failure
   * propagates as std::bad_alloc. The returned pointer is invalidated by any
   * subsequent AddStatSeries/StatAppend call that registers a new series
   * (std::vector reallocation) -- use it immediately, don't cache it across
   * calls (mirrors the old C gvizStatSeries* contract).
   */
  StatSeries *AddStatSeries(const char *name, StatChartKind kind);

  /**
   * Appends @p value to the series named @p name, creating it (with
   * StatChartKind::Line) if it does not exist yet. This is the one-liner
   * embedders are expected to call from their inner loop.
   *
   * @return false (no-op) if @p name is null, true otherwise. Allocation
   * failure propagates as std::bad_alloc.
   */
  bool StatAppend(const char *name, double value);

  /** Discards all samples of the series named @p name (the series itself
   *  stays registered). No-op if no such series exists or @p name is
   *  null. */
  void StatClear(const char *name);

  /** Number of registered stat series. */
  size_t StatSeriesCount() const noexcept { return stats_.size(); }

  /** The @p idx-th registered series; nullptr if out of bounds. */
  const StatSeries *StatSeriesAt(size_t idx) const noexcept;

  /** Returns the series named @p name, or nullptr if none is registered. */
  const StatSeries *FindStatSeries(const char *name) const noexcept;

  /** Whether a planar rotation system has been installed (see the Planar
   *  embedder; face queries on an embedded graph live there too). */
  bool IsPlanarEmbedded() const noexcept { return planarEmbedded_; }

  /** Replaces the highlight subgraph. */
  void SetHighlight(Subgraph highlight);

  /** Clears the highlight subgraph, if any. */
  void ClearHighlight() noexcept { highlight_.reset(); }

  /** Whether a highlight subgraph is set. */
  bool HasHighlight() const noexcept { return highlight_.has_value(); }

  /** The highlight subgraph, or nullptr when none is set. */
  const Subgraph *GetHighlight() const noexcept {
    return highlight_ ? &*highlight_ : nullptr;
  }

  // Per-vertex position access. Unchecked (no bounds validation) -- these
  // sit in every embedder's inner loop and must not gain a branch, exactly
  // like Graph::Degree/Neighbor.

  /** Pointer to the @p idx-th vertex's position (Dim() doubles). */
  double *GetVPosition(size_t idx) noexcept { return positions_.data() + idx * dim_; }
  const double *GetVPosition(size_t idx) const noexcept {
    return positions_.data() + idx * dim_;
  }

  /** Overwrites the @p idx-th vertex's position from @p position (Dim()
   *  doubles). */
  void SetVPosition(size_t idx, const double *position) noexcept;

  /** Adds @p position (Dim() doubles) to the @p idx-th vertex's position. */
  void AddVPosition(size_t idx, const double *position) noexcept;

  /**
   * Places every vertex in the subgraph uniformly at random inside the
   * [-boxExtent, boxExtent]^Dim() box, so a layout can start compact rather
   * than scattering vertices arbitrarily far apart. @p seed seeds the
   * generator; pass 0 for a time-based seed. Shared starting-layout logic
   * for embedders and front-ends that want a scattered placement before
   * running one.
   */
  void RandomizePositions(double boxExtent, unsigned int seed);

  /**
   * Writes vertex positions to @p filename in text format.
   *
   * @return true on success, false if the file cannot be opened. I/O
   * failure is routine/checkable here, not exceptional -- the object stays
   * validly constructed either way.
   */
  bool SaveEmbedding(const char *name, const char *filename) const;

  /**
   * Loads vertex positions from @p filename. Vertex count and dimension
   * must match this embedding.
   *
   * @return true on success, false on I/O error or a vertex-count/dimension
   * mismatch -- a routine, checkable outcome, not an exception, since the
   * object stays validly constructed (with whatever it held before the
   * call) either way.
   */
  bool LoadEmbedding(const char *filename);

protected:
  /**
   * Marks whether a planar rotation system has been installed. Only the
   * Planar embedder (and anything building on it, e.g. Tutte/SpringTutte
   * via their own Begin) should call this -- deliberately protected, not
   * public, mirroring the old C comment that planarity-specific state is
   * "not the base type's" concern to expose for general mutation, only to
   * report.
   */
  void SetPlanarEmbedded(bool value) noexcept { planarEmbedded_ = value; }

private:
  void ShowAllSubgraphVerticesInMask() noexcept;

  Subgraph subgraph_;
  size_t dim_;
  std::vector<double> positions_;
  std::vector<Action> actions_;
  std::vector<StatSeries> stats_;
  DrawMask drawMask_;
  bool planarEmbedded_ = false;
  /**
   * std::optional, not a zero-valued Subgraph like the old C highlight
   * field: Subgraph's `const Graph&` member makes it move-constructible but
   * not assignable (see Subgraph.hpp), so there is no "unbound" Subgraph
   * value to default it to and no way to reassign one in place. optional's
   * emplace()/reset() give the same "set/clear/replace" shape the old code
   * had without needing Subgraph to support assignment.
   */
  std::optional<Subgraph> highlight_;

  // SYNCED TOPOLOGY -- this object's own answer to "what is currently laid
  // out and ready to render", advanced only by Sync(). The parent Graph is
  // mutated directly and is always live; nothing here reflects a mutation
  // until the next Sync() commits it, so every consumer -- a renderer, an
  // embedder's own physics -- reads one coherent snapshot instead of
  // choosing between a live view and a simulated view that disagree.

  /** Graph::MutationCount() as of the last Sync(), making the no-op Sync
   *  check one integer compare. Sentinel "never synced" value until the
   *  first Sync(), so that first call always builds the snapshot regardless
   *  of what the graph's counter happens to be. */
  uint64_t syncedMutationCount_ = std::numeric_limits<uint64_t>::max();
  /**
   * Graph::Size() as of the last Sync() (or, before the first Sync(), as of
   * construction). Raw ids >= this are unknown to the snapshot: the
   * adjacency accessors return empty/0 for them, and the next Sync() admits
   * them. Also the admission low-water mark: vertices that existed when
   * this object was constructed are the caller's membership choice (via the
   * subgraph passed to the constructor); only vertices added after that are
   * auto-admitted by Sync().
   */
  size_t syncedGraphSize_;
  /**
   * Out-adjacency CSR of the synced structure, rows indexed by raw vertex
   * id: outNeighborOffsets_ has syncedGraphSize_ + 1 entries; row v holds
   * v's subgraph neighbors as of the last Sync() (for undirected graphs the
   * parent adjacency is stored symmetrically, so this is simply v's full
   * neighborhood). Rows of vertices outside the subgraph are empty. Empty
   * before the first Sync().
   */
  std::vector<size_t> outNeighborOffsets_;
  std::vector<size_t> outNeighbors_;
  /** Reverse (in-neighbor) CSR of the synced structure, same indexing: row
   *  v holds every u with a synced edge u -> v. Empty unless the graph is
   *  directed -- an undirected edge is already in both out rows. */
  std::vector<size_t> inNeighborOffsets_;
  std::vector<size_t> inNeighbors_;
};

} // namespace gviz::layout

#endif
