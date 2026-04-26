#ifndef _GVIZ_APP_SCENE_BUILDERS_H_
#define _GVIZ_APP_SCENE_BUILDERS_H_

#include "core/gvizScene.h"

/* Initialize an empty 2D scene. */
int gvizBuildBlankScene(gvizScene *out);

/* 2D scene: GRIP-embedded Sierpinski triangle (depth 4 recommended). */
int gvizBuildGRIPSierpinskiScene(gvizScene *out, int depth);

/* 2D scene: GRIP-embedded Sierpinski carpet (depth 4 recommended). */
int gvizBuildGRIPCarpetScene(gvizScene *out, int depth);

/* 2D scene: live Tutte barycentric embedding on a spider-web graph. */
int gvizBuildTutteDemoScene(gvizScene *out);

/* 2D scene: Reingold-Tilford layout of a random tree. */
int gvizBuildTreeDemoScene(gvizScene *out);

/* 2D scene loaded from a tree JSON file. */
int gvizBuildSceneFromTreeFile(gvizScene *out, const char *path);

/*
 * 2D scene: polyhedral Tutte demo. Builds a small 3-connected planar mesh
 * (octahedron edge graph), pins an initial face, and runs Tutte. Press
 * SPACE in-scene to scan all faces and re-embed with the largest as outer.
 */
int gvizBuildPolyTutteDemoScene(gvizScene *out);

#endif
