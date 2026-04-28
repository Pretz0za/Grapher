#include "app/gvizLayerCreate.h"
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
#include <string.h>

/* ---- helpers ------------------------------------------------------------- */

static gvizGraph *graphToHeap(gvizGraph *src) {
  gvizGraph *h = (gvizGraph *)GVIZ_ALLOC(sizeof(gvizGraph));
  if (!h) {
    gvizGraphRelease(src);
    return NULL;
  }
  *h = *src;
  return h;
}

static void releaseEmbeddedTree(gvizEmbeddedGraph *eg) {
  gvizEmbeddedTree *t = (gvizEmbeddedTree *)eg;
  gvizEmbeddedTreeRTRelease(t);
  GVIZ_DEALLOC(t);
}

static gvizGraph buildOctahedronGraph(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < 6; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  size_t edges[][2] = {
      {0, 1}, {1, 2}, {2, 0}, {0, 3}, {1, 3},
      {1, 4}, {2, 4}, {2, 5}, {0, 5},
      {3, 4}, {4, 5}, {5, 3},
  };
  for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
    gvizGraphAddEdge(&g, edges[i][0], edges[i][1]);
  return g;
}

static void buildRandomTree(gvizGraph *tree, size_t root, int depth, int maxDepth) {
  if (depth >= maxDepth) return;
  int children = 2 + rand() % 4;
  for (int i = 0; i < children; i++) {
    gvizGraphAddVertex(tree, NULL, NULL, NULL);
    size_t child = tree->vertices.count - 1;
    gvizGraphAddEdge(tree, root, child);
    buildRandomTree(tree, child, depth + 1, maxDepth);
  }
}

/* ---- per-(algo, source) builders ----------------------------------------- */

static int loadGraphForSource(const gvizLayerCreateParams *p, gvizGraph *out,
                              size_t *outerFace, size_t *outerFaceLen,
                              size_t *outRoot) {
  gvizGraph stack;
  switch (p->source) {
  case GVIZ_SRC_DEMO_SIERPINSKI:
    stack = createSierpinski(3, NULL);
    *out = stack;
    return 0;
  case GVIZ_SRC_DEMO_OCTAHEDRON:
    stack = buildOctahedronGraph();
    *out = stack;
    if (outerFace && outerFaceLen) {
      outerFace[0] = 0; outerFace[1] = 1; outerFace[2] = 2;
      *outerFaceLen = 3;
    }
    return 0;
  case GVIZ_SRC_DEMO_RANDOM_TREE:
    gvizGraphInit(out, 1);
    gvizGraphAddVertex(out, NULL, NULL, NULL);
    buildRandomTree(out, 0, 0, 5);
    if (outRoot) *outRoot = 0;
    return 0;
  case GVIZ_SRC_FROM_FILE:
    if (!p->filepath[0]) return -1;
    /* Decide by extension. */
    {
      const char *dot = strrchr(p->filepath, '.');
      if (dot && (strcmp(dot, ".obj") == 0 || strcmp(dot, ".OBJ") == 0)) {
        size_t face[8] = {0};
        size_t flen = 0;
        if (gvizLoadOBJAsGraph(p->filepath, &stack, face, &flen) != 0)
          return -1;
        *out = stack;
        if (outerFace && outerFaceLen) {
          for (size_t i = 0; i < flen; i++) outerFace[i] = face[i];
          *outerFaceLen = flen;
        }
        return 0;
      }
      /* Default: tree JSON. */
      size_t root = 0;
      if (gvizLoadTreeJSON(p->filepath, out, &root) != 0)
        return -1;
      if (outRoot) *outRoot = root;
      return 0;
    }
  }
  return -1;
}

/* ---- algorithm-specific layer builders ----------------------------------- */

