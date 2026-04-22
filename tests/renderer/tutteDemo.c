#include "dsa/gvizGraph.h"
#include "msf_gif.h"
#include "raylib.h"
#include "raymath.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/embeddings/gvizTutteEmbedding.h"
#include "rlgl.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

#define SPOKES 6
#define RINGS 3 /* including boundary ring */
#define BOUNDARY_R 300.0
#define GIF_FRAMERATE 2

/*
 * Spider-web graph: RINGS concentric hexagonal rings connected by radial
 * spokes. Ring 0 is the boundary (vertices 0..5). Ring 1..RINGS-2 are interior
 * rings. Last vertex is the center hub.
 *
 * Vertex layout:
 *   ring r, spoke s  →  r*SPOKES + s   (r in [0, RINGS-1])
 *   center hub       →  RINGS*SPOKES
 */
#define TOTAL_VERTS (RINGS * SPOKES + 1)

static gvizGraph buildSpiderWeb(void) {
  gvizGraph g;
  gvizGraphInit(&g, 0);

  for (int i = 0; i < TOTAL_VERTS; i++)
    gvizGraphAddVertex(&g, NULL, NULL, NULL);

  /* Ring edges */
  for (int r = 0; r < RINGS; r++) {
    for (int s = 0; s < SPOKES; s++) {
      size_t a = (size_t)(r * SPOKES + s);
      size_t b = (size_t)(r * SPOKES + (s + 1) % SPOKES);
      gvizGraphAddEdge(&g, a, b);
    }
  }

  /* Spoke edges: ring r → ring r+1 */
  for (int r = 0; r < RINGS - 1; r++) {
    for (int s = 0; s < SPOKES; s++) {
      size_t a = (size_t)(r * SPOKES + s);
      size_t b = (size_t)((r + 1) * SPOKES + s);
      gvizGraphAddEdge(&g, a, b);
    }
  }

  /* Innermost ring → center hub */
  size_t hub = RINGS * SPOKES;
  for (int s = 0; s < SPOKES; s++)
    gvizGraphAddEdge(&g, (size_t)((RINGS - 1) * SPOKES + s), hub);

  return g;
}

/* Seed interior vertices at random positions within the boundary circle. */
static void seedRandom(gvizTutteState *s) {
  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)s;
  size_t N = eg->graph->vertices.count;
  for (size_t u = 0; u < N; u++) {
    if (gvizTestBit(s->isBoundary, u))
      continue;
    double angle = ((double)rand() / RAND_MAX) * 2.0 * M_PI;
    double r = ((double)rand() / RAND_MAX) * (BOUNDARY_R * 0.8);
    double p[2] = {r * cos(angle), r * sin(angle)};
    gvizEmbeddedGraphSetVPosition(eg, u, p);
  }
  s->iteration = 0;
  s->lastMaxDelta = 0.0;
  s->converged = 0;
}

/* ---- GPU geometry ---------------------------------------------------------
 */

static size_t countEdgeVerts(gvizEmbeddedGraph *eg) {
  size_t total = 0;
  for (size_t i = 0; i < eg->graph->vertices.count; i++)
    total += gvizGraphGetVertexNeighbors(eg->graph, i)->count * 2;
  return total;
}

static float *buildEdgeVerts(gvizEmbeddedGraph *eg, size_t *outCount) {
  size_t total = countEdgeVerts(eg);
  float *v = malloc(total * 2 * sizeof(float));
  if (!v)
    return NULL;

  size_t vi = 0;
  for (size_t i = 0; i < eg->graph->vertices.count; i++) {
    double *p = gvizEmbeddedGraphGetVPosition(eg, i);
    gvizArray *nb = gvizGraphGetVertexNeighbors(eg->graph, i);
    for (size_t j = 0; j < nb->count; j++) {
      size_t other = *(size_t *)gvizArrayAtIndex(nb, j);
      double *op = gvizEmbeddedGraphGetVPosition(eg, other);
      v[vi++] = (float)p[0];
      v[vi++] = (float)p[1];
      v[vi++] = (float)op[0];
      v[vi++] = (float)op[1];
    }
  }
  *outCount = total;
  return v;
}

static void uploadEdges(unsigned int vbo, gvizEmbeddedGraph *eg) {
  size_t count;
  float *v = buildEdgeVerts(eg, &count);
  if (!v)
    return;
  rlUpdateVertexBuffer(vbo, v, (int)(count * 2 * sizeof(float)), 0);
  free(v);
}

static unsigned int createEdgeVAO(gvizEmbeddedGraph *eg, unsigned int *vboOut) {
  size_t count;
  float *v = buildEdgeVerts(eg, &count);

  unsigned int vao = rlLoadVertexArray();
  rlEnableVertexArray(vao);
  unsigned int vbo =
      rlLoadVertexBuffer(v, (int)(count * 2 * sizeof(float)), true);
  rlSetVertexAttribute(0, 2, RL_FLOAT, 0, 0, 0);
  rlEnableVertexAttribute(0);
  rlDisableVertexArray();
  free(v);

  *vboOut = vbo;
  return vao;
}

