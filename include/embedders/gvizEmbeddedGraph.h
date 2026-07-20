#ifndef GVIZ_EMBEDDED_GRAPH_H
#define GVIZ_EMBEDDED_GRAPH_H

#include "ds/gvizArray.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizSubgraph.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct gvizEmbedding {
  size_t dim;
  double *vertexPositions;
} gvizEmbedding;

struct gvizEmbeddedGraph;

/**
 * Payload delivered to an action handler. Front-ends (renderers, scripts,
 * tests) fill in whichever fields make sense for the triggering event and
 * leave the rest zeroed.
 */
typedef struct gvizActionPayload {
  /** Pointer/cursor location in embedding coordinates, if any. */
  double worldX, worldY;
  /** Seconds elapsed since the previous frame/tick, 0 if unknown. */
  double deltaTime;
  /** Generic integer argument (e.g. repeat count, button id). */
  int64_t iarg;
  /** Generic floating-point argument (e.g. scroll delta, strength). */
  double darg;
} gvizActionPayload;

/**
 * An action subroutine attached to an embedded graph. @p embedding is the
 * graph the action was registered on; @p userData is the pointer supplied at
 * registration time.
 */
typedef void (*gvizActionHandler)(struct gvizEmbeddedGraph *embedding,
                                  void *userData,
                                  const gvizActionPayload *payload);

typedef struct gvizAction {
  /** Stable identifier, e.g. "grip.refineRound". Not copied; the string must
   *  outlive the embedded graph. */
  const char *name;
  gvizActionHandler handler;
  void *userData;
} gvizAction;

typedef struct gvizActionRegistry {
  gvizAction *actions;
  size_t count;
  size_t capacity;
} gvizActionRegistry;

/** How a front-end should chart a stat series. Chosen by the embedder. */
typedef enum gvizStatChartKind {
  /** Line chart, linear y axis (default). */
  GVIZ_STAT_CHART_LINE = 0,
  /** Line chart, logarithmic y axis (for quantities spanning decades). */
  GVIZ_STAT_CHART_LINE_LOG = 1,
} gvizStatChartKind;

/**
 * A named time series recorded by the creator of an embedded graph (typically
 * an embedder) and charted by front-ends. Samples are appended one double at
 * a time; the sample index is the x axis (round, iteration, ...).
 */
typedef struct gvizStatSeries {
  /** Stable identifier, e.g. "grip.heat". Not copied; the string must outlive
   *  the embedded graph. */
  const char *name;
  gvizStatChartKind kind;
  double *samples;
  size_t count;
  size_t capacity;
  /** Incremented on every append/clear; front-ends compare it to re-chart
   *  without polling the data. */
  uint64_t revision;
} gvizStatSeries;

typedef struct gvizStatRegistry {
  gvizStatSeries *series;
  size_t count;
  size_t capacity;
} gvizStatRegistry;

/** Controls which vertices and edges front-ends (renderers) should draw. */
typedef enum gvizDrawEdgePolicy {
  /** Draw every edge in the subgraph (default). */
  GVIZ_DRAW_EDGES_ALL = 0,
  /** Draw no edges. */
  GVIZ_DRAW_EDGES_NONE = 1,
  /** Draw edge (u, v) only when both endpoints pass the vertex filter. */
  GVIZ_DRAW_EDGES_IF_BOTH_VISIBLE = 2,
} gvizDrawEdgePolicy;

typedef struct gvizDrawMask {
  /** Owned subset: a vertex is visible when its bit is set and it is in the
   *  subgraph. Initialized to all subgraph vertices on gvizEmbeddedGraphInit. */
  gvizVertexSubset visibleVertices;
  gvizDrawEdgePolicy edgePolicy;
  /** Incremented when the mask is reset, the edge policy changes, or
   *  gvizEmbeddedGraphDrawMaskNotifyChanged is called; renderers may compare
   *  this to detect mask changes without polling the bitset. */
  uint64_t revision;
} gvizDrawMask;

typedef struct gvizEmbeddedGraph {
  gvizSubgraph subgraph;
  gvizEmbedding embedding;
  gvizActionRegistry actions;
  gvizDrawMask drawMask;
  gvizStatRegistry stats;
  int planarEmbedded;
  gvizSubgraph highlight;
} gvizEmbeddedGraph;

/**
 * Initializes @p embedding to hold @p dimension-dimensional positions for every
 * vertex in the parent graph of @p subgraph. Takes ownership of @p subgraph's
 * vertex and edge subsets; the caller must not release them afterward.
 * Positions are zeroed and indexed by parent-graph vertex id.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizEmbeddedGraphInit(gvizEmbeddedGraph *embedding, gvizSubgraph subgraph,
                          size_t dimension);

/** Releases the position buffer and action registry owned by @p embedding. */
void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding);

// BULK READ ACCESS (for renderers and other consumers): -----------------------

