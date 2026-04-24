#ifndef _GVIZ_GRAPH_VBO_H_
#define _GVIZ_GRAPH_VBO_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"

/*
 * GPU-side line representation of a gvizEmbeddedGraph.
 * Expanded non-indexed VBO: each edge is stored as two consecutive float[3]
 * endpoint positions, drawn with GL_LINES in a single glDrawArrays call.
 * For animated embeddings, gvizGraphVBOUploadPositions cheaply re-uploads
 * positions without reallocating the GPU buffer.
 */
typedef struct gvizGraphVBO {
    unsigned int vaoId;
    unsigned int vboPositions; /* float[3] per endpoint, 2 endpoints per edge */
    int vertexCount;           /* 2 * number of directed neighbor pairs */
} gvizGraphVBO;

void gvizGraphVBOInit(gvizGraphVBO *vbo);
void gvizGraphVBORelease(gvizGraphVBO *vbo);

/* Full rebuild — call when vertex count or edge topology changes. */
void gvizGraphVBORebuild(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg);

/* Positions-only update — call when only coordinates change. */
void gvizGraphVBOUploadPositions(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg);

/* Draw edges as GL_LINES using the active raylib camera matrices. */
void gvizGraphVBODraw(const gvizGraphVBO *vbo);

#endif
