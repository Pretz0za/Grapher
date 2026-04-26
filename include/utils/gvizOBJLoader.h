#ifndef _GVIZ_UTILS_OBJ_LOADER_H_
#define _GVIZ_UTILS_OBJ_LOADER_H_

#include "dsa/gvizGraph.h"

/*
 * Load a Wavefront .obj mesh as an undirected gvizGraph of its edge skeleton.
 *
 * Vertex positions in the .obj file are NOT stored — only the count is used to
 * allocate vertices in @p outGraph. Faces (`f` lines) define the edges: for
 * each face with vertex indices v0 v1 ... vk, edges (vi, v(i+1) % k) are added,
 * deduplicated against the existing graph.
 *
 * Face vertex tokens may be of the form `idx`, `idx/uv`, `idx/uv/n`, or
 * `idx//n`; only the integer before the first `/` is consumed. The .obj
 * format uses 1-based indices; these are converted to 0-based.
 *
 * The first three vertex indices of the first `f` line are written to
 * @p outerTriangle (1-based -> 0-based) for use as a Tutte outer face.
 *
 * Returns 0 on success, -1 on parse / IO / allocation failure. On failure
 * @p outGraph is left uninitialized (any partial state is released).
 */
int gvizLoadOBJAsGraph(const char *path, gvizGraph *outGraph,
                       size_t outerTriangle[3]);

#endif
