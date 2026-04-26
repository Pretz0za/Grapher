#include "app/gvizLayerPolyTutte.h"
#include "core/alloc.h"
#include "core/event.h"
#include "core/gvizCamera.h"
#include "dsa/gvizArray.h"
#include "raylib.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizPlanarEmbedding.h"
#include "renderer/embeddings/gvizPlanarRotation.h"
#include "renderer/layers/gvizGraphVBO.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ---- Internal highlight helpers (shape mirrors gvizLayerGraph) ----------- */

static size_t pt_computeEdgeStartIdx(gvizGraph *g, size_t *out) {
    size_t N = g->vertices.count;
    size_t total = 0;
    for (size_t i = 0; i < N; i++) {
        out[i] = total;
        total += gvizGraphGetVertexNeighbors(g, i)->count;
    }
    out[N] = total;
    return total;
}

static void pt_rebuildIndex(gvizLayerPolyTutte *self) {
    gvizGraph *g = &self->graph;
    size_t N = g->vertices.count;

    size_t *newStart = (size_t *)GVIZ_REALLOC(self->edgeStartIdx, (N + 1) * sizeof(size_t));
    if (!newStart) return;
    self->edgeStartIdx = newStart;
    size_t total = pt_computeEdgeStartIdx(g, self->edgeStartIdx);

    size_t eunits = GVIZ_ARRAY_UNITS(total ? total : 1);
    GVIZ_BIT_UNIT *neb = (GVIZ_BIT_UNIT *)GVIZ_REALLOC(self->edgeHighlight,
                                                       eunits * sizeof(GVIZ_BIT_UNIT));
    if (neb) {
        self->edgeHighlight = neb;
        memset(self->edgeHighlight, 0, eunits * sizeof(GVIZ_BIT_UNIT));
    }
    self->edgeHighlightBits = total;

    size_t vunits = GVIZ_ARRAY_UNITS(N ? N : 1);
    GVIZ_BIT_UNIT *nvb = (GVIZ_BIT_UNIT *)GVIZ_REALLOC(self->vertexHighlight,
                                                       vunits * sizeof(GVIZ_BIT_UNIT));
    if (nvb) {
        self->vertexHighlight = nvb;
        memset(self->vertexHighlight, 0, vunits * sizeof(GVIZ_BIT_UNIT));
    }
    self->vertexHighlightBits = N;
    self->highlightDirty = 1;
}

static void pt_clearHighlights(gvizLayerPolyTutte *self) {
    if (self->vertexHighlight) {
        size_t u = GVIZ_ARRAY_UNITS(self->vertexHighlightBits ? self->vertexHighlightBits : 1);
        memset(self->vertexHighlight, 0, u * sizeof(GVIZ_BIT_UNIT));
    }
    if (self->edgeHighlight) {
        size_t u = GVIZ_ARRAY_UNITS(self->edgeHighlightBits ? self->edgeHighlightBits : 1);
        memset(self->edgeHighlight, 0, u * sizeof(GVIZ_BIT_UNIT));
    }
    self->highlightDirty = 1;
}

static void pt_setVertexHL(gvizLayerPolyTutte *self, size_t v) {
    if (!self->vertexHighlight || v >= self->vertexHighlightBits) return;
    gvizSetBit(self->vertexHighlight, v);
    self->highlightDirty = 1;
}

static void pt_setEdgeHL(gvizLayerPolyTutte *self, size_t u, size_t v) {
    gvizGraph *g = &self->graph;
    if (u >= g->vertices.count) return;
    gvizArray *nb = gvizGraphGetVertexNeighbors(g, u);
    for (size_t j = 0; j < nb->count; j++) {
        if (*(size_t *)gvizArrayAtIndex(nb, j) == v) {
            size_t bit = self->edgeStartIdx[u] + j;
            if (bit < self->edgeHighlightBits)
                gvizSetBit(self->edgeHighlight, bit);
            break;
        }
    }
    if (!g->directed) {
        gvizArray *nb2 = gvizGraphGetVertexNeighbors(g, v);
        for (size_t j = 0; j < nb2->count; j++) {
            if (*(size_t *)gvizArrayAtIndex(nb2, j) == u) {
                size_t bit = self->edgeStartIdx[v] + j;
                if (bit < self->edgeHighlightBits)
                    gvizSetBit(self->edgeHighlight, bit);
                break;
            }
        }
    }
    self->highlightDirty = 1;
}

