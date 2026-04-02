#ifndef __GRAPH_H__
#define __GRAPH_H__

#include "core/alloc.h"
#include "dsa/gvizBitArray.h"
#include "gvizArray.h"

#define MAX_LINE_SIZE 4096

/**
 * @brief A position on the terminal screen to print to, using ANSI escape
 * codes.
 */
typedef struct Position {
  int x,
      y; /**< The x and y positions to be passed into the ANSI escape code. */
} Position;

/**
 * @brief A Vertex's data for an adjacency list graph data structure.
 *
 * This structure represens a single vertex in a Graph.
 * The memory pointed to by data and neighbors is owned by the Vertex.
 * The adjacency list holds the indices of any vertex v such that there is an
 * edge (u, v) in the Graph.
 */
typedef struct Vertex {
  void *data;          /**< Pointer to the Vertex's data. */
  gvizArray neighbors; /**< Dynamically allocated vector for adjacency list. */
} gvizVertex;

/**
 * @brief A definition of a directed or undirected Graph data structure.
 *
 * Uses adjacency lists stored in each Vertex to store edges.
 * The memory of the underlying Vertex array is owned by the Graph.
 * The size of the underlying Vertex array is fixed. Once full, no more
 * vertices may be added to the graph.
 */
typedef struct {
  gvizArray vertices; /**< The list of all vertices in the graph. */
  size_t *map;        /**< If this is a subgraph, this maps to the indices to
                        their corresponding image in the original graph. */
  int directed;       /**< Whether or not the graph is directed. */
} gvizGraph;

// GRAPH CONSTRUCTION: ---------------------------------------------------------

/**
 * Allocates the adjacency array and initalizes the Vector data.
 *
 * @param v    A pointer to the memory to initialize.
 * @param data Where the Vertex's data attribute will point to.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0  If the operation was successful
 * @retval -1 If the allocation failed or v is NULL.
 */
int gvizVertexInit(gvizVertex *v, void *data);
int gvizVertexInitAtCapacity(gvizVertex *v, void *data, size_t initialCapacity);

/**
 * Copies the @p src into it. @p dest will manage its own adjacency list.
 *
 * @param dest A pointer to the destination Vertex memory address.
 * @param src  A pointer to the source Vertex memory address.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0 If the operation was successfu.
 * @retval -1 If reallocation failed or src or dest is NULL.
 */
int gvizVertexCopy(gvizVertex *dest, const gvizVertex *src);

/**
 * Initializes @p dest as a vertex. Copies the @p src into it. @p dest will
 * manage its own adjacency list.
 *
 * @param dest A pointer to the destination Vertex memory address.
 * @param src  A pointer to the source Vertex memory address.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0 If the operation was successfu.
 * @retval -1 If reallocation failed or src or dest is NULL.
 */
int gvizVertexClone(gvizVertex *dest, const gvizVertex *src);

/**
 * Allocates the vertex array and initializes a directed or undirected Graph.
 * The underlying Vertex array is initilized to hold 64 vertices by default.
 * The capacity doubles when the array is full and a new vertex is added.
 *
 * @param directed whether or not it is a directed graph. 1 for directed.
 *
 * @return a
 */
int gvizGraphInit(gvizGraph *g, int directed);
int gvizGraphInitAtCapacity(gvizGraph *g, int directed, size_t initialCapacity);

/**
 * Copies the Graph data of @p src to @p dest.
 *
 * @param dest A pointer to the Graph the data will be copied into.
 * @param src  A pointer to the Graph the data will be copied from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful.
 * @retval -1 If a reallocation fails.
 */
int gvizGraphCopy(gvizGraph *dest, const gvizGraph *src);

/**
 * Initializes @p dest as a Graph. Copies the Graph data of @p src to @p dest.
 *
 * @param dest A pointer to the Graph the data will be copied into.
 * @param src  A pointer to the Graph the data will be copied from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful.
 * @retval -1 If a reallocation fails.
 */
