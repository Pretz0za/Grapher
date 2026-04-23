#ifndef _GVIZ_APP_SCENE_BUILDERS_H_
#define _GVIZ_APP_SCENE_BUILDERS_H_

#include "core/gvizScene.h"

/*
 * Initialize an empty 2D scene. Caller is responsible for releasing @p out.
 */
int gvizBuildBlankScene(gvizScene *out);

/*
 * Initialize a 2D scene containing a GRIP-embedded Sierpinski triangle of the
 * given depth (use 4 for a reasonable demo).
 */
int gvizBuildGRIPSierpinskiScene(gvizScene *out, int depth);

/*
 * Initialize a 2D scene by loading a tree JSON file. Returns 0 on success,
 * -1 on any I/O or parse failure.
 */
int gvizBuildSceneFromTreeFile(gvizScene *out, const char *path);

#endif
