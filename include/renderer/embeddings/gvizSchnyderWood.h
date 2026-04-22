#ifndef _GVIZ_SCHNYDER_WOOD_H_
#define _GVIZ_SCHNYDER_WOOD_H_

#include "dsa/gvizGraph.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include <stddef.h>

/** Sentinel meaning "no parent" (root vertex, or edge excluded from this tree).
 */
#define GVIZ_SW_NONE ((size_t)-1)

/**
 * A Schnyder wood (realizer) for a triangulated planar graph.
 *
 * Given a triangulated planar graph G with a designated outer-face triangle
 * (root[0], root[1], root[2]) and a valid CCW rotation system, a Schnyder
 * wood decomposes all edges except the base edge root[0]–root[1] into three
 * directed trees T0, T1, T2:
 *
 *   Ti is rooted at root[i].
 *
 *   parent[i][v] is the parent of v in Ti, or GVIZ_SW_NONE when v has no
 *   parent in Ti (i.e. v == root[i], or the edge is the excluded base edge).
 *
 * Properties:
 *   - Every interior vertex (not in {root[0], root[1], root[2]}) has exactly
 *     one outgoing edge in each of T0, T1, and T2.
 *   - root[2] has parent[0][root[2]] = root[0] and parent[1][root[2]] = root[1]
 *     (the two non-base outer-triangle edges).
 *   - root[0] and root[1] have GVIZ_SW_NONE for all tree parents (the base
 *     edge root[0]–root[1] is excluded from every tree).
 */
typedef struct gvizSchnyderWood {
  size_t n;          /**< Number of vertices (same as the input graph). */
  size_t root[3];    /**< root[i] = root vertex of tree Ti.             */
  size_t *parent[3]; /**< parent[i][v] = parent of v in Ti.             */
} gvizSchnyderWood;

/**
 * Constructs a Schnyder wood for a triangulated planar graph @p g.
 *
 * @p g must carry a valid CCW rotation system (cyclic adjacency lists) such
 * as one produced by gvizPlanarEmbeddingInit() followed by
 * gvizPlanarEmbeddingTriangulate().  Every face of @p g must be a triangle.
 *
 * The outer-face triangle is inferred automatically: vertex 0 is root[0],
 * its first adjacency-list neighbour is root[1], and the third vertex of the
 * face traced by the dart root[0]→root[1] is root[2].
 *
 * @param sw  Output — must point to uninitialised storage.
 * @param g   A triangulated undirected planar graph with a CCW rotation system.
 * @return    0 on success, -1 on allocation failure or invalid input.
 */
int gvizSchnyderWoodInit(gvizSchnyderWood *sw, const gvizGraph *g);

/**
 * Releases all memory owned by @p sw.
 */
void gvizSchnyderWoodRelease(gvizSchnyderWood *sw);

void gvizSchnyderWoodEmbed(const gvizSchnyderWood *sw,
                           gvizEmbeddedGraph *embedding);

#endif /* _GVIZ_SCHNYDER_WOOD_H_ */
