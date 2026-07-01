#ifndef _GVIZ_FORCE_DIRECTED_H_
#define _GVIZ_FORCE_DIRECTED_H_

#include <stddef.h>

#define gvizNumericEpsilon 1e-7

// Kamada Kawai Force Vector calculation
void gvizPairwiseKKForce(int n, double *vPos, double *uPos, size_t gDist,
                         double edgeLength, double *acc);

// Fruchterman Reingold Attraction Force Vector calculation
void gvizPairwiseFRAttForce(int n, double *vPos, double *uPos,
                            double edgeLength, double *acc);

// Fruchterman Reingold Repulsive Force Vector calculation
void gvizPairwiseFRRepForce(int n, double *vPos, double *uPos,
                            double edgeLength, double *acc);

#endif
