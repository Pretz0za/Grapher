#include "renderer/layers/gvizGraphVBO.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>

#define GL_SILENCE_DEPRECATION
#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

void gvizGraphVBOInit(gvizGraphVBO *vbo) {
    vbo->vaoId = 0;
    vbo->vboPositions = 0;
    vbo->vertexCount = 0;
}

void gvizGraphVBORelease(gvizGraphVBO *vbo) {
    if (vbo->vboPositions) { rlUnloadVertexBuffer(vbo->vboPositions); vbo->vboPositions = 0; }
    if (vbo->vaoId)        { rlUnloadVertexArray(vbo->vaoId);         vbo->vaoId = 0; }
    vbo->vertexCount = 0;
}

/* Build a flat float[3*2] array: for each neighbor pair (i,j), store pos_i then pos_j. */
static float *buildExpandedVerts(gvizEmbeddedGraph *eg, int dim3, size_t *outCount) {
    gvizGraph *g = eg->graph;
    size_t N = g->vertices.count;

    size_t totalEdges = 0;
    for (size_t i = 0; i < N; i++)
        totalEdges += gvizGraphGetVertexNeighbors(g, i)->count;

    float *verts = GVIZ_ALLOC(totalEdges * 2 * 3 * sizeof(float));
    if (!verts) { *outCount = 0; return NULL; }

    size_t vi = 0;
    for (size_t i = 0; i < N; i++) {
        double *p  = gvizEmbeddedGraphGetVPosition(eg, i);
        gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, i);
        for (size_t j = 0; j < nbrs->count; j++) {
            double *op = gvizEmbeddedGraphGetVPosition(eg, *(size_t *)gvizArrayAtIndex(nbrs, j));
            verts[vi++] = (float)p[0];
            verts[vi++] = (float)p[1];
            verts[vi++] = dim3 ? (float)p[2] : 0.0f;
            verts[vi++] = (float)op[0];
            verts[vi++] = (float)op[1];
            verts[vi++] = dim3 ? (float)op[2] : 0.0f;
        }
    }

    *outCount = totalEdges * 2;
    return verts;
}

void gvizGraphVBORebuild(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg) {
    size_t count = 0;
    int dim3 = (eg->embedding.dim >= 3);
    float *verts = buildExpandedVerts(eg, dim3, &count);
    if (!verts) return;

    gvizGraphVBORelease(vbo);

    vbo->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(vbo->vaoId);
    vbo->vboPositions = rlLoadVertexBuffer(verts, (int)(count * 3 * sizeof(float)), true);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();
    vbo->vertexCount = (int)count;

    GVIZ_DEALLOC(verts);
}

void gvizGraphVBOUploadPositions(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg) {
    if (!vbo->vboPositions) return;
    size_t count = 0;
    int dim3 = (eg->embedding.dim >= 3);
    float *verts = buildExpandedVerts(eg, dim3, &count);
    if (!verts) return;
    rlUpdateVertexBuffer(vbo->vboPositions, verts, (int)(count * 3 * sizeof(float)), 0);
    GVIZ_DEALLOC(verts);
}

void gvizGraphVBODraw(const gvizGraphVBO *vbo) {
    if (!vbo->vaoId || vbo->vertexCount == 0) return;

    rlDrawRenderBatchActive();

    int    *locs = rlGetShaderLocsDefault();
    Matrix  mvp  = MatrixMultiply(
                       MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixTransform()),
                       rlGetMatrixProjection());

    rlEnableShader(rlGetShaderIdDefault());
    rlSetUniformMatrix(locs[RL_SHADER_LOC_MATRIX_MVP], mvp);
    float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    rlSetUniform(locs[RL_SHADER_LOC_COLOR_DIFFUSE], black, RL_SHADER_UNIFORM_VEC4, 1);

    rlEnableVertexArray(vbo->vaoId);
    glDrawArrays(GL_LINES, 0, vbo->vertexCount);
    rlDisableVertexArray();

    rlDisableShader();
}
