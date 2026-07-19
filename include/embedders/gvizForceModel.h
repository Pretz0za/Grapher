#ifndef _GVIZ_FORCE_MODEL_H_
#define _GVIZ_FORCE_MODEL_H_

#include <stddef.h>

/** Selects which force model gvizForceModelGet returns. */
typedef enum gvizForceModelKind {
  GVIZ_FORCE_MODEL_FRUCHTERMAN_REINGOLD = 0,
  GVIZ_FORCE_MODEL_LINLOG = 1,
} gvizForceModelKind;

/**
 * A pluggable force computation strategy for gvizForceEmbedder: the
 * embedder's heat/Barnes-Hut/action machinery stays fixed, while the actual
 * force math is swapped out through this vtable.
 */
typedef struct gvizForceModel {
  /** Maps a vertex's raw graph degree to the mass used both for this
   *  vertex's own repulsion and for quadtree mass aggregation. */
  double (*vertexMass)(size_t degree);
  /** Accumulates the attractive force between @p vPos and @p uPos (a real
   *  graph edge) into @p acc. @p edgeLength is the model's target edge
   *  length; models that don't use it may ignore it. */
  void (*attractive)(int n, double *vPos, double *uPos, double edgeLength,
                     double *acc);
  /** Accumulates the repulsive force @p vPos feels from @p otherPos (either
   *  another vertex or a quadtree pseudo-body) into @p acc. @p vMass and
   *  @p otherMass are the respective masses per vertexMass (or, for a
   *  pseudo-body, the aggregated quadtree node mass); @p vRadius and
   *  @p otherRadius are the vertices' rendered radii (0 for a pseudo-body,
   *  which has no single radius of its own, or whenever overlap prevention
   *  is disabled); @p overlapConstant is Gephi ForceAtlas2's "Prevent
   *  Overlap" constant -- the flat magnitude multiplier repulsion saturates
   *  at once the two circles touch or overlap, instead of diverging;
   *  @p edgeLength is the model's target edge length, ignored by models that
   *  don't use it. */
  void (*repulsive)(int n, double *vPos, double *otherPos, double vMass,
                    double otherMass, double vRadius, double otherRadius,
                    double overlapConstant, double edgeLength, double *acc);
} gvizForceModel;

/** Returns the static force model for @p kind. Never NULL for a valid kind. */
const gvizForceModel *gvizForceModelGet(gvizForceModelKind kind);

#endif
