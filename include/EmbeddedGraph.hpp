#ifndef GVIZ_EMBEDDEDGRAPH_HPP
#define GVIZ_EMBEDDEDGRAPH_HPP

#include "BitSet.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gviz::layout {

class EmbeddedGraph;

/**
 * Payload delivered to an action handler. Front-ends fill in whichever
 * fields make sense for the triggering event and leave the rest zeroed.
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

/** An action subroutine attached to an embedded graph. @p embedding is the
 *  graph the action was registered on; @p userData is the pointer supplied
 *  at registration time. */
using ActionHandler = void (*)(EmbeddedGraph &embedding, void *userData,
                                const ActionPayload &payload);

struct Action {
  /** Stable identifier, e.g. "grip.refineRound". Not copied; the string must
   *  outlive the embedded graph. */
  const char *name;
  ActionHandler handler;
  void *userData;
};

/** How a front-end should chart a stat series. */
enum class StatChartKind {
  /** Line chart, linear y axis (default). */
  Line = 0,
  /** Line chart, logarithmic y axis. */
  LineLog = 1,
};

/**
 * A named time series recorded by the creator of an embedded graph and
 * charted by front-ends. Samples are appended one double at a time; the
 * sample index is the x axis.
 */
struct StatSeries {
  /** Stable identifier, e.g. "grip.heat". Not copied; the string must
   *  outlive the embedded graph. */
  const char *name;
  StatChartKind kind = StatChartKind::Line;
  std::vector<double> samples;
  /** Incremented on every append/clear. */
  uint64_t revision = 0;
};

/** Controls which vertices and edges front-ends should draw. */
enum class DrawEdgePolicy {
  /** Draw every edge in the subgraph (default). */
  All = 0,
  /** Draw no edges. */
  None = 1,
  /** Draw edge (u, v) only when both endpoints pass the vertex filter. */
  IfBothVisible = 2,
};

/** Presentation filter layered on top of a subgraph: which vertices/edges a
 *  renderer should draw. */
struct DrawMask {
  /** A vertex is visible when its (local) bit is set. Sized once, at
   *  construction, to the embedding's vertex count. */
  BitSet visibleVertices;
  DrawEdgePolicy edgePolicy = DrawEdgePolicy::All;
  /** Incremented when the mask is reset, the edge policy changes, or
   *  DrawMaskNotifyChanged() is called. */
  uint64_t revision = 0;
};

/**
 * Shared base of every layout algorithm in this library: an n-dimensional,
 * locally-indexed position buffer, plus three generic registries (actions,
 * stats, draw mask). Holds no graph/subgraph of its own and knows nothing
 * about `gviz::GraphLike`; every concrete embedder owns its own strongly
 * typed structure and passes this base constructor just the vertex count.
 *
 * Every per-vertex accessor here (GetVPosition/SetVPosition/AddVPosition,
 * the draw mask) is indexed by local/compact index [0, PositionCount()).
 * Concrete embedders that accept a native handle define their own
 * same-named methods that shadow these, translating via their index and
 * delegating to the explicitly-qualified base method.
 *
 * Position buffer, draw mask, and every derived embedder's own per-vertex
 * arrays are sized once from the vertex count passed to the constructor and
 * never grow. No highlight subgraph: selection/highlight state is
 * presentation state and lives on the front-end, not here.
 */
class EmbeddedGraph {
public:
  /**
   * Allocates a @p dimension-dimensional, zero-initialized position slot
   * for each of @p vertexCount vertices, and a draw mask defaulting every
   * one of them to visible.
   *
   * @throws std::bad_alloc on allocation failure.
   */
  EmbeddedGraph(size_t vertexCount, size_t dimension);

  EmbeddedGraph(const EmbeddedGraph &) = delete;
  EmbeddedGraph(EmbeddedGraph &&) noexcept = default;
  EmbeddedGraph &operator=(const EmbeddedGraph &) = delete;
  EmbeddedGraph &operator=(EmbeddedGraph &&) = delete;
  virtual ~EmbeddedGraph() = default;

  // BULK READ ACCESS: -----------------------------------------------------

  /** Returns the dimension of the embedding. */
  size_t Dim() const noexcept { return dim_; }

  /** Returns the number of position slots, i.e. the vertex count passed to
   *  the constructor. */
  size_t PositionCount() const noexcept { return vertexCount_; }

  /** Returns the contiguous position buffer: PositionCount() * Dim()
   *  doubles, vertex-major, indexed by local index. */
  std::span<const double> Positions() const noexcept {
    return std::span<const double>(positions_.data(), vertexCount_ * dim_);
  }

  // DRAW MASK: --------------------------------------------------------------

  /** Sets the edge filter policy and bumps the draw mask revision. */
  void SetDrawMaskEdgePolicy(DrawEdgePolicy edgePolicy);

  /** Marks local index @p i visible (does not bump the revision).
   *  Unchecked: @p i must be < PositionCount(). */
  void DrawMaskShowVertex(size_t i) noexcept;

  /** Marks local index @p i hidden (does not bump the revision). Same
   *  unchecked contract as DrawMaskShowVertex. */
  void DrawMaskHideVertex(size_t i) noexcept;

  /** Clears every vertex in the draw mask (does not bump the revision). */
  void DrawMaskClearVertices() noexcept;

  /** Bumps the draw mask revision after a batch of Show/Hide calls. */
  void DrawMaskNotifyChanged() noexcept { drawMask_.revision++; }

