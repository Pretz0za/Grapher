#ifndef GVIZ_PLANAR_HPP
#define GVIZ_PLANAR_HPP

#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "Graph.hpp"
#include "Subgraph.hpp"

#include <cstddef>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace gviz::layout {

// ============================================================================
// ROTATION-SYSTEM DART QUERIES
// ============================================================================
//
// A "dart" (half-edge) is a directed (tail, head) pair; CCW order among a
// vertex's darts is exactly that vertex's adjacency-list order once a
// rotation system has been installed (see ApplyPlanarRotation/Planar below).

/** A directed dart in a CCW rotation system: tail @p u, head @p v. */
struct HalfEdge {
  size_t u;
  size_t v;

  bool operator==(const HalfEdge &) const noexcept = default;
};

/** Returns the CCW predecessor of @p v in @p u's adjacency list. Unchecked:
 *  @p v must be a neighbor of @p u. */
size_t PrevNeighborCCW(const Graph &g, size_t u, size_t v);

/** Returns the CCW successor of @p v in @p u's adjacency list. Same
 *  unchecked contract as PrevNeighborCCW. */
size_t NextNeighborCCW(const Graph &g, size_t u, size_t v);

/** Returns the reverse dart @p v -> @p u. */
inline HalfEdge HalfEdgeTwin(HalfEdge e) noexcept { return HalfEdge{e.v, e.u}; }

/** Returns the next dart around the face to the left of @p e, walking @p g's
 *  CCW rotation system. Unchecked: @p e's endpoints must be adjacent in
 *  @p g. */
HalfEdge HalfEdgeNext(const Graph &g, HalfEdge e);

// ============================================================================
// FACE WALKING
// ============================================================================

/**
 * A single-pass, range-for-friendly walk around the face to the left of a
 * starting dart, using @p g's CCW rotation system. Dereferencing yields the
 * head vertex of the current dart; iteration stops after exactly one full
 * cycle back to the starting dart.
 */
class FaceWalk {
public:
  /**
   * Begins walking the face containing dart @p start.
   * @throws LayoutError if @p start is not an edge of @p sg.
   */
  FaceWalk(const Graph &g, const Subgraph &sg, HalfEdge start);

  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const size_t *;
    using reference = size_t;

    iterator() = default;

    size_t operator*() const noexcept { return current_.v; }

    iterator &operator++() {
      Advance();
      return *this;
    }
    iterator operator++(int) {
      iterator tmp = *this;
      Advance();
      return tmp;
    }

    bool operator==(const iterator &other) const noexcept { return done_ == other.done_; }
    bool operator!=(const iterator &other) const noexcept { return !(*this == other); }

  private:
    friend class FaceWalk;
    void Advance();

    const Graph *g_ = nullptr;
    HalfEdge start_{};
    HalfEdge current_{};
    bool done_ = true;
  };

  iterator begin() const;
  iterator end() const noexcept { return iterator(); }

private:
  const Graph &g_;
  HalfEdge start_;
};

// ============================================================================
// FACE ENUMERATION AND TRIANGULATION
// ============================================================================

/** All combinatorial faces of @p sg's CCW rotation system, each a
 *  CCW-ordered vertex cycle, enumerated once at construction time. */
class FaceEnumerator {
public:
  /** @p sg's parent graph must already carry a CCW rotation system (see
   *  ApplyPlanarRotation/Planar). */
  FaceEnumerator(const Graph &g, const Subgraph &sg);

  std::vector<std::vector<size_t>> &Faces() noexcept { return faces_; }
  const std::vector<std::vector<size_t>> &Faces() const noexcept { return faces_; }

  /** Total directed darts considered when this was (most recently) counted:
   *  V - DartCount()/2 + Faces().size() == 2 is Euler's formula for a
   *  connected planar embedding. Triangulate() keeps this current. */
  size_t DartCount() const noexcept { return dartCount_; }

  /** Adds @p n to the dart count. Used by Triangulate to keep this in sync
   *  as it inserts edges. */
  void AddDarts(size_t n) noexcept { dartCount_ += n; }

private:
  std::vector<std::vector<size_t>> faces_;
  size_t dartCount_ = 0;
};

/**
 * Triangulates the planar graph @p g under @p sg by adding edges across
 * non-triangular faces in @p faces, updating @p faces' contents and dart
 * count as it goes. @p sg must be a full subgraph; it is re-derived as the
 * full subgraph of the augmented graph afterward.
 */
void Triangulate(Graph &g, Subgraph &sg, FaceEnumerator &faces);

// ============================================================================
// PLANAR ROTATION APPLICATION (the core Boyer-Myrvold wrapper)
// ============================================================================

