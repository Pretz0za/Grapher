#ifndef GRAPH
#define GRAPH

#include "stack.h"
#include <stddef.h>
#include <stdio.h>

/**
 * @brief A Vertex's data for an adjacency list graph data structure.
 *
 * This structure represens a single vertex in a Graph.
 * The memory pointed to by data and neighbors is owned by the Vertex.
 * The adjacency list holds the indices of any vertex v such that there is an
 * edge (u, v) in the Graph.
 */
typedef struct Vertex {
  Vector *neighbors; /**< Dynamically allocated vector for adjacency list. */
  void *data;        /**< Pointer to the Vertex's data. */
} Vertex;

/**
 * @brief Copies a vertex payload.
 *
 * Creates a deep copy of the data pointed to by @p data.
 *
 * @param data Pointer to the source payload.
 *
 * @return Pointer to a newly allocated copy of the payload.
 *         The caller becomes responsible for freeing it.
 */
typedef void *(*DataCopyFn)(const void *data);

/**
 * @brief Frees a vertex payload.
 *
 * Releases memory allocated for a payload copy.
 *
 * @param data Pointer to the payload to free.
 */
typedef void (*DataFreeFn)(void *data);

/**
 * @brief A definition of a directed or undirected Graph data structure.
 *
 * Uses adjacency lists stored in each Vertex to store edges.
 * The memory of the underlying Vertex array is owned by the Graph.
 * The size of the underlying Vertex array is fixed. Once full, no more vertices
 * may be added to the graph.
 */
typedef struct {
  Vertex **vertices; /**< The list of all vertices in the graph. */
  size_t count;      /**< The current number of vertices in the graph. */
  size_t size;       /**< The maximum number of vertices in the graph. */
  int directed;      /**< Whether or not the graph is directed. */

  DataCopyFn copyData; /**< A pointer to the Vertex payload copy function. */
  DataFreeFn freeData; /**< A pointer to the Vertex payload free function. */

} Graph;

/**
 * Allocates and returns a directed or undirected Graph. The underlying Vertex
 * array has fixed size equal to vertexCount.
 *
 * @param vertexCount the maximum number of vertices the Graph can hold. Can be
 *                    0. In which case, @p g ->vertices will be set to NULL.
 * @param directed    whether or not it is a directed graph. Non-zero for
 *                    directed.
 * @param copyData    a pointer to the Vertex payload copy function.
 * @param freeData    a pointer to the Vertex payload free function.
 *
 * @return A pointer to the newly allocated Graph.
 */
[[nodiscard]] Graph *createGraph(size_t vertexCount, int directed,
                                 DataCopyFn copyData, DataFreeFn freeData);

/**
 * Attemps to allocate and add a Vertex to a Graph.
 *
 * @param g    a pointer to the Graph the vertex will be added to.
 * @param data a pointer to the data to be stored in the Vertex.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0 If the Vertex is created and added successfully.
 * @retval 1 If the Graph @p g is at maximum capacity. Does not allocate memory.
 */
int addVertex(Graph *g, void *data);

/**
 * Destroys all vertices currently in a given Graph.
 *
 * @param g a pointer to the Graph to be cleared.
 */
void clearVertices(Graph *g);

/**
 * Attemps to add an edge (u, v) to a Graph. If @p g is undirected, u will also
 * be added to v's adjacency list. May reallocate to increase the size of
 * affected vertex/vertices.
 *
 * @param g    a pointer to the Graph the vertex will be added to.
 * @param from the index of u in g->vertices.
 * @param to   the index of v in g->vertices.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the edge was created successfully.
 * @retval -1 If the indices of u and/or v are out of bounds.
 */
int addEdge(Graph *g, size_t from, size_t to);

/**
 * Attempts to remove and edge (u, v) from a Graph. If @p g is undirected, u
 * will also be removed from v's adjacency list.
 *
 * @param bool g a pointer to the Graph to remove the edge from.
 * @param from   the index of u in g->vertices.
 * @param to     the index of v in g->vertices.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the edge was removed successfully.
 * @retval 1 If the edge did not exist in the Graph.
 * @retval -1 If the indices of u and/or v are out of bounds.
 */
