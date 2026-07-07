#ifndef __GVIZ_GRAPH_LOADER__
#define __GVIZ_GRAPH_LOADER__

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
  int zero_based;  /**< Non-zero if vertex IDs in the file are 0-based. */
  int skip_header; /**< Non-zero to skip the first non-comment line (n m header). */
} gvizEdgesFileOptions;

/** Fills @p opts with network-repository defaults (undirected, 1-based). */
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

#endif