static void pt_highlightFace(gvizLayerPolyTutte *self, size_t faceIdx) {
    pt_clearHighlights(self);
    if (faceIdx >= self->faces.count) return;
    gvizArray *face = (gvizArray *)gvizArrayAtIndex(&self->faces, faceIdx);
    size_t n = face->count;
    size_t *verts = (size_t *)face->arr;
    for (size_t i = 0; i < n; i++) pt_setVertexHL(self, verts[i]);
    for (size_t i = 0; i < n; i++) {
        size_t a = verts[i];
        size_t b = verts[(i + 1) % n];
        pt_setEdgeHL(self, a, b);
    }
}

static void pt_writeColors(gvizLayerPolyTutte *self, gvizEmbeddedGraph *eg) {
    if (self->vbo.colorsCount == 0 || !self->vbo.colors) return;
    gvizGraph *g = eg->graph;
    size_t N = g->vertices.count;
    size_t fi = 0;
    for (size_t u = 0; u < N; u++) {
        gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, u);
        int uHi = (self->vertexHighlight && u < self->vertexHighlightBits)
                      ? (gvizTestBit(self->vertexHighlight, u) ? 1 : 0)
                      : 0;
        for (size_t j = 0; j < nbrs->count; j++) {
            size_t v = *(size_t *)gvizArrayAtIndex(nbrs, j);
            size_t bit = self->edgeStartIdx[u] + j;
            int eHi = (self->edgeHighlight && bit < self->edgeHighlightBits)
                          ? (gvizTestBit(self->edgeHighlight, bit) ? 1 : 0)
                          : 0;
            int vHi = (self->vertexHighlight && v < self->vertexHighlightBits)
                          ? (gvizTestBit(self->vertexHighlight, v) ? 1 : 0)
                          : 0;
            float r1 = (uHi || eHi) ? 1.0f : 0.0f;
            float r2 = (vHi || eHi) ? 1.0f : 0.0f;
            if (fi + 6 > self->vbo.colorsCount) return;
            self->vbo.colors[fi++] = r1; self->vbo.colors[fi++] = 0.0f; self->vbo.colors[fi++] = 0.0f;
            self->vbo.colors[fi++] = r2; self->vbo.colors[fi++] = 0.0f; self->vbo.colors[fi++] = 0.0f;
        }
    }
    gvizGraphVBOUploadEndpointColors(&self->vbo, self->vbo.colors);

    if (self->vbo.discHighlights && self->vbo.radiiCount == N) {
        for (size_t u = 0; u < N; u++) {
            int hi = (self->vertexHighlight && u < self->vertexHighlightBits)
                         ? (gvizTestBit(self->vertexHighlight, u) ? 1 : 0)
                         : 0;
            self->vbo.discHighlights[u] = hi ? 1.0f : 0.0f;
        }
        gvizGraphVBOSetDiscHighlights(&self->vbo, self->vbo.discHighlights, N);
    }
}

/* ---- Faces / area -------------------------------------------------------- */

static double polygonArea2D(const size_t *faceVerts, size_t n,
                            gvizEmbeddedGraph *eg) {
    if (n < 3) return 0.0;
    double area = 0.0;
    for (size_t i = 0; i < n; i++) {
        size_t a = faceVerts[i];
        size_t b = faceVerts[(i + 1) % n];
        double *pa = gvizEmbeddedGraphGetVPosition(eg, a);
        double *pb = gvizEmbeddedGraphGetVPosition(eg, b);
        area += pa[0] * pb[1] - pb[0] * pa[1];
    }
    return fabs(area) * 0.5;
}