int removeEdge(Graph *g, size_t from, size_t to);

/**
 * Gets the data from a Vertex in a given Graph.
 *
 * @param g   a pointer to the Graph the vertex is in.
 * @param idx the index of the Vertex in g->vertices.
 *
 * @return a pointer to the address holding the data.
 * @retval ptr  Pointer to the successfully retrieved data.
 * @retval NULL If the Vertex index is out of bounds.
 */
void *getVertexData(Graph *g, size_t idx);

/**
 * Gets the adjacency list of a Vertex in a given Graph.
 *
 * @param g   a pointer to the Graph the Vertex is in.
 * @param idx the index of the Vertex in g->vertices.
 *
 * @return A pointer to a Vector holding the adjacency list
 * @retval ptr  Pointer to the successfully retrieved adjacency list.
 * @retval NULL If the Vertex index is out of bounds.
 */
Vector *neighbors(Graph *g, size_t idx);

/**
 * Checks if there exists an edge (u, v) in a Graph.
 *
 * @param g    a pointer to the Graph the vertices are in.
 * @param from the index of u in g->vertices.
 * @param to   the index of v in g->vertices.
 *
 * @return An integer signaling the result.
 * @retval 0 There is no edge from u to v in g.
 * @retval 1 There is an edge from u to v in g.
 * @retval -1 The indices of u and/or v are out of bounds.
 */
int adjacent(Graph *g, size_t from, size_t to);

/**
 * Copies the Graph in dest to the Graph in src. May allocate additional
 * memory for vertices or edges. May destroy some vertices.
 *
 * @param dest a pointer to the Graph the data will be copied into.
 * @param src  a pointer to the Graph the data will be copied from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful.
 * @retval 1  If size of the Graph in @p dest does not fit the vertices in @p
 *            src.
 * @retval -1 If @p dest and @p src do not have the same DataFreeFn and
 *            DataCopyFn.
 *
 * @note @p dest and @p src MUST have the same DataFreeFn and DataCopyFn to
 * safely copy.
 */
int copyGraph(Graph *dest, Graph *src);

/**
 * Copies the Graph in dest to the Graph in src and reverses the edges. May
 * allocate additional memory for vertices or edges. May destroy some
 * vertices.
 *
 * @param dest a pointer to the Graph the data will be copied into.
 * @param src  a pointer to the Graph the data will be copied from.
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0  If the operation was successful.
 * @retval 1  If size of the Graph in @p dest does not fit the vertices in @p
              src.
 * @retval -1 If @p dest and @p src do not have the same DataFreeFn and
 *            DataCopyFn.
 *
 * @note @p dest and @p src MUST have the same DataFreeFn and DataCopyFn to
 * safely copy.
 */
Graph *copyReversedGraph(Graph *g);

/**
 * Frees a Vertex's memory and any memory it managed.
 *
 * @param v        a pointer to the Vertex to free.
 * @param freeData a pointer to a function that frees the payload.
 */
void destroyVertex(Vertex *v, DataFreeFn freeData);

/**
 * Fress a Graph's memory and any memory it managed.
 *
 * @param g a pointer to the Graph to free.
 */
void destroyGraph(Graph *g);

/**
 * Performs a Depth First Search on a Graph.
 *
 * @param g    a pointer to the Graph to search.
 * @param from the index of the vertex to begin the search from.
 *
 * @return The expansion order of the vertices in found by the DFS.
 * @retval vec A Vector holding the indices of the expanded vertices, in
 * order.
 */
[[nodiscard]] Vector *DepthFirstSearch(Graph *g, size_t from);

/**
 * Writes a visualization of a DFS tree to the specified output stream.
 *
 * @param g a pointer to the Graph the DFS was performed on.
 * @param dfs a pointer to the Vector holding the expansion order of the DFS.
 * @param stream a pointer to the output stream.
 */
void printDFSTree(Graph *g, Vector *dfs, FILE *stream);

#endif
