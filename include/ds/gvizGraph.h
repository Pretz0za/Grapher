#ifndef GVIZ_GRAPH_H
#define GVIZ_GRAPH_H

#include "gvizArray.h"
#include "gvizSubgraph.h"
#include <stdint.h>

/**
 * @brief A single directed adjacency-list entry: the neighboring vertex's
 * index and the weight of the edge to it.
 */
typedef struct {
  size_t idx;   /**< Index of the neighboring vertex in the Graph. */
  double weight; /**< Weight of the edge to that vertex. */
} gvizEdge;

/**
 * @brief A Vertex's data for an adjacency list graph data structure.
 *
 * This structure represens a single vertex in a Graph.
 * The memory pointed to by data and edges is owned by the Vertex.
 * The adjacency list holds a gvizEdge for any vertex v such that there is an
 * edge (u, v) in the Graph.
 */
typedef struct Vertex {
  void *data;      /**< Pointer to the Vertex's data. */
  gvizArray edges; /**< Dynamically allocated vector of gvizEdge (adjacency list). */
} gvizVertex;

/**
 * @brief A definition of a directed or undirected Graph data structure.
 *
 * Uses adjacency lists stored in each Vertex to store edges.
 * The memory of the underlying Vertex array is owned by the Graph.
 * The size of the underlying Vertex array is fixed. Once full, no more
 * vertices may be added to the graph.
 */