static void releaseFaces(gvizLayerPolyTutte *self) {
    for (size_t i = 0; i < self->faces.count; i++) {
        gvizArray *f = (gvizArray *)gvizArrayAtIndex(&self->faces, i);
        gvizArrayRelease(f);
    }
    gvizArrayRelease(&self->faces);
    gvizArrayInit(&self->faces, sizeof(gvizArray));
}

/* ---- Lifecycle ----------------------------------------------------------- */

int gvizLayerPolyTutteInit(gvizLayerPolyTutte *layer, gvizGraph *mesh,
                           const size_t *outerFace, size_t outerFaceLen,
                           size_t z) {
    if (outerFaceLen < 3) outerFaceLen = 3;
    gvizLayerInit((gvizLayer *)layer, (gvizViewport){0, 0, 0, 0},
                  &GVIZ_LAYER_POLY_TUTTE_VTABLE, z);
    layer->layer.flags = GVIZ_LAYER_VISIBLE;
    layer->gpuDirty = 2;
    layer->highlightDirty = 1;
    layer->hasTutte = 0;
    layer->vertexHighlight = NULL;
    layer->vertexHighlightBits = 0;
    layer->edgeHighlight = NULL;
    layer->edgeHighlightBits = 0;
    layer->edgeStartIdx = NULL;
    layer->phase = GVIZ_POLY_TUTTE_INITIAL;
    layer->scanFaceIdx = 0;
    layer->bestFaceIdx = 0;
    layer->bestFaceArea = -DBL_MAX;
    layer->boundaryRadius = 300.0;
    layer->scanTimer = 0.0f;
    gvizArrayInit(&layer->faces, sizeof(gvizArray));

    if (gvizGraphClone(&layer->graph, mesh) != 0) return -1;

    if (gvizTutteSolveEmbeddingInit(&layer->tutte, &layer->graph, 2, 0) != 0) {
        gvizGraphRelease(&layer->graph);
        return -1;
    }

    gvizTutteSolveFixConvexPolygon(&layer->tutte, outerFace, outerFaceLen,
                                   layer->boundaryRadius);
    gvizTutteSolveEmbeddingStep(&layer->tutte, 0);
    layer->hasTutte = 1;

    gvizGraphVBOInit(&layer->vbo);
    gvizGraphVBOSetMode(&layer->vbo, GVIZ_GRAPH_VBO_EDGES | GVIZ_GRAPH_VBO_DISCS);

    pt_rebuildIndex(layer);
    return 0;
}

void gvizLayerPolyTutteDraw(void *layerV, const gvizCamera *camera) {
    (void)camera;
    gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;
    if (self->graph.vertices.count == 0) { self->gpuDirty = 0; return; }

    gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&self->tutte;

    int rebuilt = 0;
    if (self->gpuDirty >= 2 || !self->vbo.vaoId) {
        gvizGraphVBORebuild(&self->vbo, eg);
        rebuilt = 1;
    } else if (self->gpuDirty == 1)
        gvizGraphVBOUploadPositions(&self->vbo, eg);
    self->gpuDirty = 0;

    if (rebuilt || self->highlightDirty) {
        pt_writeColors(self, eg);
        self->highlightDirty = 0;
    }

    gvizGraphVBODraw(&self->vbo);
}

