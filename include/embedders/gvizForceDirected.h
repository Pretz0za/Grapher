#ifndef _GVIZ_FORCE_DIRECTED_H_
#define _GVIZ_FORCE_DIRECTED_H_

#include <stddef.h>

#define gvizNumericEpsilon 1e-7

/**
 * Accumulates the Kamada-Kawai spring force between @p vPos and @p uPos into @p acc.
 * @p gDist is graph distance; @p edgeLength is the target edge length.
 */
void gvizPairwiseKKForce(int n, double *vPos, double *uPos, size_t gDist,
                         double edgeLength, double *acc);

/**
 * Accumulates the Fruchterman-Reingold attractive force between @p vPos and @p uPos
 * into @p acc.
 */
void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos,
                            double edgeLength, double *acc);

/**
 * Accumulates the Fruchterman-Reingold repulsive force between @p vPos and @p uPos
 * into @p acc.
 */
void gvizPairwiseFRRepForce(int n, double *vPos, double *uPos,
                            double edgeLength, double *acc);

#endif
