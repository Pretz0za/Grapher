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
 * Used by gvizForceEmbedder's Fruchterman-Reingold force model.
 */
void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos, double k,
                            double *acc);

/**
 * Accumulates the Fruchterman-Reingold repulsive force between @p vPos and
 * @p uPos into @p acc, as described in the original FR paper: f_r(d) = k^2/d.
 * @p radiusSum (0 to disable) is subtracted from @p vPos/@p uPos's center
 * distance, so the force acts on the surface-to-surface gap between the two
 * vertices' rendered radii rather than on their center distance. Once that
 * gap reaches 0 (the circles touch or overlap), the magnitude stops growing
 * and holds flat at k^2 * @p overlapConstant instead of diverging (Gephi
 * ForceAtlas2's "Prevent Overlap" constant), keeping the force bounded by
 * construction. Used by gvizForceEmbedder's Fruchterman-Reingold force model
 * (both directly and, via gvizPairwiseFRRepForceWeighted, for its Barnes-Hut
 * pseudo-body case).
 */
void gvizPairwiseFRRepForce(int n, double *vPos, double *uPos, double k,
                            double radiusSum, double overlapConstant,
                            double *acc);

/**
 * Accumulates the Fruchterman-Reingold repulsive force @p vPos would feel
 * from @p mass bodies collapsed at their shared center of mass @p comPos,
 * approximating their combined repulsion as a single pseudo-body of that
 * mass instead of visiting each body individually. This is the Barnes-Hut
 * approximation used by gvizForceEmbedder when a quadtree node is far enough
 * away (per its opening-angle test) to be treated as one point. @p radiusSum
 * is @p vPos's own radius only (0 to disable): a pseudo-body has no single
 * radius of its own, and the opening-angle test already guarantees it's far
 * enough away that overlap with any individual body it aggregates is moot.
 * @p overlapConstant is forwarded to gvizPairwiseFRRepForce (see there).
 */
void gvizPairwiseFRRepForceWeighted(int n, double *vPos, double *comPos,
                                    size_t mass, double k, double radiusSum,
                                    double overlapConstant, double *acc);

/**
 * Accumulates the LinLog attractive force between @p vPos and @p uPos into
 * @p acc: magnitude log(1 + dist), pulling v toward u.
 */
void gvizPairwiseLinLogAttForce(int n, double *vPos, double *uPos,
                                double *acc);

/**
 * Accumulates the LinLog repulsive force between @p vPos and @p uPos into
 * @p acc: magnitude (vMass*otherMass)/dist, pushing v away from u. @p
 * radiusSum (0 to disable) is subtracted from the center distance, so the
 * force acts on the surface-to-surface gap between the two vertices' rendered
 * radii rather than on their center distance. Once that gap reaches 0 (the
 * circles touch or overlap), the magnitude stops growing and holds flat at
 * (vMass*otherMass) * @p overlapConstant instead of diverging (Gephi
 * ForceAtlas2's "Prevent Overlap" constant), keeping the force bounded by
 * construction.
 */
void gvizPairwiseLinLogRepForce(int n, double *vPos, double *uPos,
                                double vMass, double otherMass,
                                double radiusSum, double overlapConstant,
                                double *acc);

/**
 * Accumulates a constant-magnitude gravitational force into @p acc, pulling
 * @p vPos toward the origin.
 */
void gvizPairwiseGravityForce(int n, double *vPos, double magnitude,
                              double *acc);

#endif