void gvizLayerPolyTutteUpdate(void *layerV, float dt) {
    gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;

    (void)dt;

    if (self->phase == GVIZ_POLY_TUTTE_SCANNING) {
        self->scanTimer += dt;
        if (self->scanTimer < 0.2f) return;
        self->scanTimer = 0.0f;

        fprintf(stderr, "[PolyTutte] scanning face %zu / %zu\n",
                self->scanFaceIdx, self->faces.count);

        if (self->scanFaceIdx < self->faces.count) {
            gvizArray *face = (gvizArray *)gvizArrayAtIndex(&self->faces,
                                                            self->scanFaceIdx);
            size_t n = face->count;
            size_t *verts = (size_t *)face->arr;
            double area = polygonArea2D(verts, n,
                                        (gvizEmbeddedGraph *)&self->tutte);
            if (area > self->bestFaceArea) {
                self->bestFaceArea = area;
                self->bestFaceIdx = self->scanFaceIdx;
            }
        }

        pt_clearHighlights(self);
        self->scanFaceIdx++;
        if (self->scanFaceIdx >= self->faces.count) {
            self->phase = GVIZ_POLY_TUTTE_FINAL;
            return;
        }
        pt_highlightFace(self, self->scanFaceIdx);
        return;
    }

    if (self->phase == GVIZ_POLY_TUTTE_FINAL) {
        pt_clearHighlights(self);
        if (self->faces.count > 0 && self->bestFaceIdx < self->faces.count) {
            gvizArray *best = (gvizArray *)gvizArrayAtIndex(&self->faces,
                                                            self->bestFaceIdx);
            size_t bn = best->count;
            if (bn >= 3) {
                size_t *bv = (size_t *)best->arr;
                if (self->hasTutte) {
                    gvizTutteSolveEmbeddingRelease(&self->tutte);
                    self->hasTutte = 0;
                }
                if (gvizTutteSolveEmbeddingInit(&self->tutte, &self->graph, 2, 0) == 0) {
                    gvizTutteSolveFixConvexPolygon(&self->tutte, bv, bn,
                                                   self->boundaryRadius);
                    gvizTutteSolveEmbeddingStep(&self->tutte, 0);
                    self->hasTutte = 1;
                    self->gpuDirty = 2;
                }
            }
        }
        self->phase = GVIZ_POLY_TUTTE_INITIAL;
        return;
    }
}

void gvizLayerPolyTutteRelease(void *layerV) {
    gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;
    gvizGraphVBORelease(&self->vbo);
    if (self->hasTutte) gvizTutteSolveEmbeddingRelease(&self->tutte);
    releaseFaces(self);
    gvizArrayRelease(&self->faces);
    gvizGraphRelease(&self->graph);
    if (self->vertexHighlight) GVIZ_DEALLOC(self->vertexHighlight);
    if (self->edgeHighlight) GVIZ_DEALLOC(self->edgeHighlight);
    if (self->edgeStartIdx) GVIZ_DEALLOC(self->edgeStartIdx);
    self->vertexHighlight = NULL;
    self->edgeHighlight = NULL;
    self->edgeStartIdx = NULL;
}

int gvizLayerPolyTutteHandleEvent(void *layerV, const gvizEvent *event) {
    gvizLayerPolyTutte *self = (gvizLayerPolyTutte *)layerV;
    if (event->type != GVIZ_EVENT_KEY_DOWN) return 0;
    if (event->key.key != KEY_SPACE) return 0;
    if (self->phase != GVIZ_POLY_TUTTE_INITIAL) return 0;

    /* Compute rotation and enumerate faces. */
    gvizComputeRotationSystem((gvizEmbeddedGraph *)&self->tutte);
    pt_rebuildIndex(self);
    self->gpuDirty = 2;

    releaseFaces(self);

    gvizPlanarEmbeddingState ps = {0};
    ps.embedding = self->tutte.graph;
    ps.kuratowskiSubdivision = NULL;

    gvizFaceIteratorContext ctx;
    if (gvizFaceIteratorInit(&ps, &ctx) != 0) return 1;
    if (gvizPlanarEmbeddingFaces(&ps, &ctx) != 0) {
        gvizFaceIteratorRelease(&ctx);
        return 1;
    }
    /* Move ctx.faces into self->faces (deep copy of inner arrays). */
    for (size_t i = 0; i < ctx.faces.count; i++) {
        gvizArray *src = (gvizArray *)gvizArrayAtIndex(&ctx.faces, i);
        gvizArray dst;
        gvizArrayClone(&dst, src);
        gvizArrayPush(&self->faces, &dst);
    }
    gvizFaceIteratorRelease(&ctx);

    fprintf(stderr, "[PolyTutte] faces enumerated: %zu\n", self->faces.count);

    self->phase = GVIZ_POLY_TUTTE_SCANNING;
    self->scanFaceIdx = 0;
    self->bestFaceIdx = 0;
    self->bestFaceArea = -DBL_MAX;
    self->scanTimer = 0.0f;
    if (self->faces.count > 0)
        pt_highlightFace(self, 0);
    return 1;
}
