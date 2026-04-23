#include "app/gvizSceneBuilders.h"
#include "core/alloc.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
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
