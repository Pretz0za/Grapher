#include "ds/gvizGraph.h"
#include "ds/gvizSubgraph.h"
#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizTutteEmbedder.h"
#include "utils/graphs.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int subgraphsMatchFace(const gvizSubgraph *a, const gvizSubgraph *b) {
  if (gvizSubgraphVertexCount(a) != gvizSubgraphVertexCount(b))
    return 0;

  size_t u;
  gvizSubgraphVertexIterator vit = gvizSubgraphVertexIteratorCreate(a);
  while (gvizSubgraphVertexIterate(&vit, &u)) {
    if (!gvizSubgraphHasVertex(b, u))
      return 0;
  }

  vit = gvizSubgraphVertexIteratorCreate(a);
  while (gvizSubgraphVertexIterate(&vit, &u)) {
    size_t v;
    gvizSubgraphNeighborIterator nit = gvizSubgraphNeighborIteratorCreate(a, u);
    while (gvizSubgraphNeighborIterate(&nit, &v)) {
      if (!gvizSubgraphHasEdge(a, u, v))
        continue;
      if (!gvizSubgraphHasEdge(b, u, v) && !gvizSubgraphHasEdge(b, v, u))
        return 0;
    }
  }
  return 1;
}

static int setupTutte(size_t rows, size_t cols, gvizGraph *graph,
                      gvizTutteState *tutte) {
  *graph = build_rect_mesh(rows, cols);
  gvizGraphBuildLayout(graph);

  if (gvizTutteEmbedderInit(tutte, gvizSubgraphCreateFull(graph), 2, 1e-5) < 0)
    return -1;

  if (gvizTutteEmbedderBegin(tutte) < 0) {
    gvizTutteEmbedderRelease(tutte);
    return -1;
  }

  gvizTutteEmbedderRun(tutte, 5000);
  return 0;
}

int main(void) {
  size_t rows = 6, cols = 6;
  gvizGraph graph;
  gvizTutteState tutte;

  if (setupTutte(rows, cols, &graph, &tutte) < 0) {
    fprintf(stderr, "setup failed\n");
    return 1;
  }

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&tutte;

  printf("planar=%d converged=%d\n", gvizEmbeddedGraphIsPlanarEmbedded(eg),
         tutte.converged);

  int interiorMiss = 0;
  int interiorWrong = 0;
  int outsideMiss = 0;

  for (size_t i = 1; i + 1 < rows; i++) {
    for (size_t j = 1; j + 1 < cols; j++) {
      size_t c[4] = {i * cols + j, i * cols + j + 1, (i + 1) * cols + j + 1,
                     (i + 1) * cols + j};
      double *a = gvizEmbeddedGraphGetVPosition(eg, c[0]);
      double *b = gvizEmbeddedGraphGetVPosition(eg, c[1]);
      double *d = gvizEmbeddedGraphGetVPosition(eg, c[2]);
      double *e = gvizEmbeddedGraphGetVPosition(eg, c[3]);
      double wx = (a[0] + b[0] + d[0] + e[0]) * 0.25;
      double wy = (a[1] + b[1] + d[1] + e[1]) * 0.25;

      gvizSubgraph expected = gvizSubgraphCreateEmpty(&graph);
      for (int t = 0; t < 4; t++)
        gvizSubgraphShowVertex(&expected, c[t]);
      gvizSubgraphShowEdge(&expected, c[0], c[1]);
      gvizSubgraphShowEdge(&expected, c[1], c[2]);
      gvizSubgraphShowEdge(&expected, c[2], c[3]);
      gvizSubgraphShowEdge(&expected, c[3], c[0]);

      gvizSubgraph picked = {0};
      if (gvizEmbeddedGraphFaceSubgraphAt(eg, wx, wy, &picked) != 0) {
        interiorMiss++;
        gvizSubgraphRelease(&expected);
        continue;
      }

      if (!subgraphsMatchFace(&picked, &expected))
        interiorWrong++;

      gvizSubgraphRelease(&picked);
      gvizSubgraphRelease(&expected);
    }
  }

  double bminX = INFINITY, bminY = INFINITY;
  for (size_t u = 0; u < (size_t)gvizGraphSize(&graph); u++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, u);
    if (p[0] < bminX)
      bminX = p[0];
    if (p[1] < bminY)
      bminY = p[1];
  }

  gvizSubgraph outside = {0};
  if (gvizEmbeddedGraphFaceSubgraphAt(eg, bminX - 10.0, bminY - 10.0,
                                     &outside) != 0)
    outsideMiss = 1;
  else
    gvizSubgraphRelease(&outside);

  printf("Summary: interiorMiss=%d interiorWrong=%d outsideMiss=%d\n",
         interiorMiss, interiorWrong, outsideMiss);

  gvizTutteEmbedderRelease(&tutte);
  gvizGraphRelease(&graph);

  return (interiorMiss || interiorWrong || outsideMiss) ? 1 : 0;
}
