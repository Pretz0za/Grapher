#ifndef __GRAPHVIS_H__
#define __GRAPHVIS_H__

#include "dsa/gvizGraph.h"
#include <raylib.h>

/**
 * @brief The 2D position and data of a Vertex that we will visualise.
 */
typedef struct vComponent2 {
  Vector2 pos; /** The position it occupies in 2D space. */
  float r;
} vComponent2;

typedef struct ViewStack {
  gvizGraph *end;
  size_t count, capacity;
} ViewStack;
typedef struct Embedding {
  vComponent2 *components;
  int width, height;
} Embedding;

typedef struct VisState {
  gvizGraph *g;
  ViewStack *stack;
  Embedding gamma;
} VisState;

/**
 * Draws a vComponent on the window.
 *
 * @param vComponent A vComponent to draw.
 */
void drawVertex(vComponent2 vComponent, unsigned char opacity);

/**
 * Draws an edge between two vComponents.
 *
 * @param from The vComponent the edge is originating from.
 * @param to   The vComponent the edge is going to.
 */
void drawEdge(vComponent2 from, vComponent2 to, int directed,
              unsigned char opacity);

/*
 * Opens a Raylib window and renders the state.
 */
void openStateInWindow(VisState state);

// TREES -----------------------------------------------------------------------

/*
 * @brief Vertex data decoration for Reingold-Tilford drawing algorithm.
 */
typedef struct RTVertexData {
  size_t lMost; /**< The left-most Vertex in the deepest layer of the subtree
                     rooted at this Vertex. */
  size_t rMost; /**< The right-most Vertex in the deepest layer of the subtree
                     rooted at this Vertex. */
  size_t depth; /**< The depth of this Vertex in the whole tree. */
  size_t offsetCount; /**< The number of elements in offset array. Normally this
                           would always be children count, however, if animating
                           it will be dynamic. */
  int ancestor; /**< The root of the subtree (if known) this vertex belongs to.
                 */
} RTVertexData;

/*
 * @brief Data required to perform Reingold-Tilford drawing algorithm.
 */
typedef struct RTTree {
  gvizGraph *tree;          /**< The tree to draw. */
  RTVertexData *vertexData; /**< The data of each Vertex. This is populated by
                                 the algorithm. */
  int *parents;
  int *thread; /**< Boolean array indicating which vertices were threaded. */
  float **offsets; /**< Array of float offset arrays. offsets[i] is the offset
                        array for the children of i. */
  size_t height;   /**< The calculated height of the whole tree. */
  size_t defaultAncestor;
} RTTree;

/*
 * Runs the divide-and-conquer Reingold-Tilford algorithm to calculate a tree
 * Graph's embedding. Fills the vertexData and thread arrays of an RTTree as
 * well as its height.
 *
 * The generated embedding has the following properties:
 *
 * 1- Vertices at the same level of the tree should lie along a straight line,
 * and the straight lines defining the levels should be parallel.
 *
 * 2- Children of a Vertex should appear in the same order as they do in the
 * adjacency list, from left to right.
 *
 * 3- A Vertex should be centered over all its children.
 *
 * 4- A tree and its mirror, should produce drawings that are reflections of one
 *    another; moreover, a sub-tree should be drawn the same way regardless of
 *    where it occurs in the tree.
 *
 * @param tree  A pointer to a RTTree struct with a valid tree Graph,
 *              uninitialize vertexData array, zeroed out thread array.
 * @param root  The root of the subtree of @p tree->tree to calculate the
 *              vertexData of.
 * @param level The depth of @p root in @p tree->tree.
 *
 * @note
 * References: Tidier Drawings of Trees EDWARD M. REINGOLD AND JOHN S. TILFORD
 *                 (https://reingold.co/tidier-drawings.pdf)
 *        (https://llimllib.github.io/pymag-trees/) for ancestor recording
 */
void runReingoldTilfordAlgorithm(RTTree *tree, size_t root, size_t level);

/*
 * Calculated the values in an array of vComponent2 from a filled out RTTree by
 * adding offsets. Additionally returns the minimum and maximum X values that
 * appear in the subtree rooted at @p root.
 *
 * @param tree       A pointer to a RTTree with pre-calculated correct values.
 * @param components An array of vComponent2 that will hold the output.
 * @param root       The root of the subtree to run this function on.
 * @param rootPos    The position in the plane the @p root has. Calculated in
 *                   each previous iteration level.
 *
 * @return A Vector2 holding the mimimum and maximum X values in the subtree
 *         rooted at @p root.
 */
Vector2 makeTreeComponents(RTTree *tree, vComponent2 *components, size_t root,
                           Vector2 rootPos);

/*
 * Allocates and returns data for a tree Graph embedding in 2D.
 *
 * A Graph G is a tree iff:
 * 1- G has no cycles.
 * 2- For all vertices v in G, there is only one u such that G has edge (u, v).
 *    Except for the root of the tree, which has no incoming edges.
 *
 * @param tree A pointer to the Graph of the tree to draw.
 *
 * @return An Embedding for the Graph. The caller is responsible for freeing the
 *         embedding's vComponents array.
 *
 * @note This function handles the overhead of performing the Reingold-Tilford
 *       algorithm and interpreting the output.
 */
[[nodiscard]] Embedding generateTreeEmbedding(gvizGraph *tree);

#endif
