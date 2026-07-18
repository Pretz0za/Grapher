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
 * Accumulates GRIP's variant of the Fruchterman-Reingold "attractive" force
 * between @p vPos and @p uPos into @p acc. Not the attractive force from the
 * original FR paper: this one decays with distance (see gvizPairwiseFRAttForce
 * for the paper-accurate version). GRIP applies this to k-nearest non-edge
 * vertices.
 */
void gvizPairwiseGRIPFRAttForce(int n, double *vPos, double *uPos,
                                double edgeLength, double *acc);

/**
 * Accumulates GRIP's variant of the Fruchterman-Reingold "repulsive" force
 * between @p vPos and @p uPos into @p acc. Not the repulsive force from the
 * original FR paper: this one grows with distance (see gvizPairwiseFRRepForce
 * for the paper-accurate version). GRIP applies this along graph edges.
 */
void gvizPairwiseGRIPFRRepForce(int n, double *vPos, double *uPos,
                                double edgeLength, double *acc);

/**
 * Accumulates the Fruchterman-Reingold attractive force between @p vPos and
 * @p uPos into @p acc, as described in the original FR paper: f_a(d) = d^2/k.
 * Used by gvizFRPairwiseEmbedder.
 */
void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos, double k,
                            double *acc);

/**
 * Accumulates the Fruchterman-Reingold repulsive force between @p vPos and
 * @p uPos into @p acc, as described in the original FR paper: f_r(d) = k^2/d.
 * Used by gvizFRPairwiseEmbedder.
 */
void gvizPairwiseFRRepForce(int n, double *vPos, double *uPos, double k,
                            double *acc);

/**
 * Accumulates the Fruchterman-Reingold repulsive force @p vPos would feel
 * from @p mass bodies collapsed at their shared center of mass @p comPos,
 * approximating their combined repulsion as a single pseudo-body of that
 * mass instead of visiting each body individually. This is the Barnes-Hut
 * approximation used by gvizForceEmbedder when a quadtree node is far enough
 * away (per its opening-angle test) to be treated as one point.
 */
void gvizPairwiseFRRepForceWeighted(int n, double *vPos, double *comPos,
                                    size_t mass, double k, double *acc);

#endif
