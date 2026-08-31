#ifndef GVIZ_FORCEMODEL_HPP
#define GVIZ_FORCEMODEL_HPP

#include "Vec.hpp"

#include <cstddef>

namespace gviz::layout {

/**
 * A pluggable force computation strategy for force-directed layout
 * (ForceAtlas, a later port milestone): the embedder's heat/Barnes-Hut/
 * action machinery stays fixed, while the actual force math is swapped out
 * through this base class. Direct port of the old gvizForceModel
 * function-pointer vtable struct (gvizForceModel.h) -- a real class
 * hierarchy replaces the table, and the two concrete models below
 * (FruchtermanReingold, LinLog) replace its two static singleton instances.
 *
 * Design note on the old gvizForceModelGet(kind) accessor: it isn't ported
 * as a factory function here. Both models are stateless (no per-instance
 * data at all -- every method only reads its arguments), so there is
 * nothing an enum-keyed lookup buys over just constructing the concrete
 * type directly. ForceAtlas is expected to hold a
 * std::unique_ptr<ForceModel> and initialize it at its own construction
 * time with std::make_unique<FruchtermanReingold>() or
 * std::make_unique<LinLog>() depending on which model it was asked for --
 * ordinary polymorphism, no extra indirection through a kind enum needed.
 *
 * Every method is const: neither concrete model carries state, so one
 * instance can be shared (by reference or through a single owning
 * unique_ptr) across every vertex/pair in a round.
 */
class ForceModel {
public:
  virtual ~ForceModel() = default;

  /** Maps a vertex's raw graph degree to the mass used both for this
   *  vertex's own repulsion and for quadtree mass aggregation. */
  virtual double VertexMass(size_t degree) const = 0;

  /** Accumulates the attractive force between @p vPos and @p uPos (a real
   *  graph edge; both n-dimensional points) into @p acc. @p edgeLength is
   *  the model's target edge length; models that don't use it may ignore
   *  it. */
  virtual void Attractive(size_t n, const double *vPos, const double *uPos,
                          double edgeLength, double *acc) const = 0;

  /** Accumulates the repulsive force @p vPos feels from @p otherPos (either
   *  another vertex or a quadtree pseudo-body) into @p acc. @p vMass and
   *  @p otherMass are the respective masses per VertexMass (or, for a
   *  pseudo-body, the aggregated quadtree node mass); @p vRadius and
   *  @p otherRadius are the vertices' rendered radii (0 for a pseudo-body,
   *  which has no single radius of its own, or whenever overlap prevention
   *  is disabled); @p overlapConstant is Gephi ForceAtlas2's "Prevent
   *  Overlap" constant -- the flat magnitude multiplier repulsion saturates
   *  at once the two circles touch or overlap, instead of diverging;
   *  @p edgeLength is the model's target edge length, ignored by models
   *  that don't use it. */
  virtual void Repulsive(size_t n, const double *vPos, const double *otherPos,
                         double vMass, double otherMass, double vRadius,
                         double otherRadius, double overlapConstant,
                         double edgeLength, double *acc) const = 0;
};

/**
 * Classic Fruchterman-Reingold. Vertex mass is a constant 1.0 (FR has no
 * notion of degree-weighted mass); attraction grows as dist^2/edgeLength
 * along real edges (VecAccFRAttForce); repulsion is k^2/dist between every
 * pair, expressed through VecAccFRRepForceWeighted so the same code path
 * covers both an ordinary vertex-vertex pair (weight 1) and a Barnes-Hut
 * vertex-vs-pseudo-body pair (weight = aggregated quadtree node mass).
 */
class FruchtermanReingold final : public ForceModel {
public:
  double VertexMass(size_t degree) const override {
    (void)degree;
    return 1.0;
  }

  void Attractive(size_t n, const double *vPos, const double *uPos,
                  double edgeLength, double *acc) const override {
    VecAccFRAttForce(n, vPos, uPos, edgeLength, acc);
  }

  // vMass is intentionally ignored here, exactly as the old C
  // gvizForceModel's frRepulsive did: FR repulsion is expressed from the
  // "other" side's mass only (VecAccFRRepForceWeighted's weight parameter).
  // That's correct whether "other" is an ordinary vertex (mass 1.0, so this
  // reduces to the unweighted paper formula k^2/dist) or a Barnes-Hut
  // pseudo-body (aggregated mass standing in for many bodies at once) --
  // not a bug, preserved deliberately.
  void Repulsive(size_t n, const double *vPos, const double *otherPos,
                 double vMass, double otherMass, double vRadius,
                 double otherRadius, double overlapConstant, double edgeLength,
                 double *acc) const override {
    (void)vMass;
    VecAccFRRepForceWeighted(n, vPos, otherPos, static_cast<size_t>(otherMass),
                             edgeLength, vRadius + otherRadius, overlapConstant,
                             acc);
  }
};

/**
 * LinLog. Vertex mass grows with degree (1 + degree, so high-degree hubs
 * repel more strongly and aggregate more weight in Barnes-Hut); attraction
 * grows as log(1 + dist) and ignores edgeLength entirely
 * (VecAccLinLogAttForce); repulsion scales with the product of both sides'
 * masses over distance (VecAccLinLogRepForce). See Vec.hpp's doc comment on
 * VecAccLinLogAttForce for the growth-rate rationale.
 */
class LinLog final : public ForceModel {
public:
  double VertexMass(size_t degree) const override {
    return 1.0 + static_cast<double>(degree);
  }

  void Attractive(size_t n, const double *vPos, const double *uPos,
                  double edgeLength, double *acc) const override {
    (void)edgeLength;
    VecAccLinLogAttForce(n, vPos, uPos, acc);
  }

  void Repulsive(size_t n, const double *vPos, const double *otherPos,
                 double vMass, double otherMass, double vRadius,
                 double otherRadius, double overlapConstant, double edgeLength,
                 double *acc) const override {
    VecAccLinLogRepForce(n, vPos, otherPos, vMass, otherMass,
                         vRadius + otherRadius, overlapConstant, edgeLength,
                         acc);
  }
};

} // namespace gviz::layout

#endif
