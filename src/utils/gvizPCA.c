#include "utils/gvizPCA.h"
#include "cblas.h"
#include "core/alloc.h"
#include <stdlib.h>
#include <string.h>

extern void dsyev_(const char *jobz, const char *uplo, const int *n,
                   double *a, const int *lda, double *w,
                   double *work, const int *lwork, int *info);

int gvizPCAProject(const double *positions, size_t n,
                   size_t fromDim, size_t toDim, double *out) {
  if (!positions || !out || n == 0 || toDim == 0 || toDim >= fromDim)
    return -1;

  double *centered = (double *)GVIZ_ALLOC(n * fromDim * sizeof(double));
  double *cov      = (double *)GVIZ_ALLOC(fromDim * fromDim * sizeof(double));
  double *eigvals  = (double *)GVIZ_ALLOC(fromDim * sizeof(double));
  if (!centered || !cov || !eigvals) {
    if (centered) GVIZ_DEALLOC(centered);
    if (cov)      GVIZ_DEALLOC(cov);
    if (eigvals)  GVIZ_DEALLOC(eigvals);
    return -1;
  }

  memcpy(centered, positions, n * fromDim * sizeof(double));

  for (size_t d = 0; d < fromDim; d++) {
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += centered[i * fromDim + d];
    double mean = sum / (double)n;
    for (size_t i = 0; i < n; i++) centered[i * fromDim + d] -= mean;
  }

  memset(cov, 0, fromDim * fromDim * sizeof(double));
  cblas_dsyrk(CblasRowMajor, CblasUpper, CblasTrans,
              (int)fromDim, (int)n, 1.0 / (double)n,
              centered, (int)fromDim, 0.0, cov, (int)fromDim);

  for (size_t i = 0; i < fromDim; i++)
    for (size_t j = 0; j < i; j++)
      cov[i * fromDim + j] = cov[j * fromDim + i];

  int nDim = (int)fromDim;
  int info = 0;
  int lwork = -1;
  double wopt = 0.0;
  dsyev_("V", "U", &nDim, cov, &nDim, eigvals, &wopt, &lwork, &info);
  if (info != 0) {
    GVIZ_DEALLOC(centered); GVIZ_DEALLOC(cov); GVIZ_DEALLOC(eigvals);
    return -1;
  }
  lwork = (int)wopt;
  double *work = (double *)GVIZ_ALLOC((size_t)lwork * sizeof(double));
  if (!work) {
    GVIZ_DEALLOC(centered); GVIZ_DEALLOC(cov); GVIZ_DEALLOC(eigvals);
    return -1;
  }

  dsyev_("V", "U", &nDim, cov, &nDim, eigvals, work, &lwork, &info);
  GVIZ_DEALLOC(work);
  if (info != 0) {
    GVIZ_DEALLOC(centered); GVIZ_DEALLOC(cov); GVIZ_DEALLOC(eigvals);
    return -1;
  }

  /* dsyev with row-major input is treated as column-major by Fortran, so the
   * eigenvectors are stored as columns of the (column-major) matrix — which,
   * when interpreted as row-major, are the rows. Eigenvalues come ascending,
   * so the top toDim live in rows [fromDim - toDim .. fromDim). */
  double *proj = (double *)GVIZ_ALLOC(fromDim * toDim * sizeof(double));
  if (!proj) {
    GVIZ_DEALLOC(centered); GVIZ_DEALLOC(cov); GVIZ_DEALLOC(eigvals);
    return -1;
  }
  for (size_t r = 0; r < fromDim; r++) {
    for (size_t c = 0; c < toDim; c++) {
      size_t srcRow = fromDim - 1 - c;
      proj[r * toDim + c] = cov[srcRow * fromDim + r];
    }
  }

  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
              (int)n, (int)toDim, (int)fromDim,
              1.0, centered, (int)fromDim, proj, (int)toDim,
              0.0, out, (int)toDim);

  GVIZ_DEALLOC(centered);
  GVIZ_DEALLOC(cov);
  GVIZ_DEALLOC(eigvals);
  GVIZ_DEALLOC(proj);
  return 0;
}
