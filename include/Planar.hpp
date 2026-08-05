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
// A NOTE ON WHY SEVERAL FUNCTIONS BELOW TAKE `Graph&`/`const Graph&`
// EXPLICITLY, SEPARATE FROM A Subgraph/EmbeddedGraph PARAMETER
// ============================================================================
//
// The old C API (gvizPlanarEmbedder.h) reached a subgraph's parent graph via
// its public `gvizSubgraph::g` field wherever it needed raw adjacency-list
// access -- which is most of this module, since a CCW rotation system IS
// adjacency-list order. Subgraph.hpp deliberately does not expose an
// equivalent `const Graph&` accessor ("Subgraph still never hands out a
// const Graph&/pointer to its parent... it gets exactly these three [scalar]
// facts and no more" -- see Subgraph.hpp's class doc), and this module has no
// standing to widen that contract by editing an already-landed, shared
// header. So instead of reaching through a Subgraph/EmbeddedGraph the way
// the C code did, every free function here that needs raw graph access takes
// the Graph explicitly -- exactly the shape gvizPlanarLargestFaceBoundary
// already used in the C API (it took both `g` and `sg` because it needed
// both), just applied consistently instead of as a one-off. Callers already
// hold the Graph (Subgraph::CreateFull/CreateEmpty/CreateVertexInduced all
// require one), so this costs nothing extra at call sites.
//
// A second consequence: Planar (and SchnyderWood) need MUTABLE graph access
// (installing a rotation system reorders adjacency lists; triangulation
// inserts edges), but Subgraph only ever stores a `const Graph&`. The old C
// papered over this with `gvizGraph *mutableGraph = (gvizGraph *)graph;` --
// a const_cast in disguise. This port avoids that cast outright: Planar
// holds its own `Graph&` member, captured directly from the caller, instead
// of trying to recover mutable access from something that was told to be
// const.

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

/**
 * Returns the CCW predecessor of @p v in @p u's adjacency list. Unchecked:
 * @p v must be a neighbor of @p u (asserted in debug builds), same
 * hot-path-unchecked contract as Graph::Neighbor.
 */
size_t PrevNeighborCCW(const Graph &g, size_t u, size_t v);

/** Returns the CCW successor of @p v in @p u's adjacency list. Same
 *  unchecked contract as PrevNeighborCCW. */
size_t NextNeighborCCW(const Graph &g, size_t u, size_t v);

/** Returns the reverse dart @p v -> @p u. */
inline HalfEdge HalfEdgeTwin(HalfEdge e) noexcept { return HalfEdge{e.v, e.u}; }

/**
 * Returns the next dart around the face to the left of @p e, walking @p g's
 * CCW rotation system. Unchecked: @p e's endpoints must be adjacent in @p g.
 */
HalfEdge HalfEdgeNext(const Graph &g, HalfEdge e);

// ============================================================================
// FACE WALKING
// ============================================================================

/**
 * A single-pass, range-for-friendly walk around the face to the left of a
 * starting dart, using @p g's CCW rotation system -- replaces the old C
 * gvizPlanarFaceWalkBegin/gvizPlanarFaceWalkStep step-function pair with a
 * real std::input_iterator so callers write `for (size_t v : FaceWalk(g, sg,
 * start))` instead of a manual do/while over a step function. Dereferencing
 * yields the head vertex of the current dart, exactly what the old
 * gvizPlanarFaceWalkStep wrote to its `outV` out-parameter; iteration stops
 * after exactly one full cycle back to the starting dart (the starting
 * dart's head is yielded once, at the beginning, never repeated at the end).
 */
class FaceWalk {
public:
  /**
   * Begins walking the face containing dart @p start.
   * @throws LayoutError if @p start is not an edge of @p sg (the routine,
   *         checkable failure the old gvizPlanarFaceWalkBegin reported via
   *         -1; construction is the natural place to reject it now).
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

/**
 * All combinatorial faces of @p sg's CCW rotation system, each a
 * CCW-ordered vertex cycle, enumerated once at construction time (mirrors
 * gvizFaceIteratorInit immediately followed by gvizPlanarEmbedderFaces --
 * there is no lazy/incremental variant in either the old code or this one,
 * so there is nothing behaviorally lost by always enumerating eagerly).
 */
class FaceEnumerator {
public:
  /** @p sg's parent graph must already carry a CCW rotation system (see
   *  ApplyPlanarRotation/Planar). */
  FaceEnumerator(const Graph &g, const Subgraph &sg);

