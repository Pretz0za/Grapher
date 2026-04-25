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
    gvizVertexDiscVBOInit(&vbo->discs);
    vbo->mode = GVIZ_GRAPH_VBO_EDGES;
    vbo->radii = NULL;
    vbo->radiiCount = 0;
    vbo->lastEG = NULL;
}

void gvizGraphVBORelease(gvizGraphVBO *vbo) {
    if (vbo->vboPositions) { rlUnloadVertexBuffer(vbo->vboPositions); vbo->vboPositions = 0; }
    if (vbo->vaoId)        { rlUnloadVertexArray(vbo->vaoId);         vbo->vaoId = 0; }
    vbo->vertexCount = 0;

    gvizVertexDiscVBORelease(&vbo->discs);
    if (vbo->radii) { GVIZ_DEALLOC(vbo->radii); vbo->radii = NULL; }
    vbo->radiiCount = 0;
    vbo->lastEG = NULL;
}

static float *buildExpandedVerts(gvizEmbeddedGraph *eg, int dim3, size_t *outCount) {
    gvizGraph *g = eg->graph;
    size_t N = g->vertices.count;

    size_t totalEdges = 0;
    for (size_t i = 0; i < N; i++)
        totalEdges += gvizGraphGetVertexNeighbors(g, i)->count;

    if (totalEdges == 0) { *outCount = 0; return NULL; }

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

static void rebuildEdges(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg) {
    size_t count = 0;
    int dim3 = (eg->embedding.dim >= 3);
    float *verts = buildExpandedVerts(eg, dim3, &count);

    if (vbo->vboPositions) { rlUnloadVertexBuffer(vbo->vboPositions); vbo->vboPositions = 0; }
    if (vbo->vaoId)        { rlUnloadVertexArray(vbo->vaoId);         vbo->vaoId = 0; }
    vbo->vertexCount = 0;

    if (!verts || count == 0) {
        if (verts) GVIZ_DEALLOC(verts);
        return;
    }

    vbo->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(vbo->vaoId);
    vbo->vboPositions = rlLoadVertexBuffer(verts, (int)(count * 3 * sizeof(float)), true);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    rlDisableVertexArray();
    vbo->vertexCount = (int)count;

    GVIZ_DEALLOC(verts);
}

static void ensureRadii(gvizGraphVBO *vbo, size_t N) {
    if (vbo->radiiCount != N) {
        if (vbo->radii) GVIZ_DEALLOC(vbo->radii);
        vbo->radii = (N > 0) ? (float *)GVIZ_ALLOC(N * sizeof(float)) : NULL;
        vbo->radiiCount = vbo->radii ? N : 0;
    }
    for (size_t i = 0; i < vbo->radiiCount; i++)
        vbo->radii[i] = GVIZ_GRAPH_VBO_DEFAULT_RADIUS;
}

void gvizGraphVBORebuild(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg) {
    vbo->lastEG = eg;
    rebuildEdges(vbo, eg);

    size_t N = eg->graph->vertices.count;
    ensureRadii(vbo, N);

    if (vbo->mode & GVIZ_GRAPH_VBO_DISCS)
        gvizVertexDiscVBORebuild(&vbo->discs, eg, vbo->radii);
}

void gvizGraphVBOUploadPositions(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg) {
    vbo->lastEG = eg;
    if (vbo->vboPositions) {
        size_t count = 0;
        int dim3 = (eg->embedding.dim >= 3);
        float *verts = buildExpandedVerts(eg, dim3, &count);
        if (verts) {
            rlUpdateVertexBuffer(vbo->vboPositions, verts, (int)(count * 3 * sizeof(float)), 0);
            GVIZ_DEALLOC(verts);
        }
    }
    if ((vbo->mode & GVIZ_GRAPH_VBO_DISCS) && vbo->discs.vaoId)
        gvizVertexDiscVBOUploadPositions(&vbo->discs, eg);
}

void gvizGraphVBOSetMode(gvizGraphVBO *vbo, unsigned int mode) {
    unsigned int prev = vbo->mode;
    vbo->mode = mode;

    int discsTurnedOn  = !(prev & GVIZ_GRAPH_VBO_DISCS) && (mode & GVIZ_GRAPH_VBO_DISCS);
    int discsTurnedOff =  (prev & GVIZ_GRAPH_VBO_DISCS) && !(mode & GVIZ_GRAPH_VBO_DISCS);

    if (discsTurnedOn && vbo->lastEG && vbo->radii)
        gvizVertexDiscVBORebuild(&vbo->discs, vbo->lastEG, vbo->radii);
    else if (discsTurnedOff)
        gvizVertexDiscVBORelease(&vbo->discs);
}

void gvizGraphVBOSetVertexRadius(gvizGraphVBO *vbo, size_t idx, float radius) {
    if (idx >= vbo->radiiCount) return;
    vbo->radii[idx] = radius;
    if (vbo->discs.vaoId)
        gvizVertexDiscVBOUploadRadii(&vbo->discs, vbo->radii, vbo->radiiCount);
}

void gvizGraphVBOSetAllRadii(gvizGraphVBO *vbo, float radius) {
    for (size_t i = 0; i < vbo->radiiCount; i++) vbo->radii[i] = radius;
    if (vbo->discs.vaoId)
        gvizVertexDiscVBOUploadRadii(&vbo->discs, vbo->radii, vbo->radiiCount);
}

void gvizGraphVBODraw(const gvizGraphVBO *vbo) {
    if ((vbo->mode & GVIZ_GRAPH_VBO_EDGES) && vbo->vaoId && vbo->vertexCount > 0) {
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

    if ((vbo->mode & GVIZ_GRAPH_VBO_DISCS) && vbo->discs.vaoId && vbo->discs.instanceCount > 0) {
        float discColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        gvizVertexDiscVBODraw(&vbo->discs, discColor, 0.0f);
    }
}
