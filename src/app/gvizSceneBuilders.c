#include "app/gvizSceneBuilders.h"
#include "app/gvizLayerTutte.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedTree.h"
#include "renderer/layers/gvizLayerGraph.h"
#include "utils/graphs.h"
#include "utils/gvizTreeIO.h"
#include <math.h>
#include <stdlib.h>

int gvizBuildBlankScene(gvizScene *out) { return gvizSceneInit2D(out); }

/* ---- Release callbacks ---------------------------------------------------- */

static void releaseGRIP(gvizEmbeddedGraph *eg) {
  gvizGRIPState *s = (gvizGRIPState *)eg;
  gvizGraph *g = eg->graph;
  gvizGRIPEmbeddingRelease(s);
  if (g) { gvizGraphRelease(g); GVIZ_DEALLOC(g); }
  GVIZ_DEALLOC(s);
}

static void releaseEmbeddedTree(gvizEmbeddedGraph *eg) {
  gvizEmbeddedTree *t = (gvizEmbeddedTree *)eg;
  gvizGraph *g = eg->graph;
  gvizEmbeddedTreeRTRelease(t);
  if (g) { gvizGraphRelease(g); GVIZ_DEALLOC(g); }
  GVIZ_DEALLOC(t);
}

/* ---- GRIP Sierpinski ------------------------------------------------------ */

int gvizBuildGRIPSierpinskiScene(gvizScene *out, int depth) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph *g = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!g) { gvizSceneRelease(out); return -1; }
  *g = createSierpinski(depth, NULL);

  gvizGRIPState *state = GVIZ_ALLOC(sizeof(gvizGRIPState));
  if (!state ||
      gvizGRIPEmbeddingInit(state, g, (size_t)pow(2.0, (double)depth), 2) != 0) {
    if (state) GVIZ_DEALLOC(state);
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGRIPEmbeddingEmbed(state);

  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    releaseGRIP((gvizEmbeddedGraph *)state);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, (gvizEmbeddedGraph *)state, releaseGRIP, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

/* ---- GRIP carpet ---------------------------------------------------------- */

int gvizBuildGRIPCarpetScene(gvizScene *out, int depth) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph *g = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!g) { gvizSceneRelease(out); return -1; }
  *g = build_sierpinski_carpet(depth);

  gvizGRIPState *state = GVIZ_ALLOC(sizeof(gvizGRIPState));
  if (!state ||
      gvizGRIPEmbeddingInit(state, g, (size_t)pow(2.0, (double)depth), 2) != 0) {
    if (state) GVIZ_DEALLOC(state);
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGRIPEmbeddingEmbed(state);

  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    releaseGRIP((gvizEmbeddedGraph *)state);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, (gvizEmbeddedGraph *)state, releaseGRIP, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

/* ---- Tree from JSON file -------------------------------------------------- */

int gvizBuildSceneFromTreeFile(gvizScene *out, const char *path) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph *g = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!g) { gvizSceneRelease(out); return -1; }

  size_t root = 0;
  if (gvizLoadTreeJSON(path, g, &root) != 0) {
    GVIZ_DEALLOC(g);
    gvizSceneRelease(out);
    return -1;
  }

  gvizEmbeddedTree *tree = GVIZ_ALLOC(sizeof(gvizEmbeddedTree));
  if (!tree || gvizEmbeddedTreeRTInit(tree, g, root) != 0) {
    if (tree) GVIZ_DEALLOC(tree);
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizEmbeddedTreeCalculateOffsets(tree, root, 0);
  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(tree, root, origin);

  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    releaseEmbeddedTree((gvizEmbeddedGraph *)tree);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, (gvizEmbeddedGraph *)tree, releaseEmbeddedTree, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

/* ---- Tutte spider-web demo ------------------------------------------------ */

#define TUTTE_SPOKES 6
#define TUTTE_RINGS 5
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
    if (tlayer)
      GVIZ_DEALLOC(tlayer);
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphRelease(&g);
  gvizSceneAddLayer(out, (gvizLayer *)tlayer);
  return 0;
}

/* ---- Random tree demo ----------------------------------------------------- */

static void buildRandomTree(gvizGraph *tree, size_t root, int depth,
                            int maxDepth) {
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

  gvizGraph *g = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!g) { gvizSceneRelease(out); return -1; }
  gvizGraphInit(g, 1);
  gvizGraphAddVertex(g, NULL, NULL, NULL);
  buildRandomTree(g, 0, 0, 5);

  gvizEmbeddedTree *tree = GVIZ_ALLOC(sizeof(gvizEmbeddedTree));
  if (!tree || gvizEmbeddedTreeRTInit(tree, g, 0) != 0) {
    if (tree) GVIZ_DEALLOC(tree);
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizEmbeddedTreeCalculateOffsets(tree, 0, 0);
  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(tree, 0, origin);

  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    releaseEmbeddedTree((gvizEmbeddedGraph *)tree);
    gvizSceneRelease(out);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, (gvizEmbeddedGraph *)tree, releaseEmbeddedTree, vp, 0);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}
