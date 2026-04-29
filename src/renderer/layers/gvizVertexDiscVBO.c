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

static float *buildCentersXYZ(const double *positions, size_t n, size_t dim) {
    if (n == 0) return NULL;
    float *centers = GVIZ_ALLOC(n * 3 * sizeof(float));
    if (!centers) return NULL;
    int hasZ = (dim >= 3);
    for (size_t i = 0; i < n; i++) {
        const double *p = positions + i * dim;
        centers[i * 3 + 0] = (float)p[0];
        centers[i * 3 + 1] = (float)p[1];
        centers[i * 3 + 2] = hasZ ? (float)p[2] : 0.0f;
    }
    return centers;
}

void gvizVertexDiscVBORebuildXYZ(gvizVertexDiscVBO *vbo,
                                 const double *positions, size_t n, size_t dim,
                                 const float *radii) {
    size_t N = n;
    float *centers = buildCentersXYZ(positions, N, dim);
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

void gvizVertexDiscVBOUploadPositionsXYZ(gvizVertexDiscVBO *vbo,
                                         const double *positions, size_t n,
                                         size_t dim) {
    if (!vbo->vboCenters || n == 0) return;
    float *centers = buildCentersXYZ(positions, n, dim);
    if (!centers) return;
    rlUpdateVertexBuffer(vbo->vboCenters, centers, (int)(n * 3 * sizeof(float)), 0);
    GVIZ_DEALLOC(centers);
}

void gvizVertexDiscVBORebuild(gvizVertexDiscVBO *vbo, gvizEmbeddedGraph *eg,
                              const float *radii) {
    gvizVertexDiscVBORebuildXYZ(vbo, eg->embedding.vertexPositions,
                                eg->graph->vertices.count, eg->embedding.dim,
                                radii);
}

void gvizVertexDiscVBOUploadPositions(gvizVertexDiscVBO *vbo,
                                      gvizEmbeddedGraph *eg) {
    gvizVertexDiscVBOUploadPositionsXYZ(vbo, eg->embedding.vertexPositions,
                                        eg->graph->vertices.count,
                                        eg->embedding.dim);
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