int gvizGraphClone(gvizGraph *dest, const gvizGraph *src);

/**
 * Copies the reverse of the Graph data of @p src to @p dest. An edge (u, v) in
 * @p src will correspond to (v, u) in @p dest.
 *
 * @param dest A pointer to the Graph the data will be copied into.
 * @param src  A pointer to the Graph the data will be copied from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful.
 * @retval -1 If a reallocation fails.
 */
int gvizGraphCopyReversed(gvizGraph *dest, const gvizGraph *src);

/**
 * Initializes @p dest as a Graph. copies the reverse of the Graph data of @p
 * src to @p dest. An edge (u,v) in @p src will correspond to (v,u) in @p dest.
 *
 * @param dest A pointer to the Graph the data will be copied into.
 * @param src  A pointer to the Graph the data will be copied from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful.
 * @retval -1 If a reallocation fails.
 */
int gvizGraphCloneReversed(gvizGraph *dest, const gvizGraph *src);

/**
 * Attemps to initialize and add a vertex to a graph.
 *
 * @param g    A pointer to the Graph the vertex will be added to.
 * @param data The address the vertex's data attribute will point to.
 * @param in   (NULL?) Array with indices of vertices with edges to v.
 * @param out  (NULL?) Array with indices of vertices with edges from v.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0  If the Vertex is created and added successfully.
 * @retval -1 If a reallocation fails.
 */
int gvizGraphAddVertex(gvizGraph *g, void *data, gvizArray *in, gvizArray *out);

/**
 * Destroys all vertices currently in a given Graph.
 *
 * @param g A pointer to the Graph to be cleared.
 */
void gvizGraphClear(gvizGraph *g);

/**
 * Attemps to add an edge (u, v) to a Graph. If @p g is undirected, u will
 * also be added to v's adjacency list. May reallocate to increase the size of
 * affected vertex/vertices.
 *
 * @param g    A pointer to the Graph the vertex will be added to.
 * @param from The index of u in g->vertices.
 * @param to   The index of v in g->vertices.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the edge was created successfully.
 * @retval -1 If (from OR to is out of bounds) OR reallocation fail.
 */
int gvizGraphAddEdge(gvizGraph *g, size_t from, size_t to);

/**
 * Attempts to remove and edge (u, v) from a Graph. If @p g is undirected, u
 * will also be removed from v's adjacency list.
 *
 * @param g    A pointer to the Graph to remove the edge from.
 * @param from The index of u in @p g->vertices.
 * @param to   The index of v in @p g->vertices.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the edge was removed successfully.
 * @retval -1 If (from OR to is out of bounds) OR edge not found.
 */
int gvizGraphRemoveEdge(gvizGraph *g, size_t from, size_t to);

// DATA ACCESS:
// ----------------------------------------------------------------

/**
 * Changes the address a vertex's data points to in a graph.
 *
 * @param g    A pointer to the graph owning the vertex.
 * @param idx  The index of the vertex in @p g->vertices.
 * @param data The address of the new data to store
 */
void gvizGraphSetVertexData(gvizGraph *g, size_t idx, void *data);

/**
 * Gets the adjacency list of a Vertex in a given Graph.
 *
 * @param g   A pointer to the Graph the Vertex is in.
 * @param idx The index of the Vertex in @p g->vertices.
 *
 * @return A pointer to a Vector holding the adjacency list
 * @retval ptr  Pointer to the successfully retrieved adjacency list.
 * @retval NULL If the Vertex index is out of bounds.
 */
gvizArray *gvizGraphGetVertexNeighbors(const gvizGraph *g, size_t idx);

/**
 * Checks if there exists an edge (u, v) in a Graph.
 *
 * @param g    A pointer to the Graph the vertices are in.
 * @param from The index of u in @p g->vertices.
 * @param to   The index of v in @p g->vertices.
 *
 * @return An integer signaling the result.
 * @retval 0  There is no edge from u to v in g.
 * @retval 1  There is an edge from u to v in g.
 * @retval -1 The indices of u and/or v are out of bounds.
 */