typedef struct gvizGraph {
  gvizArray vertices; /**< The list of all vertices in the graph. */
  size_t *map;        /**< If this is a subgraph, this maps to the indices to
                        their corresponding image in the original graph. */
  int directed;       /**< Whether or not the graph is directed. */
  /** Shared edge layout; NULL until gvizGraphBuildLayout. Not auto-updated. */
  gvizGraphLayout *layout;
  /**
   * Monotonic counter bumped by every structural mutation (AddVertex,
   * AddEdge, RemoveEdge, InsertNeighborAt, Clear). Lets a derived view
   * (e.g. gvizEmbeddedGraph's synced snapshot) answer "has the graph
   * changed since I last looked?" with one integer compare instead of
   * re-walking the structure. Not bumped by data/weight updates or
   * adjacency reordering, which change no topology.
   */
  uint64_t mutationCount;
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

/**
 * Like gvizVertexInit, but pre-allocates the adjacency list to @p initialCapacity.
 *
 * @return 0 on success, -1 if @p v is NULL or allocation fails.
 */
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

/**
 * Like gvizGraphInit, but pre-allocates the vertex array to @p initialCapacity.
 *
 * @return 0 on success, -1 if @p g is NULL or allocation fails.
 */
int gvizGraphInitAtCapacity(gvizGraph *g, int directed, size_t initialCapacity);

/** Returns the number of vertices in @p g. */
size_t gvizGraphSize(const gvizGraph *g);

/** Returns whether @p g is directed. */
int gvizGraphIsDirected(const gvizGraph *g);

/** Returns the structural mutation counter -- see the mutationCount field. */
uint64_t gvizGraphMutationCount(const gvizGraph *g);

/**
 * Returns the number of edges in @p g. Requires a current layout from
 * gvizGraphBuildLayout; returns 0 if @p g->layout is NULL.
 */
size_t gvizGraphEdgeCount(const gvizGraph *g);

/**
 * Builds or rebuilds the shared edge-bitset layout for @p g.
 * Allocates @p g->layout when NULL; otherwise recomputes prefix sums, edge
 * count, and reallocates vertex offsets when the vertex count changed.
 * Call again after structural edge changes; layout is not auto-invalidated.
 */
void gvizGraphBuildLayout(gvizGraph *g);

/**
 * Rebuilds the layout only if the graph has been structurally mutated since
 * it was last built (or it was never built): an O(1) mutation-counter
 * compare, then gvizGraphBuildLayout when stale. The on-demand way to use
 * layout-dependent machinery (edge subsets, full subgraphs -- e.g. a
 * transient pick/highlight subgraph) against a graph that grows between
 * uses. Callers holding OTHER full subgraphs over @p g must know a rebuild
 * after mutations shifts the shared bit addressing under those subgraphs
 * (the same caveat gvizGraphBuildLayout always had); replace or rebuild
 * them before reading them again.
 *
 * @return 0 when the layout is current (rebuilt or already fresh), -1 on
 * allocation failure or NULL @p g.
 */
int gvizGraphEnsureLayout(gvizGraph *g);

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
 * @param in   (NULL?) Array of gvizEdge with vertices that have edges to v.
 * @param out  (NULL?) Array of gvizEdge with vertices v has edges to.
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
 * also be added to v's adjacency list (with the same weight). May reallocate
 * to increase the size of affected vertex/vertices. Invalidates @p g->layout
 * until gvizGraphBuildLayout is called again.
 *
 * @param g      A pointer to the Graph the vertex will be added to.
 * @param from   The index of u in g->vertices.
 * @param to     The index of v in g->vertices.
 * @param weight The weight of the edge (u, v).
 *
 * @return An error code showing whether or not the operation was successful
 * @retval 0 If the edge was created successfully.
 * @retval -1 If (from OR to is out of bounds) OR reallocation fail.
 */
int gvizGraphAddEdge(gvizGraph *g, size_t from, size_t to, double weight);

/**
 * Attempts to remove and edge (u, v) from a Graph. If @p g is undirected, u
 * will also be removed from v's adjacency list. Invalidates @p g->layout until
 * gvizGraphBuildLayout is called again.
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
 * Returns the number of neighbors (out-degree) of a Vertex in a given Graph.
 *
 * @param g   A pointer to the Graph the Vertex is in.
 * @param idx The index of the Vertex in @p g->vertices.
 *
 * @return The number of entries in the Vertex's adjacency list. 0 if @p idx
 *         is out of bounds.
 */
size_t gvizGraphDegree(const gvizGraph *g, size_t idx);

/**
 * Gets the vertex index of the @p i-th neighbor of a Vertex in a Graph.
 *
 * @param g   A pointer to the Graph the Vertex is in.
 * @param idx The index of the Vertex in @p g->vertices.
 * @param i   The position of the neighbor in @p idx's adjacency list.
 *
 * This function does NOT check that @p idx or @p i are in bounds.
 *
 * @return The vertex index of the neighbor at position @p i.
 */
size_t gvizGraphNeighbor(const gvizGraph *g, size_t idx, size_t i);

/**
 * Gets the weight of the edge to the @p i-th neighbor of a Vertex in a Graph.
 *
 * @param g   A pointer to the Graph the Vertex is in.
 * @param idx The index of the Vertex in @p g->vertices.
 * @param i   The position of the neighbor in @p idx's adjacency list.
 *
 * This function does NOT check that @p idx or @p i are in bounds.
 *
 * @return The weight of the edge at position @p i.
 */
double gvizGraphNeighborWeight(const gvizGraph *g, size_t idx, size_t i);

/**
 * Finds the position of @p to in @p from's adjacency list.
 *
 * @param g    A pointer to the Graph the vertices are in.
 * @param from The index of u in @p g->vertices.
 * @param to   The index of v in @p g->vertices.
 *
 * @return The position of v in u's adjacency list.
 * @retval pos The position of v, in [0, gvizGraphDegree(g, from)).
 * @retval -1  If from/to are out of bounds, or no edge (u, v) exists.
 */
int gvizGraphNeighborPosition(const gvizGraph *g, size_t from, size_t to);

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
 * Gets the weight of edge (u, v) in a Graph.
 *
 * @param g         A pointer to the Graph the vertices are in.
 * @param from      The index of u in @p g->vertices.
 * @param to        The index of v in @p g->vertices.
 * @param outWeight Where the edge's weight will be written on success.
 *
 * @return 0 on success, -1 if from/to are out of bounds or no edge exists.
 */
int gvizGraphGetEdgeWeight(const gvizGraph *g, size_t from, size_t to,
                           double *outWeight);

/**
 * Sets the weight of edge (u, v) in a Graph. If @p g is undirected, the
 * mirrored edge (v, u) is updated to the same weight.
 *
 * @param g      A pointer to the Graph the vertices are in.
 * @param from   The index of u in @p g->vertices.
 * @param to     The index of v in @p g->vertices.
 * @param weight The new weight for the edge.
 *
 * @return 0 on success, -1 if from/to are out of bounds or no edge exists.
 */
int gvizGraphSetEdgeWeight(gvizGraph *g, size_t from, size_t to,
                           double weight);

/**
 * Inserts a single directed adjacency entry (@p to, @p weight) into @p from's
 * adjacency list at position @p pos, shifting later entries back. Does NOT
 * mirror to @p to's own list -- intended for rotation-system bookkeeping
 * (e.g. planar embedding) where each endpoint's insertion position is
 * computed and applied independently. Invalidates @p g->layout.
 *
 * @param g      A pointer to the Graph the vertex is in.
 * @param from   The index of the vertex whose adjacency list is modified.
 * @param to     The neighboring vertex index to insert.
 * @param weight The weight of the new adjacency entry.
 * @param pos    The position to insert at, in [0, gvizGraphDegree(g, from)].
 *
 * @return 0 on success, -1 if @p from is out of bounds, @p pos is out of
 *         range, or reallocation fails.
 */
int gvizGraphInsertNeighborAt(gvizGraph *g, size_t from, size_t to,
                              double weight, size_t pos);

/**
 * Rewrites @p idx's adjacency list to match the vertex-id order given by
 * @p order, preserving each entry's existing weight. @p order must be a
 * permutation of the vertex ids currently in @p idx's adjacency list.
 *
 * @param g     A pointer to the Graph the vertex is in.
 * @param idx   The index of the vertex whose adjacency list is reordered.
 * @param order An array of @p n vertex ids, a permutation of @p idx's
 *              current neighbors.
 * @param n     The length of @p order; must equal gvizGraphDegree(g, idx).
 *
 * @return 0 on success, -1 if @p idx is out of bounds or @p order is not a
 *         valid permutation of the current neighbor set.
 */
int gvizGraphReorderNeighbors(gvizGraph *g, size_t idx, const size_t *order,
                              size_t n);

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

#endif
