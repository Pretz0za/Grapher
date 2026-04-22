#include "cblas.h"
#include "dsa/gvizGraph.h"
#include "lapack.h"
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

int main() {

  size_t WIDTH = 400, HEIGHT = 100;
  size_t DEPTH = 6;

  printf("initializing state\n");
  gvizGRIPState state;
  gvizGraph g;
  // g = build_tetrahedral_mesh(DEPTH);
  // gvizGraph klein = build_klein_bottle(WIDTH, HEIGHT);
  // gvizGraph mobius = build_mobius_strip(WIDTH, HEIGHT);
  // gvizGraph knottedMesh = build_knotted_rect_mesh(WIDTH, HEIGHT);
  // g = createSierpinskiTetrahedron(DEPTH, NULL);
  // gvizGraph mesh = build_rect_mesh(WIDTH, HEIGHT);
  // gvizGraph sierpinski = createSierpinski(DEPTH, NULL);
  // gvizGraph trimesh = build_equilateral_tri_mesh(DEPTH);
  g = build_sierpinski_carpet(DEPTH);
  // gvizGRIPEmbeddingInit(&state, &mesh, (WIDTH - 1) + (HEIGHT - 1) + 10, 2);
  gvizGRIPEmbeddingInit(&state, &g, pow(2, DEPTH), 2);
  // gvizGRIPEmbeddingInit(&state, &g, DEPTH, 2); // trimesh 2d
  // gvizGRIPEmbeddingInit(&state, &g, DEPTH, 3); // trimesh 3d
  // gvizGRIPEmbeddingInit(&state, &g, pow(4, DEPTH), 2);
  // gvizGRIPEmbeddingInit(&state, &mobius, pow(2, DEPTH + 1), 4);
  // gvizGRIPEmbeddingInit(&state, &g, pow(2, DEPTH + 1), 3); // 3d sierpinski

  printf("creating filtration\n");
  gvizGRIPEmbeddingEmbed(&state);
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)&state;

  // assert(!gvizEmbeddedGraphLoadEmbedding(embedding, "3d_sierpinski_11.txt"));

  // gvizGRIPRefineEmbedding(&state);

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

  // Build edge vertex buffer once before the render loop.
  // Each edge becomes two 3-float vertices (line endpoint pair).
  size_t N = embedding->graph->vertices.count;

  size_t edgeVertexCount = 0;
  for (size_t i = 0; i < N; i++) {
    gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, i);
    edgeVertexCount += children->count * 2;
  }

  float *edgeVerts = (float *)malloc(edgeVertexCount * 3 * sizeof(float));
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

  // Upload geometry to GPU.
  unsigned int graphVAO = rlLoadVertexArray();
  rlEnableVertexArray(graphVAO);
  unsigned int graphVBO = rlLoadVertexBuffer(
      edgeVerts, (int)(edgeVertexCount * 3 * sizeof(float)), false);
  rlSetVertexAttribute(0, 3, RL_FLOAT, 0, 0, 0);
  rlEnableVertexAttribute(0);
  rlDisableVertexArray();
  free(edgeVerts);

  int graphVertexCount = (int)edgeVertexCount;

  unsigned char currOpacity = 0xFF;

  while (!WindowShouldClose()) {

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

    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      rlSetClipPlanes(0.1, 1000000.0); // near, far
      if (is3D)
        BeginMode3D(camera3D);
      else
        BeginMode2D(camera2D);

      // draw

      // ((gvizLayer *)&layer)->vtable->draw(&layer, &camera);

      // Draw graph edges using pre-uploaded VAO/VBO in a single GL call.
      rlEnableShader(rlGetShaderIdDefault());
      float black[4] = {0.0f, 0.0f, 0.0f, 1.0f};
      int *locs = rlGetShaderLocsDefault();
      rlSetUniform(locs[RL_SHADER_LOC_COLOR_DIFFUSE], black,
                   RL_SHADER_UNIFORM_VEC4, 1);
      Matrix mvp = MatrixMultiply(
          MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixTransform()),
          rlGetMatrixProjection());
      rlSetUniformMatrix(locs[RL_SHADER_LOC_MATRIX_MVP], mvp);
      rlEnableVertexArray(graphVAO);
      glDrawArrays(GL_LINES, 0, graphVertexCount);
      rlDisableVertexArray();
      rlDisableShader();

      if (is3D)
        EndMode3D();
      else
        EndMode2D();
      // DrawText("Hello World", 0, 0, 20, GREEN);
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

  // gvizGraphRelease(&mesh);
  rlUnloadVertexArray(graphVAO);
  rlUnloadVertexBuffer(graphVBO);
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