int gvizGraphEdgeExists(gvizGraph *g, size_t from, size_t to);

/**
 * Gets the data from a Vertex in a given Graph.
 *
 * @param g   A pointer to the Graph the vertex is in.
 * @param idx The index of the Vertex in @p g->vertices.
 *
 * @return A pointer to the address holding the data.
 * @retval ptr  Pointer to the successfully retrieved data.
 * @retval NULL If the Vertex index is out of bounds.
 */
void *gvizGraphGetVertexData(gvizGraph *g, size_t idx);

// MEMORY FREEING:
// -------------------------------------------------------------

/**
 * Releases all the data that is controlled by a vertex.
 *
 * @param v        A pointer to the Vertex to free. The vertex data stored at
 *                 the address will become invalid.
 */
void gvizVertexRelease(gvizVertex *v);

/**
 * Releases all the data the is controlled by a graph.
 *
 * @param g A pointer to the Graph to free. This pointer will become
            invalid after the function returns
 */
void gvizGraphRelease(gvizGraph *g);

// GRAPH ALGORITHMS:
// -----------------------------------------------------------

/**
 * Performs a Depth First Search on a Graph from a given source. The resulting
 * DFS tree is initialized and stored in @p out. the map attribute of @p out
 * will map vertex indices from @p out vertex array to @p g vertex array.
 *
 * @param g    A pointer to the Graph to search.
 * @param out  The address to store and initalize the resulting DFS tree at.
 * @param from The index of the vertex to begin the search from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  The tree was calculated and stored in @p out successfully.
 * @retval -1 Reallocation failure.
 */
int gvizGraphDFSTree(gvizGraph *g, gvizGraph *out, size_t source);

/**
 * Performs a Breadth First Search on a Graph from a given source. The resulting
 * BFS tree is initialized and stored in @p out. the map attribute of @p out
 * will map vertex indices from @p out vertex array to @p g vertex array.
 * Sets the data pointer of each vertex in @p out to its depth. The pointer is
 * invalid! however interpreting the address as a size_t will yield the depth of
 * the vertex.
 *
 * @param g    A pointer to the Graph to search.
 * @param out  The address to store and initalize the resulting BFS tree at.
 * @param from The index of the vertex to begin the search from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  The tree was calculated and stored in @p out successfully.
 * @retval -1 Reallocation failure.
 */
int gvizGraphBFSTree(gvizGraph *g, gvizGraph *out, size_t source,
                     size_t maxDepth, int invMap);

/**
 * Finds the K nearest vertices, belonging to a filter, from a source vertex in
 * a graph. Uses graph distance (number of edges used), not euclidian.
 *
 * @param g A pointer to the Graph to search
 * @param out A pointer to an array of size at least @p k to store the results
 * @param k The number of nearest neighbors to find
 * @param source The index of the vertex to start the search from
 * @param filter A bit array indicating which vertices are being searched for
 *
 * @return The number of neighbors found, a negative number if an error occured.
 * @retval n  The number of vertices reached that satisfy the filter
 * @retval <0 Allocation failiure
 *
 * @note If filter is NULL then all vertices of the graph are considered,
 * otherwise, a BFS is performed until K vertices in filter are found.
 */
int gvizGraphKNearestNeighbors(gvizGraph *g, size_t *out, size_t k,
                               size_t source, GVIZ_BIT_ARRAY filter);

// VISUALIZATION:
// --------------------------------------------------------------

/**
 * Writes a visualization of a DFS tree to the specified output stream.
 *
 * @param g       A pointer to the Graph holding the DFS tree to be rendered.
 * @param strings An array of the string the string representation of each
 *                Vertex. If NULL will print Vertex indices.
 *
 * @param stream  A pointer to the output stream.
 *
 * @note The terminal window should be zoomed out for large graphs if this
 *       function is called, otherwise the output will overlap with itself.
 */
void gvizPrintDFSTree(gvizGraph *tree, char *strings[], FILE *stream);

#endif