  /** Resets the mask to the default (every vertex visible, every edge). */
  void ResetDrawMask();

  /** The current draw mask (read-only). */
  const DrawMask &GetDrawMask() const noexcept { return drawMask_; }

  /** Monotonic counter; changes whenever the mask is set or reset. */
  uint64_t DrawMaskRevision() const noexcept { return drawMask_.revision; }

  /** Whether local index @p i should be drawn under the current mask.
   *  Unchecked: @p i must be < PositionCount(). */
  bool IsVertexVisible(size_t i) const noexcept;

  /** Whether an edge between local indices @p iu and @p iv should be drawn
   *  under the current mask, given @p edgeExists. This class has no notion
   *  of graph structure, so the caller supplies edge existence. */
  bool IsEdgeVisible(size_t iu, size_t iv, bool edgeExists) const noexcept;

  // ACTIONS: ----------------------------------------------------------------

  /**
   * Registers an action. @p name is not copied and must outlive this
   * object. If an action with the same name already exists, its handler and
   * userData are replaced.
   *
   * @return false (no-op) if @p name or @p handler is null, true otherwise.
   */
  bool AddAction(const char *name, ActionHandler handler,
                 void *userData = nullptr);

  /** Removes the action named @p name. Returns true if removed, false if
   *  not found or @p name is null. Does not preserve the relative order of
   *  the remaining actions (swap-with-last removal). */
  bool RemoveAction(const char *name);

  /** Returns the action named @p name, or nullptr if none is registered. */
  const Action *FindAction(const char *name) const noexcept;

  /** Number of registered actions. */
  size_t ActionCount() const noexcept { return actions_.size(); }

  /** The @p idx-th registered action; nullptr if out of bounds. */
  const Action *ActionAt(size_t idx) const noexcept;

  /**
   * Invokes the action named @p name with @p payload (nullptr passes a
   * default-constructed/zeroed payload).
   *
   * @return true if the action ran, false if no such action is registered
   * (a deliberate no-op, not an exception: it must stay safe to invoke
   * actions by name before an implementing embedder exists).
   */
  bool InvokeAction(const char *name, const ActionPayload *payload = nullptr);

  // STATS: --------------------------------------------------------------------

  /**
   * Registers a stat series and returns it. If a series with the same name
   * already exists, its kind is updated and it is returned unchanged.
   * @p name is not copied and must outlive this object.
   *
   * @return the series, or nullptr if @p name is null. The returned pointer
   * is invalidated by any subsequent AddStatSeries/StatAppend call that
   * registers a new series -- use it immediately, don't cache it.
   */
  StatSeries *AddStatSeries(const char *name, StatChartKind kind);

  /**
   * Appends @p value to the series named @p name, creating it (with
   * StatChartKind::Line) if it does not exist yet.
   *
   * @return false (no-op) if @p name is null, true otherwise.
   */
  bool StatAppend(const char *name, double value);

  /** Discards all samples of the series named @p name (stays registered).
   *  No-op if no such series exists or @p name is null. */
  void StatClear(const char *name);

  /** Number of registered stat series. */
  size_t StatSeriesCount() const noexcept { return stats_.size(); }

  /** The @p idx-th registered series; nullptr if out of bounds. */
  const StatSeries *StatSeriesAt(size_t idx) const noexcept;

  /** Returns the series named @p name, or nullptr if none is registered. */
  const StatSeries *FindStatSeries(const char *name) const noexcept;

  /** Whether a planar rotation system has been installed (see the Planar
   *  embedder; face queries live there too). */
  bool IsPlanarEmbedded() const noexcept { return planarEmbedded_; }

  // Per-vertex position access, indexed by local index. Unchecked.

  /** Pointer to the @p i-th (local) vertex's position (Dim() doubles). */
  double *GetVPosition(size_t i) noexcept { return positions_.data() + i * dim_; }
  const double *GetVPosition(size_t i) const noexcept {
    return positions_.data() + i * dim_;
  }

  /** Overwrites the @p i-th (local) vertex's position from @p position
   *  (Dim() doubles). */
  void SetVPosition(size_t i, const double *position) noexcept;

  /** Adds @p position (Dim() doubles) to the @p i-th (local) vertex's
   *  position. */
  void AddVPosition(size_t i, const double *position) noexcept;

  /** Places every vertex uniformly at random inside the [-boxExtent,
   *  boxExtent]^Dim() box. @p seed seeds the generator; pass 0 for a
   *  time-based seed. */
  void RandomizePositions(double boxExtent, unsigned int seed);

  /**
   * Writes vertex positions to @p filename in text format.
   *
   * @return true on success, false if the file cannot be opened.
   */
  bool SaveEmbedding(const char *name, const char *filename) const;

  /**
   * Loads vertex positions from @p filename. Vertex count and dimension
   * must match this embedding.
   *
   * @return true on success, false on I/O error or a vertex-count/dimension
   * mismatch.
   */
  bool LoadEmbedding(const char *filename);

protected:
  /** Marks whether a planar rotation system has been installed. Only the
   *  Planar embedder (and anything building on it) should call this. */
  void SetPlanarEmbedded(bool value) noexcept { planarEmbedded_ = value; }

private:
  size_t vertexCount_;
  size_t dim_;
  std::vector<double> positions_;
  std::vector<Action> actions_;
  std::vector<StatSeries> stats_;
  DrawMask drawMask_;
  bool planarEmbedded_ = false;
};

} // namespace gviz::layout

#endif
