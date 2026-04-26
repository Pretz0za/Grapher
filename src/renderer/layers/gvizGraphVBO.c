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

/* Per-endpoint colored line shader (singleton). */
static const char *EDGE_VS_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec3 aPos;\n"
    "layout(location=1) in vec3 aColor;\n"
    "uniform mat4 uMVP;\n"
    "out vec3 vColor;\n"
    "void main() {\n"
    "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char *EDGE_FS_SRC =
    "#version 330 core\n"
    "in vec3 vColor;\n"
    "out vec4 fragColor;\n"
    "void main() { fragColor = vec4(vColor, 1.0); }\n";

static unsigned int g_edgeProgram = 0;
static int g_edgeLocMVP = -1;

static unsigned int compileEdgeShader(unsigned int type, const char *src) {
    unsigned int id = glCreateShader(type);
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);
    int ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(id); return 0; }
    return id;
}

static unsigned int getEdgeProgram(void) {
    if (g_edgeProgram) return g_edgeProgram;
    unsigned int vs = compileEdgeShader(GL_VERTEX_SHADER, EDGE_VS_SRC);
    unsigned int fs = compileEdgeShader(GL_FRAGMENT_SHADER, EDGE_FS_SRC);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    unsigned int prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(prog); return 0; }
    g_edgeProgram = prog;
    g_edgeLocMVP = glGetUniformLocation(prog, "uMVP");
    return prog;
}

void gvizGraphVBOInit(gvizGraphVBO *vbo) {
    vbo->vaoId = 0;
    vbo->vboPositions = 0;
    vbo->vboColors = 0;
    vbo->colors = NULL;
    vbo->colorsCount = 0;
    vbo->vertexCount = 0;
    gvizVertexDiscVBOInit(&vbo->discs);
    vbo->mode = GVIZ_GRAPH_VBO_EDGES;
    vbo->radii = NULL;
    vbo->radiiCount = 0;
    vbo->discFill = 0.0f;
    vbo->lastEG = NULL;
}

void gvizGraphVBORelease(gvizGraphVBO *vbo) {
    if (vbo->vboPositions) { rlUnloadVertexBuffer(vbo->vboPositions); vbo->vboPositions = 0; }
    if (vbo->vboColors)    { rlUnloadVertexBuffer(vbo->vboColors);    vbo->vboColors = 0; }
    if (vbo->vaoId)        { rlUnloadVertexArray(vbo->vaoId);         vbo->vaoId = 0; }
    vbo->vertexCount = 0;

    if (vbo->colors) { GVIZ_DEALLOC(vbo->colors); vbo->colors = NULL; }
    vbo->colorsCount = 0;

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
    if (vbo->vboColors)    { rlUnloadVertexBuffer(vbo->vboColors);    vbo->vboColors = 0; }
    if (vbo->vaoId)        { rlUnloadVertexArray(vbo->vaoId);         vbo->vaoId = 0; }
    vbo->vertexCount = 0;
    if (vbo->colors) { GVIZ_DEALLOC(vbo->colors); vbo->colors = NULL; }
    vbo->colorsCount = 0;

    if (!verts || count == 0) {
        if (verts) GVIZ_DEALLOC(verts);
        return;
    }

    size_t nFloats = count * 3;
    vbo->colors = (float *)GVIZ_ALLOC(nFloats * sizeof(float));
    if (vbo->colors) {
        for (size_t i = 0; i < nFloats; i++) vbo->colors[i] = 0.0f;
        vbo->colorsCount = nFloats;
    }

    vbo->vaoId = rlLoadVertexArray();
    rlEnableVertexArray(vbo->vaoId);
    vbo->vboPositions = rlLoadVertexBuffer(verts, (int)(count * 3 * sizeof(float)), true);
    rlSetVertexAttribute(0, 3, RL_FLOAT, false, 0, 0);
    rlEnableVertexAttribute(0);
    if (vbo->colors) {
        vbo->vboColors = rlLoadVertexBuffer(vbo->colors,
                                            (int)(nFloats * sizeof(float)), true);
        rlSetVertexAttribute(1, 3, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(1);
    }
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

void gvizGraphVBOUploadEndpointColors(gvizGraphVBO *vbo, const float *rgb2N) {
    if (!vbo->vboColors || !vbo->colors || vbo->colorsCount == 0) return;
    for (size_t i = 0; i < vbo->colorsCount; i++) vbo->colors[i] = rgb2N[i];
    rlUpdateVertexBuffer(vbo->vboColors, vbo->colors,
                         (int)(vbo->colorsCount * sizeof(float)), 0);
}

void gvizGraphVBOSetAllRadii(gvizGraphVBO *vbo, float radius) {
    for (size_t i = 0; i < vbo->radiiCount; i++) vbo->radii[i] = radius;
    if (vbo->discs.vaoId)
        gvizVertexDiscVBOUploadRadii(&vbo->discs, vbo->radii, vbo->radiiCount);
}

void gvizGraphVBODraw(const gvizGraphVBO *vbo) {
    if ((vbo->mode & GVIZ_GRAPH_VBO_EDGES) && vbo->vaoId && vbo->vertexCount > 0) {
        rlDrawRenderBatchActive();

        unsigned int prog = getEdgeProgram();
        Matrix mvp = MatrixMultiply(
                         MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixTransform()),
                         rlGetMatrixProjection());

        if (prog) {
            float m[16] = {
                mvp.m0, mvp.m1, mvp.m2, mvp.m3,
                mvp.m4, mvp.m5, mvp.m6, mvp.m7,
                mvp.m8, mvp.m9, mvp.m10, mvp.m11,
                mvp.m12, mvp.m13, mvp.m14, mvp.m15
            };
            glUseProgram(prog);
            glUniformMatrix4fv(g_edgeLocMVP, 1, GL_FALSE, m);

            rlEnableVertexArray(vbo->vaoId);
            glDrawArrays(GL_LINES, 0, vbo->vertexCount);
            rlDisableVertexArray();

            glUseProgram(0);
        }
    }

    if ((vbo->mode & GVIZ_GRAPH_VBO_DISCS) && vbo->discs.vaoId && vbo->discs.instanceCount > 0) {
        float discColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        gvizVertexDiscVBODraw(&vbo->discs, discColor, vbo->discFill);
    }
}
