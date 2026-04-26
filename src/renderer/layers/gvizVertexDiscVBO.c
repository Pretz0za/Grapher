#include "renderer/layers/gvizVertexDiscVBO.h"
#include "core/alloc.h"
#include "renderer/layers/gvizDiscShader.h"
#include "raymath.h"
#include "rlgl.h"

#include <stdlib.h>
#include <string.h>

#define GL_SILENCE_DEPRECATION
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

static const float QUAD_CORNERS[8] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
};

void gvizVertexDiscVBOInit(gvizVertexDiscVBO *vbo) {
    vbo->vaoId = 0;
    vbo->vboCorners = 0;
    vbo->vboCenters = 0;
    vbo->vboRadii = 0;
    vbo->vboHighlights = 0;
    vbo->instanceCount = 0;
}

void gvizVertexDiscVBORelease(gvizVertexDiscVBO *vbo) {
    if (vbo->vboCorners)    { rlUnloadVertexBuffer(vbo->vboCorners);    vbo->vboCorners = 0; }
    if (vbo->vboCenters)    { rlUnloadVertexBuffer(vbo->vboCenters);    vbo->vboCenters = 0; }
    if (vbo->vboRadii)      { rlUnloadVertexBuffer(vbo->vboRadii);      vbo->vboRadii   = 0; }
    if (vbo->vboHighlights) { rlUnloadVertexBuffer(vbo->vboHighlights); vbo->vboHighlights = 0; }
    if (vbo->vaoId)         { rlUnloadVertexArray(vbo->vaoId);          vbo->vaoId      = 0; }
    vbo->instanceCount = 0;
}

static float *buildCenters(gvizEmbeddedGraph *eg, size_t *outN) {
    size_t N = eg->graph->vertices.count;
    int dim3 = (eg->embedding.dim >= 3);
    float *centers = GVIZ_ALLOC(N * 3 * sizeof(float));
    if (!centers) { *outN = 0; return NULL; }
    for (size_t i = 0; i < N; i++) {
        double *p = gvizEmbeddedGraphGetVPosition(eg, i);
        centers[i * 3 + 0] = (float)p[0];
        centers[i * 3 + 1] = (float)p[1];
        centers[i * 3 + 2] = dim3 ? (float)p[2] : 0.0f;
    }
    *outN = N;
    return centers;
}

void gvizVertexDiscVBORebuild(gvizVertexDiscVBO *vbo, gvizEmbeddedGraph *eg,
                              const float *radii) {
    size_t N = 0;
    float *centers = buildCenters(eg, &N);
    if (!centers || N == 0) {
        if (centers) GVIZ_DEALLOC(centers);
        return;
    }

    gvizVertexDiscVBORelease(vbo);

    vbo->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(vbo->vaoId);

    vbo->vboCenters = rlLoadVertexBuffer(centers, (int)(N * 3 * sizeof(float)), true);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    glVertexAttribDivisor(0, 1);

    vbo->vboRadii = rlLoadVertexBuffer(radii, (int)(N * sizeof(float)), true);
    rlSetVertexAttribute(1, 1, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(1);
    glVertexAttribDivisor(1, 1);

    vbo->vboCorners = rlLoadVertexBuffer(QUAD_CORNERS, (int)sizeof(QUAD_CORNERS), false);
    rlSetVertexAttribute(2, 2, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(2);
    glVertexAttribDivisor(2, 0);

    float *zeros = GVIZ_ALLOC(N * sizeof(float));
    if (zeros) {
        for (size_t i = 0; i < N; i++) zeros[i] = 0.0f;
        vbo->vboHighlights = rlLoadVertexBuffer(zeros, (int)(N * sizeof(float)), true);
        rlSetVertexAttribute(3, 1, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(3);
        glVertexAttribDivisor(3, 1);
        GVIZ_DEALLOC(zeros);
    }

    rlDisableVertexArray();

    vbo->instanceCount = (int)N;
    GVIZ_DEALLOC(centers);
}

void gvizVertexDiscVBOUploadPositions(gvizVertexDiscVBO *vbo,
                                      gvizEmbeddedGraph *eg) {
    if (!vbo->vboCenters) return;
    size_t N = 0;
    float *centers = buildCenters(eg, &N);
    if (!centers) return;
    rlUpdateVertexBuffer(vbo->vboCenters, centers, (int)(N * 3 * sizeof(float)), 0);
    GVIZ_DEALLOC(centers);
}

void gvizVertexDiscVBOUploadRadii(gvizVertexDiscVBO *vbo, const float *radii,
                                  size_t n) {
    if (!vbo->vboRadii || !radii) return;
    rlUpdateVertexBuffer(vbo->vboRadii, radii, (int)(n * sizeof(float)), 0);
}

void gvizVertexDiscVBOUploadHighlights(gvizVertexDiscVBO *vbo,
                                       const float *highlights, size_t n) {
    if (!vbo->vboHighlights || !highlights) return;
    rlUpdateVertexBuffer(vbo->vboHighlights, highlights,
                         (int)(n * sizeof(float)), 0);
}

void gvizVertexDiscVBODraw(const gvizVertexDiscVBO *vbo, const float color[4],
                           float fill) {
    if (!vbo->vaoId || vbo->instanceCount == 0) return;

    const gvizDiscShader *sh = gvizDiscShaderGet();
    if (!sh || !sh->programId) return;

    rlDrawRenderBatchActive();

    Matrix mvp = MatrixMultiply(
        MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixTransform()),
        rlGetMatrixProjection());
    float viewport[2] = {
        (float)rlGetFramebufferWidth(),
        (float)rlGetFramebufferHeight(),
    };

    rlEnableShader(sh->programId);
    if (sh->locMVP      >= 0) rlSetUniformMatrix(sh->locMVP, mvp);
    if (sh->locViewport >= 0) rlSetUniform(sh->locViewport, viewport, RL_SHADER_UNIFORM_VEC2, 1);
    if (sh->locColor    >= 0) rlSetUniform(sh->locColor, color, RL_SHADER_UNIFORM_VEC4, 1);
    if (sh->locFill     >= 0) rlSetUniform(sh->locFill, &fill, RL_SHADER_UNIFORM_FLOAT, 1);

    rlEnableVertexArray(vbo->vaoId);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, vbo->instanceCount);
    rlDisableVertexArray();

    rlDisableShader();
}
