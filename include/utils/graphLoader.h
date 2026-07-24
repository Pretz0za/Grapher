#ifndef GVIZ_GRAPH_LOADER_H
#define GVIZ_GRAPH_LOADER_H

#include "ds/gvizGraph.h"

/**
 * Options for gvizGraphLoadFromEdgesFile.
 *
 * Matches the Network Repository .edges format by default: one edge per line
 * ("u v" or "u v weight"), optional '%' comment lines, 1-based vertex IDs,
 * undirected edges.
 */
typedef struct gvizEdgesFileOptions {
  int directed;    /**< Non-zero for directed edges; 0 for undirected. */
  int skip_header; /**< Non-zero to skip the first non-comment line (n m header). */
} gvizEdgesFileOptions;

/**
 * Fills @p opts with network-repository defaults (undirected, no header).
 * External vertex ids of any base are compacted to dense 0-based internal
 * ids, so no 0-based/1-based option is needed.
 */
void gvizEdgesFileOptionsInit(gvizEdgesFileOptions *opts);

/**
 * Loads a graph from a .edges file into @p out.
 *
 * @p out must be uninitialized on entry. On failure, any partial state is
 * released and @p out is zeroed.
 *
 * @return 0 on success, -1 on I/O, parse, or allocation failure.
 */
int gvizGraphLoadFromEdgesFile(const char *path,
                               const gvizEdgesFileOptions *opts, gvizGraph *out);

/**
 * Loads a graph from a .gexf (Graph Exchange XML Format) file into @p out.
 *
 * Each vertex's data pointer is set to a heap-allocated, null-terminated,
 * pretty-printed JSON string (2-space indented, in the style of
 * JSON.stringify(obj, null, 2)) holding the node's label and any
 * <attvalues>/<attvalue> entries, keyed by attribute id. Both the GEXF
 * 1.2+ <attvalue for="..."> form and the older 1.1-draft
 * <attvalue id="..."> form (as seen in some Gephi-exported files) are
 * accepted. Values whose
 * declared <attribute type="..."> is integer/long/float/double or boolean
 * are emitted as unquoted JSON numbers/booleans; everything else is emitted
 * as a quoted, escaped JSON string. Edges are treated as undirected. Node
 * ids are matched as opaque strings (per the GEXF spec).
 *
 * @p out must be uninitialized on entry. On failure, any partial state
 * (including any per-vertex JSON strings already allocated) is released and
 * @p out is zeroed.
 *
 * The caller owns the per-vertex JSON strings and must free them with
 * gvizGraphFreeVertexDataStrings(out) before gvizGraphRelease(out), since
 * gvizGraphRelease never touches vertex data.
 *
 * @return 0 on success, -1 on I/O, parse, or allocation failure.
 */
int gvizGraphLoadFromGexfFile(const char *path, gvizGraph *out);

/**
 * Frees each vertex's data pointer as a heap-allocated, null-terminated
 * string, then clears it to NULL. Vertices with a NULL data pointer are left
 * untouched. Not GEXF-specific: usable for any graph whose vertex data holds
 * loader-owned strings (e.g. from gvizGraphLoadFromGexfFile).
 *
 * Call before gvizGraphRelease(g); gvizGraphRelease never touches vertex
 * data since gvizGraph does not assume any particular ownership of it.
 *
 * @param g A pointer to the Graph whose vertex-data strings should be freed.
 */
void gvizGraphFreeVertexDataStrings(gvizGraph *g);

/**
 * Loads an undirected graph from a Wavefront OBJ file.
 *
 * Each OBJ vertex (`v`) becomes one graph vertex; edges are inferred from face
 * (`f`) loops (consecutive corners and the closing edge). Texture/normal indices
 * on face tokens are ignored. Supports 1-based and negative relative vertex
 * indices per the OBJ spec.
 *
 * @p out must be uninitialized on entry. On failure, any partial state is
 * released and @p out is zeroed.
 *
 * @return 0 on success, -1 on I/O, parse, or allocation failure.
 */
int gvizGraphLoadFromObjFile(const char *path, gvizGraph *out);

#endif
