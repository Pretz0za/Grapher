#ifndef _GVIZ_UTILS_OBJ_LOADER_H_
#define _GVIZ_UTILS_OBJ_LOADER_H_

#include "dsa/gvizGraph.h"
#include "raylib.h"

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
 * The first vertices of the first `f` line are written to @p outerTriangle
 * (1-based -> 0-based) for use as a Tutte outer face. The full length of that
 * first face (capped at 8) is written to @p outerFaceLen so callers can pin
 * the entire outer polygon, not just three vertices. @p outerTriangle must be
 * sized to at least 8 entries.
 *
 * Returns 0 on success, -1 on parse / IO / allocation failure. On failure
 * @p outGraph is left uninitialized (any partial state is released).
 */
int gvizLoadOBJAsGraph(const char *path, gvizGraph *outGraph,
                       size_t outerTriangle[8], size_t *outerFaceLen);

/*
 * Load a Wavefront .obj file as a renderable raylib Model. Thin wrapper
 * around raylib's `LoadModel` — kept here so callers can include a
 * single project header rather than reaching into raylib directly, and
 * so future loader behavior (e.g. centering, normalization) can be
 * folded in without touching call sites.
 *
 * Returns 0 on success, -1 if raylib failed to load any meshes from
 * the file. On success the caller owns @p outModel and must release it
 * via raylib's `UnloadModel`.
 */
int gvizLoadOBJAsMesh(const char *path, Model *outModel);

#endif