/**
 * Thrown instead of plain NotPlanarError by ApplyPlanarRotation/Planar's
 * constructor when a Kuratowski subdivision witness was requested and
 * computed. Callers that only need "was it planar" can catch the base
 * `NotPlanarError&` and ignore the rest.
 */
class PlanarNotPlanarError : public NotPlanarError {
public:
  PlanarNotPlanarError(Graph witness, const std::string &what = "graph is not planar")
      : NotPlanarError(what), kuratowskiSubdivision(std::move(witness)) {}

  /** A Kuratowski (K5 or K3,3) subdivision found within the input, as
   *  evidence of why it is not planar. Same vertex ids as the graph that
   *  was tested. */
  Graph kuratowskiSubdivision;
};

/**
 * Tests planarity of the vertices/edges in @p subgraph (Boyer-Myrvold) and,
 * if planar, rewrites @p g's adjacency lists so that @p subgraph's
 * neighbors of each of its vertices appear consecutively in CCW order (any
 * neighbors outside @p subgraph are preserved after that block); rebuilds
 * @p g's layout and @p subgraph in place.
 *
 * @param captureWitness  When true (the default) and the graph turns out
 *   non-planar, a Kuratowski subdivision witness is computed and attached
 *   to the thrown PlanarNotPlanarError; pass false to skip that (cheaper)
 *   when the caller only cares about the planar/non-planar outcome.
 *
 * @throws PlanarNotPlanarError (carrying the witness) if not planar and
 *   @p captureWitness is true; plain NotPlanarError if not planar and
 *   @p captureWitness is false.
 * @throws LayoutError on an internal Boyer-Myrvold failure unrelated to
 *   planarity.
 */
void ApplyPlanarRotation(Graph &g, Subgraph &subgraph, bool captureWitness = true);

// ============================================================================
// THE PLANAR EMBEDDER
// ============================================================================

/**
 * Planar straight-line embedder. Construction tests planarity and installs
 * a CCW rotation system (throwing PlanarNotPlanarError on failure, carrying
 * a Kuratowski witness -- see ApplyPlanarRotation); Embed() triangulates and
 * runs a Schnyder-wood straight-line embedding (see SchnyderWood.hpp,
 * including its IMPLEMENTATION STATUS note -- Embed() inherits that
 * caveat).
 *
 * Always embeds the whole graph: a plain `Graph&`, no Subgraph on the
 * public API. Internally, ApplyPlanarRotation/FaceEnumerator/Triangulate
 * still need a Subgraph, so Planar's own .cpp builds a throwaway
 * `Subgraph::CreateFull(g)` as a private implementation detail.
 */
class Planar : public EmbeddedGraph {
public:
  /**
   * @throws PlanarNotPlanarError if @p g is not planar (see
   *         ApplyPlanarRotation).
   * @throws LayoutError on an internal Boyer-Myrvold failure.
   */
  explicit Planar(Graph &g);

  /** Computes a straight-line embedding (2D) for the planar graph. See the
   *  class doc's SchnyderWood caveat. */
  void Embed();

private:
  static size_t ValidateAndRotate(Graph &g);

  Graph &graph_;
};

// ============================================================================
// PLANAR QUERIES ON ANY EMBEDDED GRAPH
// ============================================================================
//
// These operate on any EmbeddedGraph, as long as it has a rotation system
// installed (IsPlanarEmbedded()).

/**
 * Builds a full subgraph (the face's vertices, plus the boundary edges
 * connecting consecutive vertices in the cycle) for @p face -- a CCW vertex
 * cycle as produced by FaceEnumerator. @p g's layout is built first if it
 * doesn't have one yet.
 */
Subgraph FaceSubgraph(const Graph &g, const std::vector<size_t> &face);

/**
 * Finds the combinatorial face whose interior contains (@p worldX,
 * @p worldY) in @p embedding's current straight-line drawing, or, when the
 * point falls outside every face's bounding region, the largest face by
 * area, and returns a full subgraph describing it.
 *
 * @return std::nullopt if @p embedding is not planar-embedded or no face is
 *         found.
 */
std::optional<Subgraph> FaceSubgraphAt(const Graph &g, const EmbeddedGraph &embedding,
                                        double worldX, double worldY);

/**
 * The vertex cycle of @p sg's largest combinatorial face (by vertex count).
 * @return an empty vector if @p sg has no face (fewer than 3 vertices, or no
 *         faces at all).
 */
std::vector<size_t> LargestFaceBoundary(const Graph &g, const Subgraph &sg);

} // namespace gviz::layout

#endif
