#ifndef _GVIZ_TREE_IO_H_
#define _GVIZ_TREE_IO_H_

#include "dsa/gvizGraph.h"

/*
 * Tree JSON schema (minimal, hand-rolled parser):
 *
 *   {
 *     "root": <integer>,
 *     "vertices": [
 *       { "id": <integer>, "children": [<integer>, ...] },
 *       ...
 *     ]
 *   }
 *
 * All integers must be non-negative; ids are interpreted directly as
 * vertex indices. The resulting graph is directed (parent -> child).
 *
 * Writes into @p out (which must be uninitialized). Stores the root
 * vertex index in @p outRoot if non-NULL.
 *
 * Returns 0 on success, -1 on I/O, parse, or allocation failure.
 */
int gvizLoadTreeJSON(const char *path, gvizGraph *out, size_t *outRoot);

/*
 * Emits the given directed tree to @p path using the schema above. Walks the
 * graph from @p root in BFS order. Returns 0 on success, -1 on I/O failure.
 */
int gvizSaveTreeJSON(const gvizGraph *g, size_t root, const char *path);

#endif
