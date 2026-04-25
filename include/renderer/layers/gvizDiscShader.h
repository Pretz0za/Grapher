#ifndef _GVIZ_DISC_SHADER_H_
#define _GVIZ_DISC_SHADER_H_

/*
 * Process-wide singleton GLSL program for instanced disc-rendered vertices.
 * Each instance is a screen-aligned (billboard) quad whose fragments are
 * clipped to a unit disc in the fragment shader.
 *
 * Lifecycle:
 *  - gvizDiscShaderGet() compiles and links lazily on first call. The caller
 *    MUST have a live OpenGL context (i.e. raylib InitWindow already done).
 *    Returns a pointer to a static struct; never free it.
 *  - gvizDiscShaderRelease() drops the GPU program. Safe to skip on app exit
 *    (the OS reclaims GL state); call it explicitly only when you want to
 *    recompile or shut down a renderer mid-process.
 *
 * Vertex attribute layout (matches gvizVertexDiscVBO):
 *   location 0: vec3  aCenter   (per-instance, divisor 1)
 *   location 1: float aRadius   (per-instance, divisor 1)
 *   location 2: vec2  aCorner   (per-vertex of quad, values in {-1,+1})
 */
typedef struct gvizDiscShader {
    unsigned int programId;
    int locMVP;
    int locViewport; /* vec2: framebuffer width, height in pixels */
    int locColor;
    int locFill;
} gvizDiscShader;

const gvizDiscShader *gvizDiscShaderGet(void);
void gvizDiscShaderRelease(void);

#endif
