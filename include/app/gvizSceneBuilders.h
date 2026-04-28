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

/*
 * 2D scene: polyhedral Tutte on a mesh loaded from a Wavefront .obj file.
 * The first face of the .obj is used as the outer boundary.
 * Returns 0 on success, -1 on parse / IO / allocation failure.
 */
int gvizBuildPolyTutteFromOBJScene(gvizScene *out, const char *objPath);

/*
 * 3D scene: render the .obj as a textured mesh model only. No graph.
 * Returns 0 on success, -1 on load failure.
 */
int gvizBuildOBJSceneFromFile(gvizScene *out, const char *objPath);

/*
 * Mixed scene with two layers side-by-side: an OBJ mesh layer (3D) and a
 * PolyTutte graph layer (2D). The OBJ data is loaded twice — once as a
 * Model for rendering, once as a gvizGraph for the embedding.
 */
int gvizBuildOBJAndPolyTutteSceneFromFile(gvizScene *out, const char *objPath);

#endif