/* ---- draw -----------------------------------------------------------------
 */

static void drawEdges(unsigned int vao, gvizEmbeddedGraph *eg) {
  rlEnableShader(rlGetShaderIdDefault());
  float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  int *locs = rlGetShaderLocsDefault();
  rlSetUniform(locs[RL_SHADER_LOC_COLOR_DIFFUSE], black, RL_SHADER_UNIFORM_VEC4,
               1);
  Matrix mvp = MatrixMultiply(
      MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixTransform()),
      rlGetMatrixProjection());
  rlSetUniformMatrix(locs[RL_SHADER_LOC_MATRIX_MVP], mvp);
  rlEnableVertexArray(vao);
  glDrawArrays(GL_LINES, 0, (int)countEdgeVerts(eg));
  rlDisableVertexArray();
  rlDisableShader();
}

static void drawHUD(const gvizTutteState *s, bool paused) {
  char hud[128];
  snprintf(hud, sizeof(hud), "iter: %zu  delta: %.2e  %s%s", s->iteration,
           s->lastMaxDelta, s->converged ? "[CONVERGED] " : "",
           paused ? "[PAUSED]" : "");
  DrawText(hud, 10, 10, 18, DARKGRAY);
  DrawText("SPACE pause  S step  R reset  G gif  RMB pan  wheel zoom", 10, 32,
           14, GRAY);
}

/* ---- GIF ------------------------------------------------------------------
 */

static MsfGifState gifState = {0};
static bool gifRecording = false;
static int gifFrameCounter = 0;

static void handleGIF(void) {
  if (!gifRecording) {
    gifRecording = true;
    gifFrameCounter = 0;
    msf_gif_begin(&gifState, GetRenderWidth(), GetRenderHeight());
  } else {
    gifRecording = false;
    MsfGifResult r = msf_gif_end(&gifState);
    SaveFileData("tests/renderer/out/tutte.gif", r.data,
                 (unsigned int)r.dataSize);
    msf_gif_free(r);
  }
}

static void tickGIF(void) {
  if (!gifRecording || ++gifFrameCounter <= GIF_FRAMERATE)
    return;
  Image img = LoadImageFromScreen();
  int cs = (int)((GIF_FRAMERATE / 60.0f) * 100);
  msf_gif_frame(&gifState, img.data, cs, 16, img.width * 4);
  gifFrameCounter = 0;
  UnloadImage(img);
}

/* ---- camera ---------------------------------------------------------------
 */

static void handleCamera(Camera2D *cam) {
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
    Vector2 d = GetMouseDelta();
    cam->target.x -= d.x / cam->zoom;
    cam->target.y -= d.y / cam->zoom;
  }
  float w = GetMouseWheelMove();
  if (w != 0) {
    cam->zoom *= (w > 0) ? 1.1f : (1.0f / 1.1f);
    if (cam->zoom < 0.05f)
      cam->zoom = 0.05f;
  }
}

/* ---- main -----------------------------------------------------------------
 */

int main(void) {
  srand(42);

  gvizGraph g = buildSpiderWeb();

  gvizTutteState s;
  gvizTutteEmbeddingInit(&s, &g, 2, 1e-5);
  s.relaxationRate = 10.0;

  /* Pin the outer hexagonal ring (ring 0, vertices 0..SPOKES-1). */
  size_t boundary[SPOKES];
  for (int i = 0; i < SPOKES; i++)
    boundary[i] = (size_t)i;
  gvizTutteFixConvexPolygon(&s, boundary, SPOKES, BOUNDARY_R);

  seedRandom(&s);

  gvizEmbeddedGraph *eg = (gvizEmbeddedGraph *)&s;

  InitWindow(900, 900, "Tutte Barycentric Embedding");
  SetTargetFPS(60);

  Camera2D camera = {
      .offset = (Vector2){450, 450},
      .target = (Vector2){0, 0},
      .zoom = 1.0f,
  };

  unsigned int vbo;
  unsigned int vao = createEdgeVAO(eg, &vbo);
  bool paused = false;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_SPACE))
      paused = !paused;
    if (IsKeyPressed(KEY_S))
      gvizTutteEmbeddingStep(&s, dt);
    if (IsKeyPressed(KEY_R))
      seedRandom(&s);
    if (IsKeyPressed(KEY_G))
      handleGIF();
    handleCamera(&camera);

    if (!paused && !s.converged)
      gvizTutteEmbeddingStep(&s, dt);

    uploadEdges(vbo, eg);

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode2D(camera);
    drawEdges(vao, eg);
    EndMode2D();
    drawHUD(&s, paused);
    EndDrawing();

    tickGIF();
  }

  rlUnloadVertexArray(vao);
  rlUnloadVertexBuffer(vbo);
  CloseWindow();
  gvizTutteEmbeddingRelease(&s);
  gvizGraphRelease(&g);
  return 0;
}
