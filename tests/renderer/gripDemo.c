#include "cblas.h"
#include "dsa/gvizGraph.h"
#include "lapack.h"
#include "raylib.h"
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
  size_t DEPTH = 8;

  printf("initializing state\n");
  gvizGRIPState state;
  gvizGraph klein = build_klein_bottle(WIDTH, HEIGHT);
  // gvizGraph mobius = build_mobius_strip(WIDTH, HEIGHT);
  // gvizGraph knottedMesh = build_knotted_rect_mesh(WIDTH, HEIGHT);
  // gvizGraph sierpinski3d = createSierpinskiTetrahedron(DEPTH, NULL);
  // gvizGraph mesh = build_rect_mesh(WIDTH, HEIGHT);
  // gvizGraph sierpinski = createSierpinski(DEPTH, NULL);
  // gvizGraph trimesh = build_equilateral_tri_mesh(DEPTH);
  // gvizGraph carpet = build_sierpinski_carpet(DEPTH);
  // gvizGRIPEmbeddingInit(&state, &mesh, (WIDTH - 1) + (HEIGHT - 1) + 10, 2);
  // gvizGRIPEmbeddingInit(&state, &carpet, pow(2, DEPTH), 2);
  // gvizGRIPEmbeddingInit(&state, &trimesh, DEPTH, 2);
  // gvizGRIPEmbeddingInit(&state, &carpet, pow(4, DEPTH), 2);
  // gvizGRIPEmbeddingInit(&state, &mobius, pow(2, DEPTH + 1), 4);
  gvizGRIPEmbeddingInit(&state, &klein, pow(2, DEPTH + 1), 4);

  printf("creating filtration\n");
  gvizGRIPEmbeddingEmbed(&state);

  projectPCA(state.graph.embedding.vertexPositions, klein.vertices.count);
  state.graph.embedding.dim = 3;

  Vector3 centroid = {0};
  if (state.graph.embedding.dim == 3) {
    size_t N = state.graph.graph->vertices.count;
    for (size_t i = 0; i < N; i++) {
      double *pos =
          gvizEmbeddedGraphGetVPosition((gvizEmbeddedGraph *)&state, i);
      centroid.x += pos[0];
      centroid.y += pos[1];
      centroid.z += pos[2];
    }
    centroid.x /= N;
    centroid.y /= N;
    centroid.z /= N;
  }

  InitWindow(1000, 1000, "graphvis");
  SetTargetFPS(60);

  Camera3D camera = {.position = (Vector3){0, 0, 5000},
                     .target = (Vector3){0, 0, 0},
                     .up = (Vector3){0, 1, 0},
                     .fovy = 45.0f,
                     .projection = CAMERA_PERSPECTIVE};
  // Camera2D camera = {(Vector2){0, 0}, (Vector2){0, 0}, 0, 1.0f};

  gvizLayerGraph layer;
  gvizViewport viewport = {0, 0, 800, 600};
  gvizLayerGraphInit(&layer, (gvizEmbeddedGraph *)&state, viewport, 999);

  unsigned char currOpacity = 0xFF;

  while (!WindowShouldClose()) {

    gvizRenderer3DCameraUpdate(&camera, centroid);
    // gvizRenderer2DCameraUpdate(&camera);

    BeginDrawing();

    {
      ClearBackground(RAYWHITE);

      rlSetClipPlanes(0.1, 1000000.0); // near, far
      BeginMode3D(camera);
      // BeginMode2D(camera);

      // draw

      ((gvizLayer *)&layer)->vtable->draw(&layer, &camera);

      EndMode3D();
      // EndMode2D();
      DrawText("Hello World", 0, 0, 20, GREEN);
      currOpacity = 0xFF;
    }

    EndDrawing();
  }

  // gvizGraphRelease(&mesh);
  CloseWindow();
  gvizGRIPEmbeddingRelease(&state);
  gvizGraphRelease(&klein);
  // gvizGraphRelease(&mobius);
  // gvizGraphRelease(&knottedMesh);
  // gvizGraphRelease(&carpet);
  // gvizGraphRelease(&sierpinski);
  // gvizGraphRelease(&sierpinski3d);
  // gvizGraphRelease(&trimesh);
  return 0;
}
