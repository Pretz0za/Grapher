#include "app/gvizLayerPolyTutte.h"
#include "dsa/gvizGraph.h"
#include <assert.h>
#include <stdio.h>

#define SPOKES 6
#define RINGS 3
#define TOTAL_VERTS (RINGS * SPOKES + 1)

static gvizGraph buildSpiderWeb(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);
  for (int i = 0; i < TOTAL_VERTS; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);
  for (int r = 0; r < RINGS; r++)
    for (int s = 0; s < SPOKES; s++)
      gvizGraphAddEdge(&g, (size_t)(r * SPOKES + s),
                       (size_t)(r * SPOKES + (s + 1) % SPOKES));
  for (int r = 0; r < RINGS - 1; r++)
    for (int s = 0; s < SPOKES; s++)
      gvizGraphAddEdge(&g, (size_t)(r * SPOKES + s),
                       (size_t)((r + 1) * SPOKES + s));
  size_t hub = RINGS * SPOKES;
  for (int s = 0; s < SPOKES; s++)
    gvizGraphAddEdge(&g, (size_t)((RINGS - 1) * SPOKES + s), hub);
  return g;
}

int main(void) {
  gvizGraph g = buildSpiderWeb();

  size_t outer[SPOKES];
  for (int i = 0; i < SPOKES; i++)
    outer[i] = (size_t)i;

  gvizLayerPolyTutte layer;
  int ok = gvizLayerPolyTutteInit(&layer, &g, outer, SPOKES, 0);
  assert(ok == 0 && "gvizLayerPolyTutteInit failed");

  gvizLayerPolyTutteRelease(&layer);
  gvizGraphRelease(&g);

  printf("polytutteDemo: OK\n");
  return 0;
}
