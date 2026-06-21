#include "cblas.h"
#include "dsa/gvizGraph.h"
#include "lapack.h"
#define MSF_GIF_IMPL
#include "msf_gif.h"
#include "raylib.h"
#include "raymath.h"
#include "renderer/embeddings/gvivGRIPEmbedding.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "renderer/gvizRenderer.h"
#include "renderer/layers/gvizLayerGraph.h"
#include "rlgl.h"
#include "utils/graphs.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

MsfGifState gifState = {0};
bool gifRecording = false;
int gifFrameCounter = 0;
#define GIF_FRAMERATE 2

// Radius of rendered vertex circles in world-space units.
#define VERTEX_RADIUS 1.0f
// Number of triangle-fan segments for each circle (higher = smoother).
#define CIRCLE_SEGMENTS 16

// Mutates positions in place: N*4 row-major -> N*3 row-major
// After call, positions[i*3 + 0..2] holds the 3D projection
void projectPCA(double *positions, size_t N) {
  // center
  double mean[4] = {0};
  for (size_t i = 0; i < N; i++)
    for (size_t d = 0; d < 4; d++)
      mean[d] += positions[i * 4 + d] / (double)N;

  double *centered = malloc(N * 4 * sizeof(double));
  for (size_t i = 0; i < N; i++)
    for (size_t d = 0; d < 4; d++)
      centered[i * 4 + d] = positions[i * 4 + d] - mean[d];

  // SVD of centered (N x 4), row-major
  // dgesvd expects column-major, so we pass centered^T (4 x N)
  // and swap m/n accordingly, using "S","S" to get left singular vectors
  // which correspond to principal components in the transposed sense.
  // Simpler: just compute covariance matrix (4x4) and its eigenvectors.
  // C = centered^T * centered / N  (4x4, manageable)

  double C[16] = {0}; // 4x4 covariance, column-major for LAPACK
  // C = centered^T * centered
  // cblas: C = alpha * A^T * A + beta * C
  cblas_dsyrk(CblasColMajor, CblasUpper, CblasTrans, 4, N, 1.0 / (double)N,
              centered, N, 0.0, C, 4);

  // symmetrize (dsyrk only fills upper triangle)
  for (int i = 0; i < 4; i++)
    for (int j = i + 1; j < 4; j++)
      C[j * 4 + i] = C[i * 4 + j];

  // eigendecomposition of C: dsyev gives eigenvalues ascending,
  // eigenvectors as columns of C on output
  double eigenvalues[4];
  double worksize;
  int lwork = -1, info, n = 4;

  dsyev_("V", "U", &n, C, &n, eigenvalues, &worksize, &lwork, &info, 1, 1);
  lwork = (int)worksize;
  double *work = malloc(lwork * sizeof(double));
  dsyev_("V", "U", &n, C, &n, eigenvalues, work, &lwork, &info, 1, 1);
  free(work);

  // eigenvalues are ascending so last 3 columns of C are top 3 eigenvectors
  // C is column-major so column k starts at C + k*4
  double *pc1 = C + 3 * 4; // largest variance
  double *pc2 = C + 2 * 4;
  double *pc3 = C + 1 * 4;

  // project and write back in place (3 doubles per vertex)
  for (size_t i = 0; i < N; i++) {
    double *row = centered + i * 4;
    double x = cblas_ddot(4, row, 1, pc1, 1);
    double y = cblas_ddot(4, row, 1, pc2, 1);
    double z = cblas_ddot(4, row, 1, pc3, 1);
    positions[i * 3 + 0] = x;
    positions[i * 3 + 1] = y;
    positions[i * 3 + 2] = z;
  }

  free(centered);
}

// ---------------------------------------------------------------------------
// Vertex shader: instanced filled-circle rendering.
//
// attrib 0 (vec3): local offset within the unit circle (pre-scaled by R).
//                  z is always 0 for a 2D disc; kept as vec3 so the same
//                  shader works in both 2D and 3D modes.
// attrib 1 (vec3): per-instance world-space centre of the circle (divisor=1).
//
// The MVP is Raylib's standard MVP uniform (matMVP).
// ---------------------------------------------------------------------------
static const char *circleVS =
    "#version 330 core\n"
    "layout(location = 0) in vec3 localPos;\n"    // unit-circle geometry
    "layout(location = 1) in vec3 instancePos;\n" // per-instance world centre
    "uniform mat4 matMVP;\n"
    "void main() {\n"
    "    vec3 worldPos = instancePos + localPos;\n"
    "    gl_Position = matMVP * vec4(worldPos, 1.0);\n"
    "}\n";

static const char *circleFS =
    "#version 330 core\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = vec4(0.0, 0.0, 0.0, 1.0);\n" // solid black
    "}\n";

