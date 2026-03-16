#ifndef __GRAPH_H__
#define __GRAPH_H__

#include "uLongArray.h"

#define MAX_LINE_SIZE 4096

#ifndef GRAPH_ALLOC
#define GRAPH_ALLOC(x) malloc(x)
#endif

#ifndef GRAPH_REALLOC
#define GRAPH_REALLOC(x, y) realloc(x, y)
#endif

#ifndef GRAPH_DEALLOC
#define GRAPH_DEALLOC(x) free(x)
#endif

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
  void *data;           /**< Pointer to the Vertex's data. */
  ULongArray neighbors; /**< Dynamically allocated vector for adjacency list. */
} Vertex;

/**
 * @brief A definition of a directed or undirected Graph data structure.
 *
 * Uses adjacency lists stored in each Vertex to store edges.
 * The memory of the underlying Vertex array is owned by the Graph.
 * The size of the underlying Vertex array is fixed. Once full, no more
 * vertices may be added to the graph.
 */
typedef struct {
  Vertex *vertices; /**< The list of all vertices in the graph. */
  int *map;         /**< If this is a subgraph, this maps to the indices of
                         root graph. */
  size_t count;     /**< The current number of vertices in the graph. */
  size_t size;      /**< The maximum number of vertices in the graph. */
  int directed;     /**< Whether or not the graph is directed. */
} Graph;

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
int vertexInit(Vertex *v, void *data);
int vertexInitAtCapacity(Vertex *v, void *data, size_t initialCapacity);

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
int vertexCopy(Vertex *dest, const Vertex *src);

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
int vertexClone(Vertex *dest, const Vertex *src);

/**
 * Allocates the vertex array and initializes a directed or undirected Graph.
 * The underlying Vertex array is initilized to hold 64 vertices by default.
 * The capacity doubles when the array is full and a new vertex is added.
 *
 * @param directed whether or not it is a directed graph. 1 for directed.
 *
 * @return a
 */
int graphInit(Graph *g, int directed);
int graphInitAtCapacity(Graph *g, int directed, size_t initialCapacity);

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
int graphCopy(Graph *dest, const Graph *src);

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
int graphClone(Graph *dest, const Graph *src);

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
int graphCopyReversed(Graph *dest, const Graph *src);

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
int graphCopyReversed(Graph *dest, const Graph *src);

/**
 * Attemps to add an already initialized vertex to a graph. This does not
 * modify the adjacency list of @p v, even if @p is non NULL and @p g is
 * undirected.
 *
 * @param g    A pointer to the Graph the vertex will be added to.
 * @param data The address the vertex's data attribute will point to.
 * @param in   (NULL?) Array with indices of vertices with edges to @p v.
 *
 * @return An error code showing whether or not the operation was successful.
 * @retval 0  If the Vertex is created and added successfully.
 * @retval -1 If a reallocation fails.
 */
int graphAddVertex(Graph *g, void *data, ULongArray *in, ULongArray *out);

/**
 * Destroys all vertices currently in a given Graph.
 *
 * @param g A pointer to the Graph to be cleared.
 */
void graphClear(Graph *g);

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
int graphAddEdge(Graph *g, size_t from, size_t to);

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
int graphRemoveEdge(Graph *g, size_t from, size_t to);

// DATA ACCESS:
// ----------------------------------------------------------------

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
ULongArray *graphGetVertexNeighbors(const Graph *g, size_t idx);

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
int graphEdgeExists(Graph *g, size_t from, size_t to);

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
void *graphGetVertexData(Graph *g, size_t idx);

// MEMORY FREEING:
// -------------------------------------------------------------

/**
 * Releases all the data that is controlled by a vertex.
 *
 * @param v        A pointer to the Vertex to free. The vertex data stored at
 *                 the address will become invalid.
 */
void vertexRelease(Vertex *v);

/**
 * Releases all the data the is controlled by a graph.
 *
 * @param g A pointer to the Graph to free. This pointer will become
            invalid after the function returns
 */
void graphRelease(Graph *g);

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
int graphDFSTree(Graph *g, Graph *out, size_t source);

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
void printDFSTree(Graph *tree, char *strings[], FILE *stream);

#endif
