#ifndef GVIZ_ERROR_HPP
#define GVIZ_ERROR_HPP

#include <stdexcept>
#include <string>

namespace gviz {

/**
 * Base of every exception this library throws for a routine, expected
 * failure to construct something (as opposed to std::bad_alloc, which
 * propagates on its own from std::vector/new and needs no wrapper here).
 * Callers may catch this broadly or catch one of the concrete types below.
 */
class LayoutError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

/** Thrown by a planar-only embedder (Tutte, SpringTutte, Planar) when
 *  constructed over a non-planar graph. */
class NotPlanarError : public LayoutError {
public:
  explicit NotPlanarError(const std::string &what = "graph is not planar")
      : LayoutError(what) {}
};

/** Thrown by ReingoldTilford when constructed over a graph that is not a
 *  rooted directed tree. */
class NotATreeError : public LayoutError {
public:
  explicit NotATreeError(const std::string &what = "graph is not a tree")
      : LayoutError(what) {}
};

/** Thrown when an embedding dimension is invalid for the requesting
 *  embedder (e.g. ForceAtlas requires 2, GRIP requires 2/3/4) or a loaded
 *  embedding's dimension does not match the target. */
class DimensionError : public LayoutError {
public:
  explicit DimensionError(const std::string &what = "invalid embedding dimension")
      : LayoutError(what) {}
};

/** Thrown by GRIP when the active vertex count is smaller than dimension + 1. */
class InsufficientVerticesError : public LayoutError {
public:
  explicit InsufficientVerticesError(
      const std::string &what = "not enough vertices for the requested dimension")
      : LayoutError(what) {}
};

/**
 * Thrown by Subgraph::CreateEmpty/CreateFull when the parent Graph has no
 * built layout yet (Graph::BuildLayout/EnsureLayout must run first). A full
 * subgraph's edge-bit addressing is defined in terms of the layout's
 * prefix sums, so there is no valid full subgraph to construct without one --
 * a routine, checkable precondition failure (the caller controls exactly
 * when to call BuildLayout), not an allocation failure or a logic bug.
 */
class NoLayoutError : public LayoutError {
public:
  explicit NoLayoutError(
      const std::string &what = "graph has no layout; call Graph::BuildLayout first")
      : LayoutError(what) {}
};

} // namespace gviz

#endif
