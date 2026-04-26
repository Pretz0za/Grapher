#include "app/gvizSceneBuilders.h"
#include "app/gvizLayerGRIPLive.h"
#include "app/gvizLayerPolyTutte.h"
#include "app/gvizLayerTutte.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvizEmbeddedTree.h"
#include "renderer/layers/gvizLayerGraph.h"
#include "utils/graphs.h"
#include "utils/gvizOBJLoader.h"
#include "utils/gvizTreeIO.h"
#include <math.h>
#include <stdlib.h>

int gvizBuildBlankScene(gvizScene *out) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizLayerTutte *tlayer = GVIZ_ALLOC(sizeof(gvizLayerTutte));
  if (!tlayer || gvizLayerTutteInitEmpty(tlayer, 0) != 0) {
    if (tlayer)
      GVIZ_DEALLOC(tlayer);
    gvizSceneRelease(out);
    return -1;
  }
  gvizSceneAddLayer(out, (gvizLayer *)tlayer);
  return 0;
}

/* ---- Release callbacks ---------------------------------------------------- */

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

  gvizGraph g = createSierpinski(depth, NULL);

  gvizLayerGRIPLive *layer = GVIZ_ALLOC(sizeof(gvizLayerGRIPLive));
  if (!layer ||
      gvizLayerGRIPLiveInit(layer, &g, (size_t)pow(2.0, (double)depth), 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphRelease(&g);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

/* ---- GRIP carpet ---------------------------------------------------------- */

int gvizBuildGRIPCarpetScene(gvizScene *out, int depth) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph g = build_sierpinski_carpet(depth);

  gvizLayerGRIPLive *layer = GVIZ_ALLOC(sizeof(gvizLayerGRIPLive));
  if (!layer ||
      gvizLayerGRIPLiveInit(layer, &g, (size_t)pow(2.0, (double)depth), 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphRelease(&g);
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

/* ---- Tutte random small graph demo ---------------------------------------- */

#define TUTTE_N 8
#define TUTTE_BOUNDARY 3
#define TUTTE_EXTRA_EDGES 5
#define TUTTE_RADIUS 300.0

static int hasEdge(gvizGraph *g, int a, int b) {
  gvizArray *nb = gvizGraphGetVertexNeighbors(g, (size_t)a);
  if (!nb) return 0;
  for (size_t i = 0; i < nb->count; i++)
    if (*(size_t *)gvizArrayAtIndex(nb, i) == (size_t)b)
      return 1;
  return 0;
}

static gvizGraph buildRandomSmallGraph(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < TUTTE_N; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  /* Random spanning tree: vertex i attaches to a random earlier vertex */
  for (int i = 1; i < TUTTE_N; i++) {
    int j = rand() % i;
    gvizGraphAddEdge(&g, (size_t)i, (size_t)j);
  }

  /* Extra random edges */
  for (int k = 0; k < TUTTE_EXTRA_EDGES; k++) {
    int a = rand() % TUTTE_N;
    int b = rand() % TUTTE_N;
    if (a != b && !hasEdge(&g, a, b))
      gvizGraphAddEdge(&g, (size_t)a, (size_t)b);
  }
  return g;
}

int gvizBuildTutteDemoScene(gvizScene *out) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph g = buildRandomSmallGraph();

  size_t boundary[TUTTE_BOUNDARY] = {0, 1, 2};
  gvizLayerTutte *tlayer = GVIZ_ALLOC(sizeof(gvizLayerTutte));
  if (!tlayer || gvizLayerTutteInit(tlayer, &g, boundary, TUTTE_BOUNDARY,
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

/* ---- Polyhedral Tutte demo (octahedron edge graph) ----------------------- */

static gvizGraph buildOctahedronGraph(void) {
  /* 8 triangular faces; planar drawing uses an outer triangle 0-1-2. */
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  /* Outer triangle 0-1-2; vertex 3 inside between 0/1, vertex 4 inside between
   * 1/2, vertex 5 inside between 2/0; vertices 3-4-5 form an inner triangle. */
  size_t edges[][2] = {
      {0, 1}, {1, 2}, {2, 0},
      {0, 3}, {1, 3},
      {1, 4}, {2, 4},
      {2, 5}, {0, 5},
      {3, 4}, {4, 5}, {5, 3},
  };
  for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
    gvizGraphAddEdge(&g, edges[i][0], edges[i][1]);
  return g;
}

int gvizBuildPolyTutteDemoScene(gvizScene *out) {
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph g = buildOctahedronGraph();
  size_t outerTri[3] = {0, 1, 2};

  gvizLayerPolyTutte *layer = GVIZ_ALLOC(sizeof(gvizLayerPolyTutte));
  if (!layer || gvizLayerPolyTutteInit(layer, &g, outerTri, 3, 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphRelease(&g);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
}

int gvizBuildPolyTutteFromOBJScene(gvizScene *out, const char *objPath) {
  if (!out || !objPath) return -1;
  if (gvizSceneInit2D(out) != 0)
    return -1;

  gvizGraph g;
  size_t outerFace[8] = {0};
  size_t outerFaceLen = 0;
  if (gvizLoadOBJAsGraph(objPath, &g, outerFace, &outerFaceLen) != 0) {
    gvizSceneRelease(out);
    return -1;
  }

  gvizLayerPolyTutte *layer = GVIZ_ALLOC(sizeof(gvizLayerPolyTutte));
  if (!layer || gvizLayerPolyTutteInit(layer, &g, outerFace, outerFaceLen, 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizGraphRelease(&g);
    gvizSceneRelease(out);
    return -1;
  }
  gvizGraphRelease(&g);
  gvizSceneAddLayer(out, (gvizLayer *)layer);
  return 0;
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
