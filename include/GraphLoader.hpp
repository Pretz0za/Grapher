#ifndef GVIZ_GRAPH_LOADER_HPP
#define GVIZ_GRAPH_LOADER_HPP

#include "Graph.hpp"

#include <filesystem>

namespace gviz::io {

/**
 * Options for LoadFromEdgesFile. Default-constructs to the Network
 * Repository .edges format defaults: undirected, no header line.
 *
 * External vertex ids of any base are compacted to dense 0-based internal
 * ids (in order of first appearance in the file), so no 0-based/1-based
 * option is needed.
 */
struct EdgesFileOptions {
  bool directed = false;
  bool skipHeader = false; /**< Skip the first non-comment line (n m header). */
};

/**
 * Loads a graph from a Network Repository .edges file: one edge per line
 * ("u v" or "u v weight" -- as in the old C loader, any weight token
 * present is parsed-past but not applied; every loaded edge gets weight
 * 1.0, matching gvizGraphLoadFromEdgesFile's actual behavior, not its
 * more ambitious doc comment), optional '%' comment lines.
 *
 * @throws std::runtime_error on I/O failure (file can't be opened), a
 * malformed line, a negative vertex id, or a file with no parseable edges
 * (after any header skip) -- these are file-content problems, not the kind
 * of graph-structural/dimensional failure gviz::LayoutError (Error.hpp) is
 * for, so this deliberately doesn't reach for that hierarchy.
 */
Graph LoadFromEdgesFile(const std::filesystem::path &path, const EdgesFileOptions &opts = {});

/**
 * Loads a graph from a .gexf (Graph Exchange XML Format) file.
 *
 * Each vertex's data pointer (Graph::GetVertexData) is set to a
 * heap-allocated `std::string*` (NOT a `char*` -- see FreeVertexDataStrings)
 * holding a pretty-printed JSON string (2-space indented, in the style of
 * JSON.stringify(obj, null, 2)) with the node's label and any
 * <attvalues>/<attvalue> entries, keyed by attribute id. Both the GEXF
 * 1.2+ <attvalue for="..."> form and the older 1.1-draft <attvalue id="...">
 * form (as seen in some Gephi-exported files) are accepted. Values whose
 * declared <attribute type="..."> is integer/long/float/double or boolean
 * are emitted as unquoted JSON numbers/booleans; everything else is emitted
 * as a quoted, escaped JSON string. Node ids are matched as opaque strings
 * (per the GEXF spec).
 *
 * @p directed selects whether every parsed <edge> is added as a directed or
 * undirected edge; this ignores any defaultedgetype/type attribute in the
 * file itself, since the caller is expected to know how it wants the
 * result interpreted.
 *
 * The caller owns the per-vertex JSON strings and must call
 * FreeVertexDataStrings(g) before g is destroyed, since Graph's destructor
 * never touches vertex data (Graph doesn't know what's there -- see
 * Graph.hpp's class doc). This function frees any JSON strings it already
 * allocated before rethrowing on a mid-parse failure, so a caught exception
 * never leaks them.
 *
 * @throws std::runtime_error on I/O failure, malformed XML, a <node>
 * missing its id, an <edge> referencing an unknown node id, or a file with
 * zero <node> elements.
 */
Graph LoadFromGexfFile(const std::filesystem::path &path, bool directed);

/**
 * Deletes each vertex's data pointer as a heap-allocated `std::string*` (as
 * set by LoadFromGexfFile -- NOT a generic "any loader-owned string"
 * utility the way the old C gvizGraphFreeVertexDataStrings was, since a
 * `std::string*` and a bare `char*` require different deallocation and
 * Graph's vertex data is an untyped void*; only call this on a Graph whose
 * vertex data was populated by LoadFromGexfFile), then clears each to
 * nullptr. Vertices with a nullptr data pointer are left untouched.
 *
 * Call before a Graph loaded via LoadFromGexfFile is destroyed; Graph's
 * destructor never touches vertex data since Graph does not assume any
 * particular ownership of it.
 */
void FreeVertexDataStrings(Graph &g);

/**
 * Loads an undirected graph from a Wavefront OBJ file's topology only.
 *
 * Each OBJ vertex (`v`) becomes one graph vertex (its coordinates are not
 * read -- this is a topology loader, not a mesh loader); edges are
 * inferred from face (`f`) loops (consecutive corners and the closing
 * edge). Texture/normal indices on face tokens are ignored. Supports
 * 1-based and negative relative vertex indices per the OBJ spec.
 *
 * @throws std::runtime_error on I/O failure, a face referencing a vertex
 * index that is zero, out of range, or unparseable, or a file with no `v`
 * lines at all.
 */
Graph LoadFromObjFile(const std::filesystem::path &path);

} // namespace gviz::io

#endif