int main() {

  size_t WIDTH = 200, HEIGHT = 100;
  size_t DEPTH = 7;

  printf("initializing state\n");
  gvizGRIPState state;
  gvizGraph g;
  // g = build_tetrahedral_mesh(DEPTH);
  // gvizGraph klein = build_klein_bottle(WIDTH, HEIGHT);
  // gvizGraph mobius = build_mobius_strip(WIDTH, HEIGHT);
  // gvizGraph knottedMesh = build_knotted_rect_mesh(WIDTH, HEIGHT);
  // g = createSierpinskiTetrahedron(DEPTH, NULL);
  // gvizGraph mesh = build_rect_mesh(WIDTH, HEIGHT);
  g = createSierpinski(DEPTH, NULL);
  // gvizGraph trimesh = build_equilateral_tri_mesh(DEPTH);
  // g = build_sierpinski_carpet(DEPTH);
  // gvizGRIPEmbeddingInit(&state, &mesh, (WIDTH - 1) + (HEIGHT - 1) + 10, 2);
  // gvizGRIPEmbeddingInit(&state, &g, pow(2, DEPTH), 2);
  // gvizGRIPEmbeddingInit(&state, &g, DEPTH, 2); // trimesh 2d
  // gvizGRIPEmbeddingInit(&state, &g, DEPTH, 3); // trimesh 3d
  // gvizGRIPEmbeddingInit(&state, &g, pow(4, DEPTH), 2);
  // gvizGRIPEmbeddingInit(&state, &mobius, pow(2, DEPTH + 1), 4);
  gvizGRIPEmbeddingInit(&state, &g, pow(2, DEPTH + 1), 2); // 3d sierpinski

  printf("creating filtration\n");
  gvizGRIPEmbeddingBegin(&state);
  // gvizGRIPEmbeddingEmbed(&state);
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)&state;

  // assert(!gvizEmbeddedGraphLoadEmbedding(embedding, "3d_sierpinski_11.txt"));

  // gvizEmbeddedGraphSaveEmbedding(embedding,
  //                                "Depth 11 Sierpinski Tetrahedron Embedding",
  //                                "3d_sierpinski_11.txt");
  //
  if (state.graph.embedding.dim == 4) {
    projectPCA(state.graph.embedding.vertexPositions, g.vertices.count);
    state.graph.embedding.dim = 3;
  }

  int is3D = (state.graph.embedding.dim == 3);

  Vector3 centroid = {0};
  Vector2 centroid2D = {0};
  {
    size_t N = state.graph.graph->vertices.count;
    for (size_t i = 0; i < N; i++) {
      double *pos =
          gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, i);
      centroid2D.x += (float)pos[0];
      centroid2D.y += (float)pos[1];
      if (is3D) {
        centroid.x += (float)pos[0];
        centroid.y += (float)pos[1];
        centroid.z += (float)pos[2];
      }
    }
    centroid2D.x /= (float)N;
    centroid2D.y /= (float)N;
    if (is3D) {
      centroid.x /= (float)N;
      centroid.y /= (float)N;
      centroid.z /= (float)N;
    }
  }

  InitWindow(1000, 1000, "graphvis");
  SetTargetFPS(60);

  int monitor = 1; // 0 = primary, 1 = second
  int monitorX = GetMonitorPosition(monitor).x;
  int monitorY = GetMonitorPosition(monitor).y;
  SetWindowPosition(monitorX + 750, monitorY);

  Camera3D camera3D = {.position =
                           (Vector3){centroid.x, centroid.y, centroid.z + 5000},
                       .target = centroid,
                       .up = (Vector3){0, 1, 0},
                       .fovy = 45.0f,
                       .projection = CAMERA_PERSPECTIVE};
  Camera2D camera2D = {.offset = (Vector2){500, 500},
                       .target = centroid2D,
                       .rotation = 0.0f,
                       .zoom = 1.0f};

  gvizLayerGraph layer;
  gvizViewport viewport = {0, 0, 800, 600};
  gvizLayerGraphInit(&layer, (gvizEmbeddedGraph *)&state, viewport, 999);

  size_t N = embedding->graph->vertices.count;

  // -------------------------------------------------------------------------
  // Edge buffers (dynamic – re-uploaded every frame).
  // -------------------------------------------------------------------------
  size_t edgeVertexCount = 0;
  for (size_t i = 0; i < N; i++) {
    gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, i);
    edgeVertexCount += children->count * 2;
  }

  // CPU-side scratch buffer reused every frame.
  float *edgeVerts = (float *)malloc(edgeVertexCount * 3 * sizeof(float));

  // Initial fill so the VBO has valid data before the first draw.
  {
    size_t vi = 0;
    for (size_t i = 0; i < N; i++) {
      double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
      gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, i);
      for (size_t j = 0; j < children->count; j++) {
        double *otherPos = gvizEmbeddedGraphGetVPosition(
            embedding, *(size_t *)gvizArrayAtIndex(children, j));
        edgeVerts[vi++] = (float)pos[0];
        edgeVerts[vi++] = (float)pos[1];
        edgeVerts[vi++] = is3D ? (float)pos[2] : 0.0f;
        edgeVerts[vi++] = (float)otherPos[0];
        edgeVerts[vi++] = (float)otherPos[1];
        edgeVerts[vi++] = is3D ? (float)otherPos[2] : 0.0f;
      }
    }
  }

  // Upload as a DYNAMIC buffer (true = dynamic).
  unsigned int graphVAO = rlLoadVertexArray();
  rlEnableVertexArray(graphVAO);
  unsigned int graphVBO = rlLoadVertexBuffer(
      edgeVerts, (int)(edgeVertexCount * 3 * sizeof(float)), true);
  rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
  rlEnableVertexAttribute(0);
  rlDisableVertexArray();

  int graphVertexCount = (int)edgeVertexCount;

  // -------------------------------------------------------------------------
  // Instanced vertex-circle buffers.
  //
  // VAO layout:
  //   attrib 0 (divisor 0): local circle geometry  (CIRCLE_SEGMENTS+2 verts)
  //   attrib 1 (divisor 1): per-instance world pos (N verts, dynamic)
  // -------------------------------------------------------------------------

  // Build unit-circle triangle-fan geometry (radius = VERTEX_RADIUS).
  // Layout: centre (0,0,0), then CIRCLE_SEGMENTS rim points, then close
  // back to the first rim point → CIRCLE_SEGMENTS+2 vertices total.
  int circleVtxCount = CIRCLE_SEGMENTS + 2;
  float *circleLocal = (float *)malloc(circleVtxCount * 3 * sizeof(float));
  circleLocal[0] = 0.0f;
  circleLocal[1] = 0.0f;
  circleLocal[2] = 0.0f;
  for (int s = 0; s <= CIRCLE_SEGMENTS; s++) {
    float angle = (float)s / (float)CIRCLE_SEGMENTS * 2.0f * (float)M_PI;
    circleLocal[(s + 1) * 3 + 0] = cosf(angle) * VERTEX_RADIUS;
    circleLocal[(s + 1) * 3 + 1] = sinf(angle) * VERTEX_RADIUS;
    circleLocal[(s + 1) * 3 + 2] = 0.0f;
  }

  // Per-instance world positions (3 floats per vertex, dynamic).
  float *instancePos = (float *)malloc(N * 3 * sizeof(float));

  Shader circleShader = LoadShaderFromMemory(circleVS, circleFS);
  int mvpLoc =
      GetShaderLocation(circleShader, "matMVP"); // Build the circle VAO.

  unsigned int circleVAO = rlLoadVertexArray();
  rlEnableVertexArray(circleVAO);

  // attrib 0: static circle geometry.
  unsigned int circleGeomVBO = rlLoadVertexBuffer(
      circleLocal, circleVtxCount * 3 * (int)sizeof(float), false);
  rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
  rlEnableVertexAttribute(0);
  glVertexAttribDivisor(0, 0); // one vertex per circle vertex (non-instanced)

  // attrib 1: per-instance positions (initially zeroed, re-uploaded per frame).
  unsigned int circleInstVBO =
      rlLoadVertexBuffer(instancePos, (int)(N * 3 * sizeof(float)), true);
  rlSetVertexAttribute(1, 3, RL_FLOAT, 0, 0, 0);
  rlEnableVertexAttribute(1);
  glVertexAttribDivisor(1, 1); // advance once per instance

  rlDisableVertexArray();
  free(circleLocal);

  unsigned char currOpacity = 0xFF;

  while (!WindowShouldClose()) {

    runRefinementRound(&state);

    if (is3D)
      gvizRenderer3DCameraUpdate(&camera3D, centroid);
    else
      gvizRenderer2DCameraUpdate(&camera2D);

    // screenshot
    if (IsKeyPressed(KEY_G)) {
      if (!gifRecording) {
        gifRecording = true;
        gifFrameCounter = 0;
        msf_gif_begin(&gifState, GetRenderWidth(), GetRenderHeight());
      } else {
        gifRecording = false;
        MsfGifResult result = msf_gif_end(&gifState);
        SaveFileData("tests/renderer/out/output.gif", result.data,
                     (unsigned int)result.dataSize);
        msf_gif_free(result);
      }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
      beginNewStage(&state);
    }

    // -----------------------------------------------------------------------
    // Update GPU buffers with current vertex positions.
    // -----------------------------------------------------------------------

    // Rebuild edge buffer.
    {
      size_t vi = 0;
      for (size_t i = 0; i < N; i++) {
        double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
        gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, i);
        for (size_t j = 0; j < children->count; j++) {
          double *otherPos = gvizEmbeddedGraphGetVPosition(
              embedding, *(size_t *)gvizArrayAtIndex(children, j));
          edgeVerts[vi++] = (float)pos[0];
          edgeVerts[vi++] = (float)pos[1];
          edgeVerts[vi++] = is3D ? (float)pos[2] : 0.0f;
          edgeVerts[vi++] = (float)otherPos[0];
          edgeVerts[vi++] = (float)otherPos[1];
          edgeVerts[vi++] = is3D ? (float)otherPos[2] : 0.0f;
        }
      }
      rlUpdateVertexBuffer(graphVBO, edgeVerts,
                           (int)(edgeVertexCount * 3 * sizeof(float)), 0);
    }

    // Rebuild per-instance position buffer.
    for (size_t i = 0; i < N; i++) {
      double *pos = gvizEmbeddedGraphGetVPosition(embedding, i);
      instancePos[i * 3 + 0] = (float)pos[0];
      instancePos[i * 3 + 1] = (float)pos[1];
      instancePos[i * 3 + 2] = is3D ? (float)pos[2] : 0.0f;
    }
    rlUpdateVertexBuffer(circleInstVBO, instancePos,
                         (int)(N * 3 * sizeof(float)), 0);

    // -----------------------------------------------------------------------
    // Render.
    // -----------------------------------------------------------------------
    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      rlSetClipPlanes(0.1, 1000000.0); // near, far
      if (is3D)
        BeginMode3D(camera3D);
      else
        BeginMode2D(camera2D);

      // Compute MVP once; used by both the edge and circle passes.
      Matrix mvp = MatrixMultiply(
          MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixTransform()),
          rlGetMatrixProjection());

      // -------------------------------------------------------------------
      // Edge pass – only when currLayer == 0.
      // -------------------------------------------------------------------
      if (state.currLayer == 0) {
        rlEnableShader(rlGetShaderIdDefault());
        float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        int *locs = rlGetShaderLocsDefault();
        rlSetUniform(locs[RL_SHADER_LOC_COLOR_DIFFUSE], black,
                     RL_SHADER_UNIFORM_VEC4, 1);
        rlSetUniformMatrix(locs[RL_SHADER_LOC_MATRIX_MVP], mvp);
        rlEnableVertexArray(graphVAO);
        glDrawArrays(GL_LINES, 0, graphVertexCount);
        rlDisableVertexArray();
        rlDisableShader();
      }

      // -------------------------------------------------------------------
      // Vertex (circle) pass – always rendered, every layer.
      // Uses the custom instanced shader; one draw call for all N circles.
      // -------------------------------------------------------------------
      glUseProgram(circleShader.id);
      glUniformMatrix4fv(mvpLoc, 1, false, MatrixToFloat(mvp));
      // Upload the same MVP that Raylib computed above.
      rlEnableVertexArray(circleVAO);
      glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, circleVtxCount, (GLsizei)N);
      rlDisableVertexArray();
      glUseProgram(0);

      if (is3D)
        EndMode3D();
      else
        EndMode2D();

      currOpacity = 0xFF;
    }

    EndDrawing();

    if (gifRecording && ++gifFrameCounter > GIF_FRAMERATE) {
      Image img = LoadImageFromScreen();
      int centiseconds = (int)((GIF_FRAMERATE / 60.0f) * 100);
      msf_gif_frame(&gifState, img.data, centiseconds, 16, img.width * 4);
      gifFrameCounter = 0;
      UnloadImage(img);
    }
  }

  // -------------------------------------------------------------------------
  // Cleanup.
  // -------------------------------------------------------------------------
  free(edgeVerts);
  free(instancePos);
  // gvizGraphRelease(&mesh);
  rlUnloadVertexArray(graphVAO);
  rlUnloadVertexBuffer(graphVBO);
  rlUnloadVertexArray(circleVAO);
  rlUnloadVertexBuffer(circleGeomVBO);
  rlUnloadVertexBuffer(circleInstVBO);
  rlUnloadShaderProgram(circleShader.id);
  CloseWindow();

  gvizGRIPEmbeddingRelease(&state);
  gvizGraphRelease(&g);
  // gvizGraphRelease(&sierpinski3d);
  // gvizGraphRelease(&mobius);
  // gvizGraphRelease(&knottedMesh);
  // gvizGraphRelease(&sierpinski);
  // gvizGraphRelease(&sierpinski);
  // gvizGraphRelease(&sierpinski3d);
  // gvizGraphRelease(&trimesh);
  return 0;
}
