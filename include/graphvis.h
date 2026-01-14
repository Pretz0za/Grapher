#ifndef __GRAPHVIS_H__
#define __GRAPHVIS_H__

#include "./graph.h"
#include <raylib.h>

/**
 * @brief The 2D position and data of a Vertex that we will visualise.
 */
typedef struct vComponent2 {
  Vertex *v;   /**< The Vertex data of the componenet. */
  Vector2 pos; /** The position it occupies in 2D space. */
  float r;
} vComponent2;

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

#endif