static int buildTutteLayer(gvizScene *scene, const gvizLayerCreateParams *p,
                           gvizLayer **out) {
  gvizGraph stack;
  size_t face[8] = {0};
  size_t flen = 0;
  if (loadGraphForSource(p, &stack, face, &flen, NULL) != 0) return -1;
  gvizGraph *g = graphToHeap(&stack);
  if (!g) return -1;
  gvizSceneGraphHandle h = gvizSceneRegisterGraph(scene, g);
  if (h == GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    return -1;
  }
  size_t boundary[3] = {0, 1, 2};
  size_t bcount = 3;
  if (flen >= 3) { for (size_t i = 0; i < flen && i < 3; i++) boundary[i] = face[i]; bcount = (flen < 3) ? flen : 3; }
  gvizLayerTutte *layer = GVIZ_ALLOC(sizeof(gvizLayerTutte));
  if (!layer || gvizLayerTutteInit(layer, g, boundary, bcount, 300.0, 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizSceneReleaseGraph(scene, h);
    return -1;
  }
  gvizLayerTutteBindHandle(layer, scene, h, NULL);
  gvizSceneReleaseGraph(scene, h);
  *out = (gvizLayer *)layer;
  return 0;
}

static int buildGRIPLayer(gvizScene *scene, const gvizLayerCreateParams *p,
                          gvizLayer **out) {
  gvizGraph stack;
  if (loadGraphForSource(p, &stack, NULL, NULL, NULL) != 0) return -1;
  gvizGraph *g = graphToHeap(&stack);
  if (!g) return -1;
  gvizSceneGraphHandle h = gvizSceneRegisterGraph(scene, g);
  if (h == GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    return -1;
  }
  gvizLayerGRIPLive *layer = GVIZ_ALLOC(sizeof(gvizLayerGRIPLive));
  size_t diameter = (size_t)sqrt((double)g->vertices.count) + 4;
  if (!layer || gvizLayerGRIPLiveInit(layer, g, diameter, 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizSceneReleaseGraph(scene, h);
    return -1;
  }
  gvizLayerGRIPLiveBindHandle(layer, scene, h, NULL);
  gvizSceneReleaseGraph(scene, h);
  *out = (gvizLayer *)layer;
  return 0;
}

static int buildPolyTutteLayer(gvizScene *scene, const gvizLayerCreateParams *p,
                               gvizLayer **out) {
  gvizGraph stack;
  size_t face[8] = {0};
  size_t flen = 0;
  if (loadGraphForSource(p, &stack, face, &flen, NULL) != 0) return -1;
  if (flen < 3) {
    gvizGraphRelease(&stack);
    return -1;
  }
  gvizGraph *g = graphToHeap(&stack);
  if (!g) return -1;
  gvizSceneGraphHandle h = gvizSceneRegisterGraph(scene, g);
  if (h == GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    return -1;
  }
  gvizLayerPolyTutte *layer = GVIZ_ALLOC(sizeof(gvizLayerPolyTutte));
  if (!layer || gvizLayerPolyTutteInit(layer, g, face, flen, 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    gvizSceneReleaseGraph(scene, h);
    return -1;
  }
  gvizLayerPolyTutteBindHandle(layer, scene, h, NULL);
  gvizSceneReleaseGraph(scene, h);
  *out = (gvizLayer *)layer;
  return 0;
}

static int buildRTLayer(gvizScene *scene, const gvizLayerCreateParams *p,
                        gvizLayer **out) {
  gvizGraph *g = GVIZ_ALLOC(sizeof(gvizGraph));
  if (!g) return -1;
  size_t root = 0;
  gvizGraph stack;
  if (loadGraphForSource(p, &stack, NULL, NULL, &root) != 0) {
    GVIZ_DEALLOC(g);
    return -1;
  }
  *g = stack;
  gvizSceneGraphHandle h = gvizSceneRegisterGraph(scene, g);
  if (h == GVIZ_SCENE_GRAPH_INVALID) {
    gvizGraphRelease(g); GVIZ_DEALLOC(g);
    return -1;
  }
  gvizEmbeddedTree *tree = GVIZ_ALLOC(sizeof(gvizEmbeddedTree));
  if (!tree || gvizEmbeddedTreeRTInit(tree, g, root) != 0) {
    if (tree) GVIZ_DEALLOC(tree);
    gvizSceneReleaseGraph(scene, h);
    return -1;
  }
  gvizEmbeddedTreeCalculateOffsets(tree, root, 0);
  double origin[2] = {0.0, 0.0};
  gvizEmbeddedTreeEmbed(tree, root, origin);
  gvizLayerGraph *layer = GVIZ_ALLOC(sizeof(gvizLayerGraph));
  if (!layer) {
    releaseEmbeddedTree((gvizEmbeddedGraph *)tree);
    gvizSceneReleaseGraph(scene, h);
    return -1;
  }
  gvizViewport vp = {0, 0, 0, 0};
  gvizLayerGraphInit(layer, (gvizEmbeddedGraph *)tree, releaseEmbeddedTree, vp, 0);
  gvizLayerGraphBindHandle(layer, scene, h, NULL);
  gvizSceneReleaseGraph(scene, h);
  *out = (gvizLayer *)layer;
  return 0;
}

static int buildEmptyLayer(gvizScene *scene, const gvizLayerCreateParams *p,
                           gvizLayer **out) {
  (void)scene; (void)p;
  gvizLayerTutte *layer = GVIZ_ALLOC(sizeof(gvizLayerTutte));
  if (!layer || gvizLayerTutteInitEmpty(layer, 0) != 0) {
    if (layer) GVIZ_DEALLOC(layer);
    return -1;
  }
  *out = (gvizLayer *)layer;
  return 0;
}

int gvizCreateLayerFromParams(gvizScene *scene,
                              const gvizLayerCreateParams *params,
                              gvizLayer **out) {
  if (!scene || !params || !out) return -1;
  *out = NULL;
  switch (params->algo) {
  case GVIZ_CREATE_TUTTE:     return buildTutteLayer(scene, params, out);
  case GVIZ_CREATE_GRIP:      return buildGRIPLayer(scene, params, out);
  case GVIZ_CREATE_POLYTUTTE: return buildPolyTutteLayer(scene, params, out);
  case GVIZ_CREATE_RT:        return buildRTLayer(scene, params, out);
  case GVIZ_CREATE_EMPTY:     return buildEmptyLayer(scene, params, out);
  }
  return -1;
}

int gvizApplyLayerCreate(gvizScene *scene, const gvizLayerCreateParams *p) {
  if (!scene || !p) return -1;
  gvizLayer *layer = NULL;
  if (gvizCreateLayerFromParams(scene, p, &layer) != 0) return -1;
  /* Set scene mode (only meaningful in empty case). */
  if (p->slotKind == GVIZ_SLOT_NEW_EMPTY_SCENE) {
    scene->mode = p->mode;
  } else if (p->slotKind == GVIZ_SLOT_SPLIT_H) {
    scene->layout.split = GVIZ_SPLIT_H;
    scene->layout.splitRatio = 0.5f;
  } else if (p->slotKind == GVIZ_SLOT_SPLIT_V) {
    scene->layout.split = GVIZ_SPLIT_V;
    scene->layout.splitRatio = 0.5f;
  }
  if (gvizSceneAddLayer(scene, layer) != 0) {
    if (layer->vtable && layer->vtable->release) layer->vtable->release(layer);
    GVIZ_DEALLOC(layer);
    return -1;
  }
  return 0;
}
