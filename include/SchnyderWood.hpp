#ifndef GVIZ_SCHNYDER_WOOD_HPP
#define GVIZ_SCHNYDER_WOOD_HPP

#include "EmbeddedGraph.hpp"
#include "Graph.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * A Schnyder wood (realizer) for a triangulated planar graph.
 *
 * Given a triangulated planar graph G with a designated outer-face triangle
 * (Root(0), Root(1), Root(2)) and a valid CCW rotation system (e.g. one
 * installed by gviz::layout::Planar), a Schnyder wood decomposes every edge
 * except the base edge Root(0)-Root(1) into three directed trees T0, T1,
 * T2:
 *
 *   Ti is rooted at Root(i).
 *   Parent(i, v) is v's parent in Ti, or kNone when v has no parent in Ti
 *   (v == Root(i), or the edge would be the excluded base edge).
 *
 * Properties:
 *   - Every interior vertex (not one of the three roots) has exactly one
 *     outgoing edge in each of T0, T1, and T2.
 *   - Root(2) has Parent(0, Root(2)) == Root(0) and
 *     Parent(1, Root(2)) == Root(1) (the two non-base outer-triangle edges).
 *   - Root(0) and Root(1) have kNone for every tree parent (the base edge
 *     Root(0)-Root(1) is excluded from every tree).
 *
 * IMPLEMENTATION STATUS -- read before relying on Embed():
 *
 * The constructor implements the standard canonical-ordering construction
 * and is believed correct (every root's own-tree parent is kNone, every
 * interior vertex has exactly one parent per tree, every parent pointer is
 * a real graph edge, and following any chain terminates at the correct
 * root).
 *
 * Embed(), however, is a known-INCOMPLETE algorithm: its region-counting
 * logic has an unresolved correctness gap ("finding previous neighbor is
 * not enough"). Do not treat Embed()'s output as a validated straight-line
 * planar drawing; SchnyderWoodTests.cpp only checks that it runs and
 * produces finite coordinates.
 */
class SchnyderWood {
public:
  /** Sentinel meaning "no parent" (root vertex, or edge excluded from this
   *  tree). */
  static constexpr size_t kNone = static_cast<size_t>(-1);

  /**
   * Constructs a Schnyder wood for a triangulated planar graph @p g, which
   * must carry a valid CCW rotation system (e.g. installed by
   * gviz::layout::Planar) where every face is a triangle. The outer-face
   * triangle is inferred automatically: vertex 0 is Root(0), its first
   * adjacency-list neighbor is Root(1), and the third vertex of the face
   * traced by the dart Root(0)->Root(1) is Root(2). @p g is not copied --
   * it must outlive this object (Embed() reads it again).
   *
   * @throws LayoutError if @p g has fewer than 3 vertices, vertex 0 has no
   *         neighbors, or (defensively) the canonical-ordering scan can't
   *         find a next vertex to process -- all indicate @p g is not a
   *         validly triangulated planar rotation system.
   */
  explicit SchnyderWood(const Graph &g);

  /** Number of vertices (same as the input graph). */
  size_t Size() const noexcept { return n_; }

  /** Root vertex of tree Ti, for tree in [0, 3). Unchecked. */
  size_t Root(size_t tree) const noexcept { return root_[tree]; }

  /** Parent of @p v in tree Ti, or kNone. Unchecked: @p tree in [0, 3),
   *  @p v in [0, Size()). */
  size_t Parent(size_t tree, size_t v) const noexcept { return parent_[tree][v]; }

  /**
   * Computes a straight-line embedding from this realizer into
   * @p embedding (2D only). Reads the graph captured at construction.
   *
   * @throws DimensionError if @p embedding is not 2-dimensional.
   */
  void Embed(EmbeddedGraph &embedding) const;

private:
  const Graph &g_;
  size_t n_ = 0;
  std::array<size_t, 3> root_{};
  std::array<std::vector<size_t>, 3> parent_;
};

} // namespace gviz::layout

#endif
