#ifndef _GVIZ_EMBEDDED_GRAPH_H_
#define _GVIZ_EMBEDDED_GRAPH_H_

#include "dsa/gvizBitArray.h"
#include "dsa/gvizSubgraph.h"

typedef struct gvizDFSState {

  size_t *bitArrayOffsets;
  GVIZ_BIT_ARRAY visitedHalfEdges;
} gvizFaceSearchState;

typedef struct gvizEmbedding {
  size_t dim;
  double *vertexPositions;
} gvizEmbedding;

typedef struct gvizEmbeddedGraph {
  gvizSubgraph subgraph;
  gvizEmbedding embedding;

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

/** Releases the position buffer owned by @p embedding. */
void gvizEmbeddedGraphRelease(gvizEmbeddedGraph *embedding);

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