  std::vector<std::vector<size_t>> &Faces() noexcept { return faces_; }
  const std::vector<std::vector<size_t>> &Faces() const noexcept { return faces_; }

  /** Total directed darts considered when this was (most recently) counted:
   *  V - DartCount()/2 + Faces().size() == 2 is Euler's formula for a
   *  connected planar embedding. Triangulate() keeps this current as it
   *  splits faces. */
  size_t DartCount() const noexcept { return dartCount_; }

  /** Adds @p n to the dart count. Public because Triangulate (a free
   *  function, not a member -- triangulation is a mutation of the *graph*
   *  that happens to also need to update an existing enumeration, not an
   *  operation FaceEnumerator performs on itself) needs to keep this in
   *  sync as it inserts edges. Not intended for other callers. */
  void AddDarts(size_t n) noexcept { dartCount_ += n; }

private:
  std::vector<std::vector<size_t>> faces_;
  size_t dartCount_ = 0;
};

/**
 * Triangulates the planar graph @p g under @p sg by adding edges across
 * non-triangular faces in @p faces, updating @p faces' contents and dart
 * count as it goes. @p sg must be a full subgraph; it is re-derived as the
 * full subgraph of the augmented graph afterward (adjacency indices shift
 * under the inserted edges, so the old edge bitset can't be migrated bit by
 * bit -- see gvizSubgraphMakeFull's contract).
 *
 * Bug fix relative to the old C gvizPlanarEmbedderTriangulate (see
 * Planar.cpp for the full explanation): the C version (a) cached a raw
 * pointer into `context->faces`' backing storage across a
 * gvizArrayPush(&context->faces, ...) call that could reallocate that same
 * array, and (b) collected the "hidden" vertices strictly between two split
 * points via a delete-while-iterating loop that skipped an element whenever
 * more than one vertex needed collecting, both because C had no bounds-safe
 * container. Neither bug is preserved here; the output for any triangulation
 * that didn't hit them is unchanged.
 */
void Triangulate(Graph &g, Subgraph &sg, FaceEnumerator &faces);

// ============================================================================
// PLANAR ROTATION APPLICATION (the core Boyer-Myrvold wrapper)
// ============================================================================

/**
 * Thrown instead of plain NotPlanarError by ApplyPlanarRotation/Planar's
 * constructor when a Kuratowski subdivision witness was requested and
 * computed. Defined here rather than added to the shared Error.hpp (out of
 * scope for this port to edit -- see the file-boundary note in this
 * project's task instructions); folding `kuratowskiSubdivision` directly
 * into NotPlanarError itself would be a reasonable, low-risk follow-up for
 * whoever next touches Error.hpp centrally. Callers that only need "was it
 * planar" can still catch the base `NotPlanarError&` and ignore the rest.
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
 * @p g's layout and @p subgraph in place. Port of
 * gvizSubgraphApplyPlanarRotation.
 *
 * @param captureWitness  When true (the default) and the graph turns out
 *   non-planar, a Kuratowski subdivision witness is computed and attached
 *   to the thrown PlanarNotPlanarError; pass false to skip that (cheaper)
 *   when the caller only cares about the planar/non-planar outcome. This
 *   replaces the old C API's nullable `gvizGraph **kuratowski` out-param.
 *
 * @throws PlanarNotPlanarError (carrying the witness) if not planar and
 *   @p captureWitness is true; plain NotPlanarError if not planar and
 *   @p captureWitness is false.
 * @throws LayoutError on an internal Boyer-Myrvold failure unrelated to
 *   planarity (the vendored library reports these as an ordinary int
 *   return, not something C++ can observe as std::bad_alloc directly).
 */
void ApplyPlanarRotation(Graph &g, Subgraph &subgraph, bool captureWitness = true);

// ============================================================================
// THE PLANAR EMBEDDER
// ============================================================================

/**
 * Planar straight-line embedder: port of gvizPlanarEmbedderState +
 * gvizPlanarEmbedderInit/Embed. Construction tests planarity and installs a
 * CCW rotation system (throwing PlanarNotPlanarError on failure, carrying a
 * Kuratowski witness -- see ApplyPlanarRotation); Embed() triangulates and
 * runs a Schnyder-wood straight-line embedding (see SchnyderWood.hpp,
 * including its IMPLEMENTATION STATUS note -- Embed() inherits that
 * caveat).
 *
 * Holds its own `Graph&` (see the file-level note above on why) in addition
 * to the `Subgraph` it hands to the EmbeddedGraph base -- both must refer to
 * the same graph; passing a @p subgraph not derived from @p g is a
 * precondition violation, unchecked, matching every other cross-object
 * consistency assumption in this library (e.g. Subgraph vs. its parent
 * Graph elsewhere).
 */
class Planar : public EmbeddedGraph {
public:
  /**
   * @throws PlanarNotPlanarError if @p g restricted to @p subgraph is not
   *         planar (see ApplyPlanarRotation).
   * @throws LayoutError on an internal Boyer-Myrvold failure.
   */
  Planar(Graph &g, Subgraph subgraph);

