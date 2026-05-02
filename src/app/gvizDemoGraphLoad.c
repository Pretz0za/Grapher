#include "app/gvizDemoGraphLoad.h"
#include "core/alloc.h"
#include "utils/graphs.h"
#include <stdlib.h>

static void buildRandomTree(gvizGraph *tree, size_t root, int depth,
                            int maxDepth) {
  if (depth >= maxDepth) return;
  int children = 2 + rand() % 4;
  for (int i = 0; i < children; i++) {
    gvizGraphAddVertex(tree, NULL, NULL, NULL);
    size_t child = tree->vertices.count - 1;
    gvizGraphAddEdge(tree, root, child);
    buildRandomTree(tree, child, depth + 1, maxDepth);
  }
}

int gvizLoadDemoGraph(const gvizLayerCreateParams *p, gvizGraph *out,
                      size_t *outerFace, size_t *outerFaceLen,
                      size_t *outRoot) {
  int p1 = p->graphParam1 > 0 ? p->graphParam1 : 3;
  int p2 = p->graphParam2 > 0 ? p->graphParam2 : 3;
  switch (p->graphType) {
  case GVIZ_DEMO_SIERPINSKI_TRI: {
    SierpinskiTriangle st;
    *out = createSierpinski(p1, &st);
    if (outerFace && outerFaceLen) {
      outerFace[0] = st.t; outerFace[1] = st.l; outerFace[2] = st.r;
      *outerFaceLen = 3;
    }
    return 0;
  }
  case GVIZ_DEMO_SIERPINSKI_TET:
    *out = createSierpinskiTetrahedron(p1, NULL);
    return 0;
  case GVIZ_DEMO_CARPET:
    *out = build_sierpinski_carpet((size_t)p1);
    return 0;
  case GVIZ_DEMO_RECT_MESH:
    *out = build_rect_mesh((size_t)p1, (size_t)p2);
    return 0;
  case GVIZ_DEMO_TET_MESH:
    *out = build_tetrahedral_mesh((size_t)p1);
    return 0;
  case GVIZ_DEMO_EQ_TRI_MESH:
    *out = build_equilateral_tri_mesh((size_t)p1);
    return 0;
  case GVIZ_DEMO_KNOTTED_RECT:
    *out = build_knotted_rect_mesh((size_t)p1, (size_t)p2);
    return 0;
  case GVIZ_DEMO_MOBIUS:
    *out = build_mobius_strip((size_t)p1, (size_t)p2);
    return 0;
  case GVIZ_DEMO_KLEIN:
    *out = build_klein_bottle((size_t)p1, (size_t)p2);
    return 0;
  case GVIZ_DEMO_RANDOM_TREE:
    gvizGraphInit(out, 1);
    gvizGraphAddVertex(out, NULL, NULL, NULL);
    buildRandomTree(out, 0, 0, p1);
    if (outRoot) *outRoot = 0;
    return 0;
  default:
    return -1;
  }
}

gvizGraph *gvizGraphToHeap(gvizGraph *src) {
  gvizGraph *h = (gvizGraph *)GVIZ_ALLOC(sizeof(gvizGraph));
  if (!h) {
    gvizGraphRelease(src);
    return NULL;
  }
  *h = *src;
  return h;
}
