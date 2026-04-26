#ifndef _GVIZ_VERTEX_DISC_VBO_H_
#define _GVIZ_VERTEX_DISC_VBO_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include <stddef.h>

/*
 * Instanced disc renderer for vertex positions of a gvizEmbeddedGraph.
 * One quad (TRIANGLE_STRIP, 4 verts) is replicated per vertex via
 * glDrawArraysInstanced. Per-instance attributes carry the world-space
 * center and the radius.
 *
 *   vboCorners: static, 4 * vec2 in {-1,+1}; attribute 2, divisor 0
 *   vboCenters: dynamic, vec3 per vertex;     attribute 0, divisor 1
 *   vboRadii:   dynamic, float per vertex;    attribute 1, divisor 1
 */
typedef struct gvizVertexDiscVBO {
    unsigned int vaoId;
    unsigned int vboCorners;
    unsigned int vboCenters;
    unsigned int vboRadii;
    unsigned int vboHighlights;
    int instanceCount;
} gvizVertexDiscVBO;

void gvizVertexDiscVBOInit(gvizVertexDiscVBO *vbo);
void gvizVertexDiscVBORelease(gvizVertexDiscVBO *vbo);

/* Full rebuild: (re)allocates VAO + all three VBOs. */
void gvizVertexDiscVBORebuild(gvizVertexDiscVBO *vbo, gvizEmbeddedGraph *eg,
                              const float *radii);

/* Re-upload only the per-instance center positions from @p eg. */
void gvizVertexDiscVBOUploadPositions(gvizVertexDiscVBO *vbo,
                                      gvizEmbeddedGraph *eg);

/* Re-upload only the per-instance radii. @p n must equal instanceCount. */
void gvizVertexDiscVBOUploadRadii(gvizVertexDiscVBO *vbo, const float *radii,
                                  size_t n);

/*
 * Re-upload only the per-instance highlight mask (1.0 = red, 0.0 = base
 * uniform color). @p n must equal instanceCount.
 */
void gvizVertexDiscVBOUploadHighlights(gvizVertexDiscVBO *vbo,
                                       const float *highlights, size_t n);

/* Bind disc shader, set MVP/MV/P/color uniforms, and issue the instanced draw. */
void gvizVertexDiscVBODraw(const gvizVertexDiscVBO *vbo, const float color[4],
                           float fill);

#endif