/** Returns the dimension of the embedding. */
size_t gvizEmbeddedGraphDim(const gvizEmbeddedGraph *embedding);

/**
 * Returns the number of position slots in the embedding, i.e. the vertex count
 * of the parent graph. Positions are indexed by parent-graph vertex id; use the
 * subgraph vertex subset to determine which slots are live.
 */
size_t gvizEmbeddedGraphPositionCount(const gvizEmbeddedGraph *embedding);

/**
 * Returns the contiguous position buffer: gvizEmbeddedGraphPositionCount() *
 * dim doubles, vertex-major. Valid until the embedding is released. Intended
 * for bulk readers (e.g. renderers) that would otherwise call
 * gvizEmbeddedGraphGetVPosition per vertex.
 */
const double *gvizEmbeddedGraphPositions(const gvizEmbeddedGraph *embedding);

/** Returns the subgraph describing the structure of the embedded graph. */
const gvizSubgraph *gvizEmbeddedGraphStructure(const gvizEmbeddedGraph *embedding);

// DRAW MASK (for renderers): --------------------------------------------------
//
// The embedder creator sets which vertices and edges should be drawn. Defaults
// show the full subgraph. Front-ends read the mask each frame (or watch
// gvizEmbeddedGraphDrawMaskRevision) and filter geometry accordingly.

/** Sets the edge filter policy and bumps drawMask.revision. */
void gvizEmbeddedGraphSetDrawMaskEdgePolicy(gvizEmbeddedGraph *embedding,
                                            gvizDrawEdgePolicy edgePolicy);

/** Marks vertex @p u visible in the draw mask (does not bump revision). */
void gvizEmbeddedGraphDrawMaskShowVertex(gvizEmbeddedGraph *embedding, size_t u);

/** Marks vertex @p u hidden in the draw mask (does not bump revision). */
void gvizEmbeddedGraphDrawMaskHideVertex(gvizEmbeddedGraph *embedding, size_t u);

/** Clears all vertices in the draw mask (does not bump revision). */
void gvizEmbeddedGraphDrawMaskClearVertices(gvizEmbeddedGraph *embedding);

/**
 * Bumps drawMask.revision after a batch of Show/Hide calls so renderers
 * rebuild filtered geometry.
 */
void gvizEmbeddedGraphDrawMaskNotifyChanged(gvizEmbeddedGraph *embedding);

/** Resets the mask to the default (all subgraph vertices, all edges). */
void gvizEmbeddedGraphResetDrawMask(gvizEmbeddedGraph *embedding);

/** Returns the current draw mask (read-only). */
const gvizDrawMask *
gvizEmbeddedGraphGetDrawMask(const gvizEmbeddedGraph *embedding);

/** Monotonic counter; changes whenever the mask is set or reset. */
uint64_t gvizEmbeddedGraphDrawMaskRevision(const gvizEmbeddedGraph *embedding);

/**
 * Returns whether vertex @p u should be drawn under the current mask and
 * subgraph.
 */
bool gvizEmbeddedGraphIsVertexVisible(const gvizEmbeddedGraph *embedding,
                                      size_t u);

/**
 * Returns whether edge (@p u, @p v) should be drawn under the current mask
 * (both endpoints must be in the subgraph when iterating).
 */
bool gvizEmbeddedGraphIsEdgeVisible(const gvizEmbeddedGraph *embedding, size_t u,
                                    size_t v);

// ACTIONS: --------------------------------------------------------------------
//
// The creator of an embedded graph (typically an embedder) may register named
// actions on it. Front-ends discover actions by name or enumeration and invoke
// them with a payload; the front-end needs no knowledge of the embedder and
// the embedder needs no knowledge of the front-end.

/**
 * Registers an action on @p embedding. @p name is not copied and must outlive
 * the embedding (string literals are the expected usage). If an action with
 * the same name already exists, its handler and userData are replaced.
 *
 * @return 0 on success, -1 on allocation failure or NULL arguments.
 */
int gvizEmbeddedGraphAddAction(gvizEmbeddedGraph *embedding, const char *name,
                               gvizActionHandler handler, void *userData);

/** Removes the action named @p name. Returns 0 if removed, -1 if not found. */
int gvizEmbeddedGraphRemoveAction(gvizEmbeddedGraph *embedding,
                                  const char *name);

/** Returns the action named @p name, or NULL if none is registered. */
const gvizAction *gvizEmbeddedGraphFindAction(const gvizEmbeddedGraph *embedding,
                                              const char *name);

/** Returns the number of registered actions. */
size_t gvizEmbeddedGraphActionCount(const gvizEmbeddedGraph *embedding);

/** Returns the @p idx-th registered action; NULL if out of bounds. */
const gvizAction *gvizEmbeddedGraphActionAt(const gvizEmbeddedGraph *embedding,
                                            size_t idx);

