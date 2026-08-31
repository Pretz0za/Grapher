#ifndef GVIZ_CONNECTEDCOMPONENTS_HPP
#define GVIZ_CONNECTEDCOMPONENTS_HPP

#include "Subgraph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::search {

/**
 * Result of ConnectedComponents(): @c labels holds one 0-based component id
 * per vertex id in @p sg's addressable range (Subgraph::VertexCapacity()),
 * SIZE_MAX for vertices not present in @p sg; @c count is the number of
 * components found (0 for an empty subgraph).
 */
struct Components {
  std::vector<size_t> labels;
  size_t count = 0;
};

/**
 * Labels the connected components of @p sg using repeated breadth-first
 * search (equivalent to repeated depth-first search on each unseen vertex).
 *
 * Simplified from the old C API: gvizConnectedComponents required the
 * caller to separately query gvizGraphSize(sg->g) to pre-size a `size_t
 * *labels` output buffer. A Subgraph now knows its own addressable vertex
 * range, so there's no separate sizing step left for the caller to get
 * right -- this returns an already-correctly-sized vector together with the
 * count in one value.
 *
 * A Subgraph is always well-formed (no null/unbound state -- see
 * Subgraph.hpp), so unlike the old C function (which returned -1 for a null
 * subgraph or missing layout) there is no failure mode left to report; this
 * never fails and always returns a result.
 *
 * Port note on directedness: the old C header claimed this "treats edges as
 * undirected regardless of the parent graph's directed flag," but the old
 * implementation (and this one) simply walks @p sg's adjacency as given --
 * for an undirected parent Graph, edges are already stored symmetrically in
 * both endpoints' lists (see Graph::AddEdge), so components come out as true
 * connected components; for a *directed* parent Graph, only outgoing edges
 * are walked, so two vertices joined only by a reverse edge end up in
 * different components. That's the behavior preserved here -- not the
 * doc comment's "regardless of directed flag" claim, which the old test
 * suite never actually exercised (every ConnectedComponents test uses an
 * undirected graph).
 */
Components ConnectedComponents(const Subgraph &sg);

/**
 * Tallies component sizes from a Components::labels array. Returns a vector
 * of length @p componentCount; index i holds the number of vertices labeled
 * i. Unlike the old C gvizConnectedComponentSizes, @p nvertices is no
 * longer a separate parameter -- it's simply @p labels.size().
 */
std::vector<size_t> ConnectedComponentSizes(const std::vector<size_t> &labels,
                                             size_t componentCount);

} // namespace gviz::search

#endif
