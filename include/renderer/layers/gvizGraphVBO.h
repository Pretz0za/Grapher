#ifndef _GVIZ_GRAPH_VBO_H_
#define _GVIZ_GRAPH_VBO_H_

#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/layers/gvizVertexDiscVBO.h"
#include <stddef.h>

/*
 * GPU-side representation of a gvizEmbeddedGraph supporting two independent
 * render modes that can be combined via bit-or:
 *  - GVIZ_GRAPH_VBO_EDGES: expanded GL_LINES VBO (one rebuild per topology
 *    change; cheap re-upload on position change).
 *  - GVIZ_GRAPH_VBO_DISCS: instanced screen-aligned discs at vertex
 *    positions, one quad per vertex, drawn via gvizVertexDiscVBO.
 *
 * Per-vertex disc radii live CPU-side in @c radii (length @c radiiCount)
 * and are mirrored to the GPU on Rebuild and on the SetRadius* calls.
 */
enum gvizGraphVBOMode {
  GVIZ_GRAPH_VBO_EDGES = 1 << 0,
  GVIZ_GRAPH_VBO_DISCS = 1 << 1,
};

#define GVIZ_GRAPH_VBO_DEFAULT_RADIUS 8.0f

typedef struct gvizGraphVBO {
  unsigned int vaoId;
  unsigned int vboPositions; /* float[3] per endpoint, 2 endpoints per edge */
  unsigned int vboColors;    /* float[3] per endpoint, parallel to positions */
  float *colors;             /* CPU mirror, length 2 * totalDirectedEdges * 3 */
  size_t colorsCount;        /* number of floats in @c colors (= vertexCount*3) */
  int vertexCount;           /* 2 * number of directed neighbor pairs */

  gvizVertexDiscVBO discs;
  unsigned int mode; /* bitmask of gvizGraphVBOMode */
  float *radii;      /* CPU-side, length radiiCount */
  size_t radiiCount;
  float *discHighlights;     /* CPU mirror, length radiiCount; 1.0 = red */
  float discFill;            /* 0.0=ring (default), 1.0=filled disc */
  gvizEmbeddedGraph *lastEG; /* borrowed; set by Rebuild for lazy disc build */

  /* Render-target dimension (2 or 3). When the bound embedding's dim
   * exceeds drawDim, positions are PCA-projected into @c projScratch
   * before GPU upload. */
  int drawDim;
  double *projScratch;       /* row-major double[N * drawDim] when active */
  size_t projScratchCap;     /* doubles allocated in projScratch */
} gvizGraphVBO;

void gvizGraphVBOInit(gvizGraphVBO *vbo);
void gvizGraphVBORelease(gvizGraphVBO *vbo);

/* Full rebuild — call when vertex count or edge topology changes. */
void gvizGraphVBORebuild(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg);

/* Positions-only update — call when only coordinates change. */
void gvizGraphVBOUploadPositions(gvizGraphVBO *vbo, gvizEmbeddedGraph *eg);

/*
 * Toggle render mode. If GVIZ_GRAPH_VBO_DISCS becomes set after a prior
 * Rebuild, disc buffers are built lazily here using the cached @p eg-less
 * state (caller must ensure a Rebuild has run with a valid graph first; if
 * radii is NULL, no disc build occurs until the next Rebuild).
 */
void gvizGraphVBOSetMode(gvizGraphVBO *vbo, unsigned int mode);

/* Set the radius of a single vertex; uploads the change immediately. */
void gvizGraphVBOSetVertexRadius(gvizGraphVBO *vbo, size_t idx, float radius);

/* Bulk setter — fills all radii uniformly and uploads. */
void gvizGraphVBOSetAllRadii(gvizGraphVBO *vbo, float radius);

/*
 * Upload a per-endpoint RGB color stream sized exactly @c colorsCount floats
 * (= 2 * totalDirectedEdges * 3). Re-uploaded via rlUpdateVertexBuffer so a
 * color change does NOT trigger a topology rebuild.
 */
void gvizGraphVBOUploadEndpointColors(gvizGraphVBO *vbo, const float *rgb2N);

/*
 * Upload a per-vertex disc highlight mask (1.0 = red, 0.0 = base color).
 * @p n must equal the vertex count (radiiCount). No-op if discs are not
 * built or @p mask is NULL.
 */
void gvizGraphVBOSetDiscHighlights(gvizGraphVBO *vbo, const float *mask,
                                   size_t n);

/*
 * Draw according to @c mode: edges first (GL_LINES), then discs on top.
 * Uses the active raylib camera matrices.
 */
void gvizGraphVBODraw(const gvizGraphVBO *vbo);

#endif
