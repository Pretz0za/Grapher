#ifndef GVIZ_REINGOLDTILFORD_HPP
#define GVIZ_REINGOLDTILFORD_HPP

#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "Graph.hpp"
#include "Tree.hpp"

#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * Classic Reingold-Tilford tidy tree layout, 2D only.
 *
 * One-shot, not iterative: no relaxation loop, no actions, no stat series --
 * the graph is assumed static for the lifetime of this object. Fixed,
 * sequential workflow: construct (validates tree-ness), CalculateOffsets
 * (root, 0) once, then Embed(root, origin) once. Calling
 * CalculateOffsets/Embed more than once, out of order, or with a mismatched
 * root is unchecked.
 *
 * Constructor takes a `const Graph&`, not a `Subgraph`, and is not
 * templated over GraphLike like most other embedders: this algorithm needs
 * gviz::search::IsTree's live `parents` output as algorithm state, and
 * positional adjacency queries (Graph::Neighbor(v, i),
 * Graph::NeighborPosition) that Subgraph's iterator-only neighbor access
 * can't answer. Treats @p graph's own dense [0, Size()) ids as the position
 * buffer's local index directly.
 */
class ReingoldTilford : public EmbeddedGraph {
public:
  /** Sentinel for Decorators::threadTo meaning no contour thread is set. */
  static constexpr size_t kNoThread = static_cast<size_t>(-1);

  /**
   * Validates that @p graph is a directed tree (via gviz::search::IsTree)
   * rooted at @p root specifically, and allocates per-vertex decorator
   * state. @p root out of [0, graph.Size()) is unchecked/UB.
   *
   * @throws NotATreeError if @p graph is not a single-rooted directed tree
   * (undirected, multiple roots, a shared parent, or a non-spanning
   * structure -- see gviz::search::TreeCheckResult) or if @p root is not
   * that tree's actual root.
   * @throws NoLayoutError if @p graph has no built layout yet (call
   * Graph::BuildLayout or Graph::EnsureLayout first).
   * @throws std::bad_alloc on allocation failure.
   */
  ReingoldTilford(const Graph &graph, size_t root);

  /**
   * Computes horizontal offsets for the subtree rooted at @p root, treating
   * @p level as every visited vertex's depth. Must run once, before Embed,
   * over the same root passed to the constructor. Recursive calls increment
   * @p level, so Decorators::depth and Height() reflect real tree depth.
   */
  void CalculateOffsets(size_t root, size_t level);

  /**
   * Recursively assigns 2D positions starting from @p root at @p position
   * (Dim() == 2 doubles). Call CalculateOffsets first.
   */
  void Embed(size_t root, const double *position);

  /** The calculated height of the whole tree (max depth reached by
   *  CalculateOffsets), 0 for a single-vertex tree. */
  size_t Height() const noexcept { return height_; }

private:
  // Per-vertex decorators, one per raw vertex id.
  struct Decorators {
    std::vector<float> offsets;
    size_t lMost = 0;
    size_t rMost = 0;
    size_t depth = 0;
    /** Root of the subtree (if known) this vertex belongs to. */
    size_t ancestor = 0;
    size_t threadTo = kNoThread;
  };

  struct SeparationResult {
    float lOffset;
    float rOffset;
    float totalNewSeparation;
    /** SeparateAlongContours' newSeparations[rightSubtreeIndex - 1] after
     *  its running-sum pass: the cumulative separation applied to the gap
     *  directly between the merged blob's last existing child and the new
     *  right subtree just folded in. CreateThreads needs this to convert a
     *  thread's stored offset out of the frame the merge just computed it
     *  in -- see CreateThreads' comment. */
    float rmostSeparation;
  };

  struct SubtreePairExtremes {
    size_t ll;
    size_t lr;
    size_t rl;
    size_t rr;
  };

  bool IsThreaded(size_t v) const noexcept { return dec_[v].threadTo != kNoThread; }
  bool ContourAtEnd(size_t v) const noexcept {
    return gviz::search::IsLeaf(graph_, v) && !IsThreaded(v);
  }
  float IterateContourRightward(size_t &contour);
  float IterateContourLeftward(size_t &contour);
  size_t GetAncestor(size_t root, size_t i) const;
  void SetAncestorAlongRightContour(size_t i);
  void InitializeRTLeaf(size_t i, size_t level);
  void InitializeRTSubtreeRoot(size_t root, size_t level);
  void CreateThreads(size_t root, size_t i, size_t &lrContour, size_t &rlContour,
                      const SubtreePairExtremes &extremes, const SeparationResult &res);
  void UpdateExtremes(size_t root, const SubtreePairExtremes &extremes,
                       const SeparationResult &res);
  SeparationResult SeparateAlongContours(size_t &lrContour, size_t &rlContour);
  void CombineSubtreeLeft(size_t root, size_t i);

  const Graph &graph_;
  std::vector<Decorators> dec_;
  /** Parent index of each vertex, -1 for the root; filled once by
   *  gviz::search::IsTree during construction and read throughout
   *  CalculateOffsets. */
  std::vector<int> parents_;
  size_t height_ = 0;
  size_t defaultAncestor_ = 0;
};

} // namespace gviz::layout

#endif
