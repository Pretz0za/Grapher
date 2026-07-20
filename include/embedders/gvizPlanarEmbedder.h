#ifndef GVIZ_PLANAR_EMBEDDER_H
#define GVIZ_PLANAR_EMBEDDER_H

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
 * Initializes @p context for face enumeration on @p sg, whose parent graph
 * must already carry a CCW rotation system (see
 * gvizSubgraphApplyPlanarRotation).
 *
 * @return 0 on success, -1 on allocation failure.
 */
int gvizFaceIteratorInit(const gvizSubgraph *sg,
                         gvizFaceIteratorContext *context);

/** Releases memory owned by @p context. */
void gvizFaceIteratorRelease(gvizFaceIteratorContext *context);

/**
 * Enumerates all faces of @p sg into @p context->faces.
 * Each face is stored as a gvizArray of vertex indices in CCW order.
 *
 * @return 0 on success, -1 on failure.
 */
int gvizPlanarEmbedderFaces(const gvizSubgraph *sg,
                            gvizFaceIteratorContext *context);

/**
 * Triangulates the planar graph under @p sg by adding edges across
 * non-triangular faces. Updates @p context if faces were previously
 * enumerated. @p sg must be a full subgraph; the parent layout is rebuilt
 * and @p sg is re-derived as the full subgraph of the augmented graph.
 */
void gvizPlanarEmbedderTriangulate(gvizSubgraph *sg,
                                   gvizFaceIteratorContext *context);

// PLANAR QUERIES ON AN EMBEDDED GRAPH: ----------------------------------------
//
// These operate on any gvizEmbeddedGraph (whichever embedder produced it) and
// are the planar module's knowledge, not the embedded graph's: the base type
// only stores the planarEmbedded flag these functions maintain and check.

/**
 * Tests planarity of @p embedding's subgraph and, when planar, installs a CCW
 * rotation system on the parent graph and marks the embedding planar-embedded
 * (see gvizEmbeddedGraphIsPlanarEmbedded).
 *
 * @return 0 if planar, -2 if non-planar, -1 on error.
 */
int gvizPlanarApplyRotationToEmbedding(gvizEmbeddedGraph *embedding);

typedef struct gvizFaceSearchState {
  const gvizEmbeddedGraph *embedding;
  gvizArray faces;
  size_t nextFace;
  /** CCW vertex cycle written by the last gvizPlanarNextFace call that
   *  returned 0; NULL once enumeration is exhausted. Owned by @p faces. */
  size_t *face;
  size_t faceCount;
} gvizFaceSearchState;

/**
 * Initializes @p state for incremental face enumeration on a planar-embedded
 * graph. Call gvizPlanarNextFace until it returns 1 (done).
 *
 * @return 0 on success, -1 on failure.
 */
int gvizPlanarFaceSearchInit(gvizEmbeddedGraph *embedding,
                             gvizFaceSearchState *state);

/**
 * Finds the next unvisited face and stores its CCW vertex cycle in
 * @p state->face. Returns 0 when a face was written, 1 when enumeration is
 * complete, -1 on error.
 */
int gvizPlanarNextFace(gvizFaceSearchState *state);

/**
 * Builds a full subgraph (the face's vertices, plus the boundary edges
 * connecting consecutive vertices in the cycle) for the face most recently
 * written into @p state->face by gvizPlanarNextFace. Caller must release
 * @p out.
 *
 * @return 0 on success, -1 when no face is available or on allocation
 * failure.
 */
int gvizPlanarFaceSearchSubgraph(const gvizFaceSearchState *state,
                                 gvizSubgraph *out);

/** Releases memory owned by @p state. */
void gvizPlanarFaceSearchRelease(gvizFaceSearchState *state);

/**
 * Finds the combinatorial face whose interior contains (@p worldX, @p worldY)
 * in the current straight-line drawing and writes a full subgraph describing
 * that face into @p out. Caller must release @p out.
 *
 * @return 0 on success, -1 when no face contains the point or on error.
 */
int gvizPlanarFaceSubgraphAt(gvizEmbeddedGraph *embedding,
                             double worldX, double worldY, gvizSubgraph *out);

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
