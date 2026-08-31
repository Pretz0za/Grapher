#ifndef GVIZ_SCHNYDER_WOOD_HPP
#define GVIZ_SCHNYDER_WOOD_HPP

#include "EmbeddedGraph.hpp"
#include "Graph.hpp"

#include <array>
#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * A Schnyder wood (realizer) for a triangulated planar graph. Port of the
 * old C gvizSchnyderWood/gvizSchnyderWoodInit/gvizSchnyderWoodEmbed.
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
 * The constructor (the old gvizSchnyderWoodInit) implements the standard
 * canonical-ordering construction and is believed correct: it is ported
 * faithfully and exercised by the same structural checks the old C test
 * suite used (every root's own-tree parent is kNone, every interior vertex
 * has exactly one parent per tree, every parent pointer is a real graph
 * edge, and following any chain terminates at the correct root).
 *
 * Embed(), however, ports a genuinely INCOMPLETE algorithm. The old
 * gvizSchnyderWoodEmbed carried a `// TODO: fix this. finding previous
 * neighbor is not enough` comment directly over its region-counting logic,
 * plus stdout debug printf()s left in from development. This port removes
 * the printf()s (dead debugging output, not intentional behavior -- keeping
 * them would make this the ONLY class in the C++ port that spams stdout on
 * every call) but preserves the region-counting algorithm exactly as
 * written, bug and all: fixing a triangulation/barycentric-coordinate
 * algorithm is out of scope for a mechanical port, and guessing at a fix
 * risks silently shipping a DIFFERENT wrong answer instead of a faithfully
 * reproduced one. Do not treat Embed()'s output as a validated straight-line
 * planar drawing; SchnyderWoodTests.cpp only checks that it runs and
 * produces finite coordinates, exactly as weak as the old C test's coverage
 * (which printed the result for a human to eyeball rather than asserting
 * anything about it).
 */
class SchnyderWood {
public:
  /** Sentinel meaning "no parent" (root vertex, or edge excluded from this
   *  tree). Replaces the old GVIZ_SW_NONE macro. */
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
   *         validly triangulated planar rotation system. The old C asserted
   *         the last condition (UB/no-op in a release build on failure);
   *         this throws instead of leaving a partially-built, silently
   *         invalid object.
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
   * @p embedding (2D only -- matches the old C signature, which took just
   * the embedding since it could reach the graph via
   * `embedding->subgraph.g`; this class instead keeps its own reference to
   * the graph it was built from, captured at construction, so the call
   * shape here matches the C one exactly despite Subgraph never exposing an
   * equivalent accessor -- see Planar.hpp's file-level note for the fuller
   * explanation of why that accessor doesn't exist).
   *
   * @throws DimensionError if @p embedding is not 2-dimensional. The old C
   *         had no such check and would silently over-read its 3-element
   *         stack coordinate buffer for any embedding with Dim() > 3; this
   *         is a genuine safety fix, not a behavior change for any correct
   *         (2D) caller.
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