/**
 * Invokes the action named @p name with @p payload (@p payload may be NULL,
 * in which case a zeroed payload is passed to the handler).
 *
 * @return 0 if the action ran, -1 if no such action is registered.
 */
int gvizEmbeddedGraphInvokeAction(gvizEmbeddedGraph *embedding,
                                  const char *name,
                                  const gvizActionPayload *payload);

// STATS: ----------------------------------------------------------------------
//
// The creator of an embedded graph may record named time series ("how did the
// mean heat evolve per round?") on it. Front-ends discover the series by
// enumeration and chart them; the front-end needs no knowledge of the embedder
// and the embedder needs no knowledge of the front-end.

/**
 * Registers a stat series on @p embedding and returns it. If a series with the
 * same name already exists, its kind is updated and it is returned unchanged.
 * @p name is not copied and must outlive the embedding (string literals are
 * the expected usage).
 *
 * @return the series, or NULL on allocation failure or NULL arguments.
 */
gvizStatSeries *gvizEmbeddedGraphAddStatSeries(gvizEmbeddedGraph *embedding,
                                               const char *name,
                                               gvizStatChartKind kind);

/**
 * Appends @p value to the series named @p name, creating it (with
 * @ref GVIZ_STAT_CHART_LINE) if it does not exist yet. This is the one-liner
 * embedders are expected to call from their inner loop.
 *
 * @return 0 on success, -1 on allocation failure or NULL arguments.
 */
int gvizEmbeddedGraphStatAppend(gvizEmbeddedGraph *embedding, const char *name,
                                double value);

/** Discards all samples of the series named @p name (the series itself stays
 *  registered). No-op if no such series exists. */
void gvizEmbeddedGraphStatClear(gvizEmbeddedGraph *embedding, const char *name);

/** Returns the number of registered stat series. */
size_t gvizEmbeddedGraphStatSeriesCount(const gvizEmbeddedGraph *embedding);

/** Returns the @p idx-th registered series; NULL if out of bounds. */
const gvizStatSeries *
gvizEmbeddedGraphStatSeriesAt(const gvizEmbeddedGraph *embedding, size_t idx);

/** Returns the series named @p name, or NULL if none is registered. */
const gvizStatSeries *
gvizEmbeddedGraphFindStatSeries(const gvizEmbeddedGraph *embedding,
                                const char *name);

/**
 * Returns non-zero when a planar rotation system has been installed (see
 * gvizPlanarApplyRotationToEmbedding in embedders/gvizPlanarEmbedder.h;
 * face queries on an embedded graph also live there).
 */
int gvizEmbeddedGraphIsPlanarEmbedded(const gvizEmbeddedGraph *embedding);

/** Replaces the highlight subgraph; takes ownership of @p highlight's subsets. */
void gvizEmbeddedGraphSetHighlight(gvizEmbeddedGraph *embedding,
                                   gvizSubgraph highlight);

/** Clears the highlight subgraph, if any. */
void gvizEmbeddedGraphClearHighlight(gvizEmbeddedGraph *embedding);

/** Returns non-zero when a highlight subgraph is set. */
int gvizEmbeddedGraphHasHighlight(const gvizEmbeddedGraph *embedding);

/** Returns the highlight subgraph, or NULL when none is set. */
const gvizSubgraph *
gvizEmbeddedGraphGetHighlight(const gvizEmbeddedGraph *embedding);

/** Returns a pointer to the @p idx-th vertex position (dim doubles). */
double *gvizEmbeddedGraphGetVPosition(gvizEmbeddedGraph *embedding, size_t idx);

/** Overwrites the @p idx-th vertex position from @p position. */
void gvizEmbeddedGraphSetVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position);

/** Adds @p position to the @p idx-th vertex position. */
void gvizEmbeddedGraphAddVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position);

/**
 * Places every vertex in @p embedding's subgraph uniformly at random inside
 * the [-boxExtent, boxExtent]^dim box, so a layout can start compact rather
 * than scattering vertices arbitrarily far apart. @p seed seeds the
 * generator; pass 0 for a time-based seed. Shared starting-layout logic for
 * embedders (see gvizForceEmbedderBegin) and front-ends that want a
 * scattered placement before running an embedder.
 */
void gvizEmbeddedGraphRandomizePositions(gvizEmbeddedGraph *embedding,
                                         double boxExtent, unsigned int seed);

/**
 * Writes vertex positions to @p filename in text format.
 *
 * @return 0 on success, -1 if the file cannot be opened.
 */
int gvizEmbeddedGraphSaveEmbedding(gvizEmbeddedGraph *embedding,
                                   const char *name, const char *filename);

/**
 * Loads vertex positions from @p filename.
 * Vertex count and dimension must match @p embedding.
 *
 * @return 0 on success, -1 on I/O error or dimension mismatch.
 */
int gvizEmbeddedGraphLoadEmbedding(gvizEmbeddedGraph *embedding,
                                   const char *filename);

#endif
