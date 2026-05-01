#include "renderer/embeddings/gvizPlanarRotation.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizGraph.h"
#include <math.h>
#include <stdlib.h>

static gvizEmbeddedGraph *g_ctxEG = NULL;
static double g_ctxOrigin[2] = {0.0, 0.0};

static int rotationCmp(const void *a, const void *b) {
  size_t va = *(const size_t *)a;
  size_t vb = *(const size_t *)b;
  double *pa = gvizEmbeddedGraphGetVPosition(g_ctxEG, va);
  double *pb = gvizEmbeddedGraphGetVPosition(g_ctxEG, vb);
  double aa = atan2(pa[1] - g_ctxOrigin[1], pa[0] - g_ctxOrigin[0]);
  double ab = atan2(pb[1] - g_ctxOrigin[1], pb[0] - g_ctxOrigin[0]);
  if (aa < ab) return -1;
  if (aa > ab) return  1;
  return 0;
}

void gvizComputeRotationSystem(gvizEmbeddedGraph *eg) {
  if (!eg || !eg->view.graph) return;
  size_t N = eg->view.graph->vertices.count;
  for (size_t u = 0; u < N; u++) {
    double *pu = gvizEmbeddedGraphGetVPosition(eg, u);
    g_ctxEG = eg;
    g_ctxOrigin[0] = pu[0];
    g_ctxOrigin[1] = pu[1];
    gvizArray *nbrs = gvizGraphGetVertexNeighbors(eg->view.graph, u);
    if (nbrs->count > 1)
      qsort(nbrs->arr, nbrs->count, sizeof(size_t), rotationCmp);
  }
  g_ctxEG = NULL;
}
