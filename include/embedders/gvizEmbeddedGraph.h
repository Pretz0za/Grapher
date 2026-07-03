#ifndef _GVIZ_EMBEDDED_GRAPH_H_
#define _GVIZ_EMBEDDED_GRAPH_H_

#include "ds/gvizBitArray.h"
#include "ds/gvizSubgraph.h"
#include <stdint.h>

typedef struct gvizDFSState {

  size_t *bitArrayOffsets;
  GVIZ_BIT_ARRAY visitedHalfEdges;
} gvizFaceSearchState;

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

typedef struct gvizEmbeddedGraph {
  gvizSubgraph subgraph;
  gvizEmbedding embedding;
  gvizActionRegistry actions;

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

/** Walks to the next face in a planar rotation system. */
int gvizEmbeddedGraphNextFace(gvizEmbeddedGraph *embedding,
                              gvizFaceSearchState *gvizDFSSate);

/** Returns a pointer to the @p idx-th vertex position (dim doubles). */
double *gvizEmbeddedGraphGetVPosition(gvizEmbeddedGraph *embedding, size_t idx);

/** Overwrites the @p idx-th vertex position from @p position. */
void gvizEmbeddedGraphSetVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position);

/** Adds @p position to the @p idx-th vertex position. */
void gvizEmbeddedGraphAddVPosition(gvizEmbeddedGraph *embedding, size_t idx,
                                   double *position);

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
