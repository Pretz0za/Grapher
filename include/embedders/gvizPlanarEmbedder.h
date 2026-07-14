#ifndef _GVIZ_PLANAR_H_
#define _GVIZ_PLANAR_H_

#include "ds/gvizArray.h"
#include "ds/gvizGraph.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"

typedef struct gvizPlanarEmbedderState {
  gvizEmbeddedGraph embedding;
  gvizGraph *kuratowskiSubdivision;
} gvizPlanarEmbedderState;

/**
 * Tests planarity and, if planar, reorders adjacency lists to a CCW rotation system.
 * On failure, @p state->kuratowskiSubdivision may hold a Kuratowski subdivision.
 *
 * @return 0 if planar, -2 if non-planar, -1 on error.
 */
int gvizPlanarEmbedderInit(gvizPlanarEmbedderState *state, gvizSubgraph subgraph);

/** Releases @p state, including any Kuratowski subdivision graph. */
void gvizPlanarEmbedderRelease(gvizPlanarEmbedderState *g);

/** Computes a straight-line embedding for the planar graph in @p state. */
int gvizPlanarEmbedderEmbed(gvizPlanarEmbedderState *state);

/**
 * Applies Boyer-Myrvold to @p subgraph and, when planar, writes the CCW rotation
 * system back into the parent graph adjacency lists. Subgraph neighbors appear
 * consecutively in CCW order; any parent-graph neighbors outside the subgraph
 * are preserved after that block. Rebuilds the parent layout and @p subgraph.
 *
 * @param kuratowski  Optional out pointer; when non-NULL and the graph is
 *                    non-planar, receives an allocated Kuratowski subdivision.
 *
 * @return 0 if planar, -2 if non-planar, -1 on error.
 */
int gvizSubgraphApplyPlanarRotation(gvizSubgraph *subgraph,
                                    gvizGraph **kuratowski);

/** A directed dart in the rotation system: tail @p u, head @p v. */
typedef struct gvizPlanarHalfEdge {
  size_t u;
  size_t v;
} gvizPlanarHalfEdge;

/** Returns the CCW predecessor of @p v in @p u 's adjacency list. */
size_t gvizPlanarPrevNeighborCCW(const gvizGraph *g, size_t u, size_t v);

/** Returns the CCW successor of @p v in @p u 's adjacency list. */
size_t gvizPlanarNextNeighborCCW(const gvizGraph *g, size_t u, size_t v);

/** Returns the reverse dart @p v→@p u. */
gvizPlanarHalfEdge gvizPlanarHalfEdgeTwin(gvizPlanarHalfEdge e);

/**
 * Returns the next dart around the face to the left of @p e, using the CCW
 * rotation system restricted to @p sg.
 */
gvizPlanarHalfEdge gvizPlanarHalfEdgeNext(const gvizSubgraph *sg,
                                          gvizPlanarHalfEdge e);

typedef struct gvizPlanarFaceWalk {
  const gvizSubgraph *sg;
  gvizPlanarHalfEdge start;
  gvizPlanarHalfEdge current;
  int active;
} gvizPlanarFaceWalk;

/**
 * Starts walking the face containing dart @p start. @p start must be an edge
 * in @p sg.
 *
 * @return 0 on success, -1 if @p start is not a subgraph edge.
 */
int gvizPlanarFaceWalkBegin(const gvizSubgraph *sg, gvizPlanarHalfEdge start,
                            gvizPlanarFaceWalk *walk);

/**
 * Writes the head vertex of the current dart to @p outV and advances.
 *
 * @return 1 while the walk continues, 0 when the start dart is reached again,
 *         -1 on error.
 */
int gvizPlanarFaceWalkStep(gvizPlanarFaceWalk *walk, size_t *outV);

typedef struct gvizFaceIteratorContext {
  size_t *borders;
  GVIZ_BIT_ARRAY visited;
  gvizArray faces;
  size_t dCount;
} gvizFaceIteratorContext;

/**
 * Initializes @p context for face enumeration on the embedded planar graph in @p state.
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizFaceIteratorInit(const gvizPlanarEmbedderState *state,
                         gvizFaceIteratorContext *context);

/** Releases memory owned by @p context. */
void gvizFaceIteratorRelease(gvizFaceIteratorContext *context);

/**
 * Enumerates all faces into @p context->faces.
 * Each face is stored as a gvizArray of vertex indices in CCW order.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizPlanarEmbedderFaces(const gvizPlanarEmbedderState *state,
                             gvizFaceIteratorContext *context);

/**
 * Triangulates the planar graph in @p state by adding edges across non-triangular faces.
 * Updates @p context if faces were previously enumerated.
 */
void gvizPlanarEmbedderTriangulate(const gvizPlanarEmbedderState *state,
                                    gvizFaceIteratorContext *context);

/**
 * Traces one face starting at dart @p u with subgraph-neighbor index @p adjIdx.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizPlanarTraceFace(const gvizSubgraph *sg,
                        gvizFaceIteratorContext *context, size_t u,
                        size_t adjIdx, gvizArray *face);

/**
 * Writes the vertex cycle of the largest combinatorial face of @p g into @p out.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizPlanarLargestFaceBoundary(const gvizGraph *g, const gvizSubgraph *sg,
                                  size_t *out, size_t max, size_t *count);

#endif