  /** Computes a straight-line embedding (2D) for the planar graph. See the
   *  class doc's SchnyderWood caveat. */
  void Embed();

private:
  static Subgraph MakeRotated(Graph &g, Subgraph &&subgraph);

  Graph &graph_;
};

// ============================================================================
// PLANAR QUERIES ON ANY EMBEDDED GRAPH
// ============================================================================
//
// These operate on any EmbeddedGraph (whichever embedder produced it, as
// long as it has a rotation system installed -- IsPlanarEmbedded()) and are
// the planar module's knowledge, not EmbeddedGraph's own: mirrors the old
// C's "PLANAR QUERIES ON AN EMBEDDED GRAPH" section of gvizPlanarEmbedder.h.
//
// Note on gvizPlanarApplyRotationToEmbedding / gvizFaceSearchState /
// gvizPlanarNextFace: NOT ported as direct free-function equivalents --
// see Planar.cpp's top-of-file comment for why (the short version: the
// first needs to set EmbeddedGraph's protected planarEmbedded flag, which a
// free function outside the class hierarchy has no standing to do, and the
// second/third were a manual step-cursor over data gvizPlanarFaceSearchInit
// already computed eagerly in one shot -- FaceEnumerator's std::vector
// already IS that computed-once data, so range-for over
// FaceEnumerator::Faces() replaces the cursor with no loss of behavior).

/**
 * Builds a full subgraph (the face's vertices, plus the boundary edges
 * connecting consecutive vertices in the cycle) for @p face -- a CCW vertex
 * cycle as produced by FaceEnumerator. @p g's layout is built first if it
 * doesn't have one yet (matches the old faceToSubgraph's
 * "ensure, don't force" behavior).
 */
Subgraph FaceSubgraph(const Graph &g, const std::vector<size_t> &face);

/**
 * Finds the combinatorial face whose interior contains (@p worldX,
 * @p worldY) in @p embedding's current straight-line drawing (or, when the
 * point falls outside every face's bounding region, the largest face by
 * area -- matches the old gvizPlanarFaceSubgraphAt's "click outside the
 * drawing selects the outer/largest face" fallback) and returns a full
 * subgraph describing it.
 *
 * @return std::nullopt if @p embedding is not planar-embedded or no face is
 *         found (both routine, checkable outcomes -- replaces the old -1).
 */
std::optional<Subgraph> FaceSubgraphAt(const Graph &g, const EmbeddedGraph &embedding,
                                        double worldX, double worldY);

/**
 * The vertex cycle of @p sg's largest combinatorial face (by vertex count).
 * @return an empty vector if @p sg has no face (fewer than 3 vertices, or no
 *         faces at all) -- routine, checkable, replaces the old -1/max-cap
 *         out-buffer shape (a std::vector has no fixed capacity to exceed).
 */
std::vector<size_t> LargestFaceBoundary(const Graph &g, const Subgraph &sg);

} // namespace gviz::layout

#endif
