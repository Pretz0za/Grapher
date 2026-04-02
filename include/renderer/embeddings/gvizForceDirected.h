#ifndef _GVIZ_FORCE_DIRECTED_H_
#define _GVIZ_FORCE_DIRECTED_H_

#include <stddef.h>

// Kamada Kawai Force Vector calculation
void gvizPairwiseKKForce(int n, double *uPos, double *vPos, size_t gDist,
                         double edgeLength, double *acc);

// Fruchterman Reingold Attraction Force Vector calculation
void gvizPairwiseFRAttForce(int n, double *uPos, double *vPos,
                            double edgeLength, double *acc);

// Fruchterman Reingold Repulsive Force Vector calculation
void gvizPairwiseFRRepForce(int n, double *uPos, double *vPos,
                            double edgeLength, double *acc);

#endif
