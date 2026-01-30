#ifndef __GRAPHVIS_H__
#define __GRAPHVIS_H__

#include "./graph.h"
#include <raylib.h>

/**
 * @brief The 2D position and data of a Vertex that we will visualise.
 */
typedef struct vComponent2 {
  Vector2 pos; /** The position it occupies in 2D space. */
  float r;
} vComponent2;

typedef struct RTVertexData {
  float *offsets;
  size_t lMost;
  size_t rMost;
  size_t depth;
} RTVertexData;

typedef struct RTTree {
  Graph *tree;
  RTVertexData *vertexData;
  int *thread;
  size_t height;
} RTTree;
typedef struct ViewStack {
  Graph *end;
  size_t count, capacity;
} ViewStack;
typedef struct Embedding {
  vComponent2 *components;
  int width, height;
} Embedding;

typedef struct VisState {
  Graph *g;
  ViewStack *stack;
  Embedding gamma;
} VisState;

/**
 * Draws a vComponent on the window.
 *
 * @param vComponent A vComponent to draw.
 */
void drawVertex(vComponent2 vComponent);

/**
 * Draws an edge between two vComponents.
 *
 * @param from The vComponent the edge is originating from.
 * @param to   The vComponent the edge is going to.
 */
void drawEdge(vComponent2 from, vComponent2 to, int directed);

void openStateInWindow(VisState state);

[[nodiscard]] Embedding generateTreeEmbedding(Graph *tree);

void runReingoldTilfordAlgorithm(RTTree *tree, size_t root, size_t level);
Vector2 makeTreeComponents(RTTree *tree, vComponent2 *components, size_t root,
                           Vector2 rootPos);

#endif
