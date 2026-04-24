#include "app/gvizSceneBuilders.h"
#include "app/gvizLayerTutte.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizEmbeddedTree.h"
#include "renderer/layers/gvizLayerGraph.h"
#include "utils/graphs.h"
#include "utils/gvizTreeIO.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

int gvizBuildBlankScene(gvizScene *out) { return gvizSceneInit2D(out); }

/*
 * Copy a gvizGraph (structure + directedness) into a fresh heap copy.
 */
static gvizGraph *cloneGraphToHeap(gvizGraph *src) {
  gvizGraph *g = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!g)
    return NULL;
  if (gvizGraphClone(g, src) != 0) {
    GVIZ_DEALLOC(g);
    return NULL;
  }
  return g;
}

int gvizBuildGRIPSierpinskiScene(gvizScene *out, int depth) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  /* Build the sierpinski graph on the stack first. */
  gvizGraph g = createSierpinski(depth, NULL);

  /* Run GRIP in 2D. gvizGRIPState's first member IS a gvizEmbeddedGraph, so
   * we can cast after embedding completes. */
  gvizGRIPState state;
  if (gvizGRIPEmbeddingInit(&state, &g, (size_t)pow(2.0, (double)depth), 2) !=
      0) {
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGRIPEmbeddingEmbed(&state);

  /* Transfer into a layer-owned gvizEmbeddedGraph. We heap-allocate a fresh
   * graph clone + a fresh embedding, copy positions, then release the GRIP
   * state (which releases its own copy of the graph references). */
  gvizGraph *hg = cloneGraphToHeap(&g);
  gvizGraphRelease(&g);
  if (!hg) {
    gvizGRIPEmbeddingRelease(&state);
    gvizSceneRelease(out);
    return -1;
  }

  gvizEmbeddedGraph *eg = GVIZ_ALLOC(sizeof(gvizEmbeddedGraph));
  if (!eg || gvizEmbeddedGraphInit(eg, hg, 3) != 0) {
    if (eg)
      GVIZ_DEALLOC(eg);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizGRIPEmbeddingRelease(&state);
    gvizSceneRelease(out);
    return -1;
  }

  /* Copy 2D positions into dim-3 slots (z=0). */
  gvizEmbeddedGraph *src = (gvizEmbeddedGraph *)&state;
  size_t N = src->graph->vertices.count;
  for (size_t i = 0; i < N; i++) {
    double *sp = gvizEmbeddedGraphGetVPosition(src, i);
    double p[3] = {sp[0], sp[1], 0.0};
    gvizEmbeddedGraphSetVPosition(eg, i, p);
  }
  gvizGRIPEmbeddingRelease(&state);

  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    gvizEmbeddedGraphRelease(eg);
    GVIZ_DEALLOC(eg);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, eg, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

int gvizBuildSceneFromTreeFile(gvizScene *out, const char *path) {
  if (gvizSceneInit2D(out) != 0)
    return -1;
  gvizGraph *hg = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!hg) {
    gvizSceneRelease(out);
    return -1;
  }
  size_t root = 0;
  if (gvizLoadTreeJSON(path, hg, &root) != 0) {
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizEmbeddedGraph *eg = GVIZ_ALLOC(sizeof(gvizEmbeddedGraph));
  if (!eg || gvizEmbeddedGraphInit(eg, hg, 3) != 0) {
    if (eg)
      GVIZ_DEALLOC(eg);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    gvizEmbeddedGraphRelease(eg);
    GVIZ_DEALLOC(eg);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, eg, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

/* ---- GRIP carpet ---------------------------------------------------------- */

int gvizBuildGRIPCarpetScene(gvizScene *out, int depth) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph g = build_sierpinski_carpet(depth);

  gvizGRIPState state;
  if (gvizGRIPEmbeddingInit(&state, &g, (size_t)pow(2.0, (double)depth), 2) != 0) {
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGRIPEmbeddingEmbed(&state);

  gvizGraph *hg = cloneGraphToHeap(&g);
  gvizGraphRelease(&g);
  if (!hg) {
    gvizGRIPEmbeddingRelease(&state);
    gvizSceneRelease(out);
    return -1;
  }

  gvizEmbeddedGraph *eg = GVIZ_ALLOC(sizeof(gvizEmbeddedGraph));
  if (!eg || gvizEmbeddedGraphInit(eg, hg, 3) != 0) {
    if (eg) GVIZ_DEALLOC(eg);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizGRIPEmbeddingRelease(&state);
    gvizSceneRelease(out);
    return -1;
  }

  gvizEmbeddedGraph *gsrc = (gvizEmbeddedGraph *)&state;
  size_t N = gsrc->graph->vertices.count;
  for (size_t i = 0; i < N; i++) {
    double *sp = gvizEmbeddedGraphGetVPosition(gsrc, i);
    double p[3] = {sp[0], sp[1], 0.0};
    gvizEmbeddedGraphSetVPosition(eg, i, p);
  }
  gvizGRIPEmbeddingRelease(&state);

  gvizLayerGraph *clayer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!clayer) {
    gvizEmbeddedGraphRelease(eg);
    GVIZ_DEALLOC(eg);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(clayer, eg, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)clayer);
  return 0;
}

/* ---- Tutte spider-web demo ------------------------------------------------ */

#define TUTTE_SPOKES 6
#define TUTTE_RINGS  3
#define TUTTE_RADIUS 300.0

static gvizGraph buildSpiderWeb(void) {
  int totalVerts = TUTTE_RINGS * TUTTE_SPOKES + 1;
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < totalVerts; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int r = 0; r < TUTTE_RINGS; r++)
    for (int s = 0; s < TUTTE_SPOKES; s++)
      gvizGraphAddEdge(&g, (size_t)(r * TUTTE_SPOKES + s),
                           (size_t)(r * TUTTE_SPOKES + (s + 1) % TUTTE_SPOKES));
  for (int r = 0; r < TUTTE_RINGS - 1; r++)
    for (int s = 0; s < TUTTE_SPOKES; s++)
      gvizGraphAddEdge(&g, (size_t)(r * TUTTE_SPOKES + s),
                           (size_t)((r + 1) * TUTTE_SPOKES + s));
  size_t hub = (size_t)(TUTTE_RINGS * TUTTE_SPOKES);
  for (int s = 0; s < TUTTE_SPOKES; s++)
    gvizGraphAddEdge(&g, (size_t)((TUTTE_RINGS - 1) * TUTTE_SPOKES + s), hub);
  return g;
}

int gvizBuildTutteDemoScene(gvizScene *out) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph g = buildSpiderWeb();

  size_t boundary[TUTTE_SPOKES];
  for (int i = 0; i < TUTTE_SPOKES; i++)
    boundary[i] = (size_t)i;

  gvizLayerTutte *tlayer = GVIZ_ALLOC(sizeof(gvizLayerTutte));
  if (!tlayer || gvizLayerTutteInit(tlayer, &g, boundary, TUTTE_SPOKES,
                                    TUTTE_RADIUS, 0) != 0) {
    if (tlayer) GVIZ_DEALLOC(tlayer);
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphRelease(&g);
  gvizSceneAddLayer(out, (gvizLayer *)tlayer);
  return 0;
}

/* ---- Random tree demo ----------------------------------------------------- */

static void buildRandomTree(gvizGraph *tree, size_t root, int depth, int maxDepth) {
  if (depth >= maxDepth)
    return;
  int children = 2 + rand() % 4;
  for (int i = 0; i < children; i++) {
    gvizGraphAddVertex(tree, NULL, NULL, NULL);
    size_t child = tree->vertices.count - 1;
    gvizGraphAddEdge(tree, root, child);
    buildRandomTree(tree, child, depth + 1, maxDepth);
  }
}

int gvizBuildTreeDemoScene(gvizScene *out) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph *hg = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!hg) {
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphInit(hg, 1);
  gvizGraphAddVertex(hg, NULL, NULL, NULL);
  buildRandomTree(hg, 0, 0, 5);

  gvizEmbeddedTree *tree = GVIZ_ALLOC(sizeof(gvizEmbeddedTree));
  if (!tree || gvizEmbeddedTreeRTInit(tree, hg, 0) != 0) {
    if (tree) GVIZ_DEALLOC(tree);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizEmbeddedTreeCalculateOffsets(tree, 0, 0);
  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(tree, 0, origin);

  gvizLayerGraph *tlayer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!tlayer) {
    gvizEmbeddedTreeRTRelease(tree);
    GVIZ_DEALLOC(tree);
    gvizGraphRelease(hg);
    GVIZ_DEALLOC(hg);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(tlayer, (gvizEmbeddedGraph *)tree, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)tlayer);
  return 0;
}
