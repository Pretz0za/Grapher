#ifndef _GVIZ_UTILS_PCA_H_
#define _GVIZ_UTILS_PCA_H_

#include <stddef.h>

/*
 * Project @p n points from @p fromDim down to @p toDim using PCA
 * (eigendecomposition of the centered covariance, top @p toDim
 * eigenvectors taken as projection axes).
 *
 *   positions: row-major double[n * fromDim], read-only.
 *   out:       caller-allocated double[n * toDim], filled with projection.
 *
 * Requires @p toDim < @p fromDim and @p n > 0. Returns 0 on success,
 * -1 on allocation failure or LAPACK error. If @p toDim >= @p fromDim,
 * behavior is undefined — caller must guard.
 */
int gvizPCAProject(const double *positions, size_t n,
                   size_t fromDim, size_t toDim, double *out);

#endif
