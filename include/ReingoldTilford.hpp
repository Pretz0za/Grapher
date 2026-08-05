#ifndef GVIZ_REINGOLDTILFORD_HPP
#define GVIZ_REINGOLDTILFORD_HPP

#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "Graph.hpp"

#include <cstddef>
#include <vector>

namespace gviz::layout {

/**
 * Classic Reingold-Tilford tidy tree layout, 2D only. Direct port of the old
 * C gvizEmbeddedTree/gvizRTDecorators; see this class's .cpp for a
 * function-by-function mapping back to the original free functions.
 *
 * One-shot, not iterative: unlike ForceAtlas/GRIP/Tutte, there is no
 * relaxation loop, no actions, no stat series, and no Sync -- the graph is
 * assumed static for the lifetime of this object. The fixed, sequential
 * workflow (mirroring the old RTInit -> CalculateOffsets -> Embed sequence)
 * is: construct (validates tree-ness), CalculateOffsets(root, 0) once, then
 * Embed(root, origin) once. Calling CalculateOffsets/Embed more than once,
 * out of order, or with a mismatched root is unchecked -- the old C
 * functions never guarded against it either.
 *
 * Constructor takes a `const Graph&`, not a `Subgraph`, despite every other
 * embedder in this port taking ownership of a Subgraph: this algorithm
 * fundamentally needs two things Subgraph deliberately never exposes --
 * gviz::search::IsTree's `const Graph&` parameter (parent-scan tree
 * validation, whose output `parents` this class keeps and reuses as live
 * algorithm state, not just a one-time check), and positional adjacency
 * queries (Graph::Neighbor(v, i), Graph::NeighborPosition) that Subgraph's
 * iterator-only neighbor access can't answer. The old C version reached
 * around this the same way (its rtGraph() helper dereferenced
 * gvizSubgraph.g directly, bypassing subgraph filtering entirely) but could
 * do so silently because gvizSubgraph's parent pointer was a public struct
 * field in C; Subgraph.hpp's design deliberately closed that door (see its
 * class comment: "Subgraph still never hands out a `const Graph&`/pointer
 * to its parent"), so this port takes the Graph it actually needs up front
 * instead of reaching into a Subgraph that was never going to give it up.
 * A Subgraph is still constructed internally (Subgraph::CreateFull(graph))
 * and moved into the EmbeddedGraph base purely so this class gets the
 * generic position-buffer/draw-mask bookkeeping every embedder gets for
 * free -- the algorithm itself never reads it.
 */
class ReingoldTilford : public EmbeddedGraph {
public:
  /** Sentinel for Decorators::threadTo meaning no contour thread is set. */
  static constexpr size_t kNoThread = static_cast<size_t>(-1);

  /**
   * Validates that @p graph is a directed tree (via gviz::search::IsTree)
   * rooted at @p root specifically, and allocates per-vertex decorator
   * state. This root cross-check is new relative to the old C
   * gvizEmbeddedTreeRTInit, which never verified @p root was the tree's
   * actual root (the vertex with no parent) -- passing the wrong one there
   * silently laid out only the subtree beneath it instead of failing. @p
   * root out of [0, graph.Size()) remains unchecked/UB, matching Graph's
   * own unchecked-index contract (the validation above indexes parents_[root]
   * without a bounds check).
   *
   * @throws NotATreeError if @p graph is not a single-rooted directed tree
   * (undirected, multiple roots, a shared parent, or a non-spanning
   * structure -- see gviz::search::TreeCheckResult) or if @p root is not
   * that tree's actual root.
   * @throws NoLayoutError if @p graph has no built layout yet
   * (Subgraph::CreateFull's precondition -- call Graph::BuildLayout or
   * Graph::EnsureLayout first).
   * @throws std::bad_alloc on allocation failure.
   */
  ReingoldTilford(const Graph &graph, size_t root);

  /**
   * Computes horizontal offsets for the subtree rooted at @p root, treating
   * @p level as every visited vertex's depth. Must run once, before Embed,
   * over the same root passed to the constructor.
   *
   * Ported as-is from gvizEmbeddedTreeCalculateOffsets, including a latent
   * bug worth flagging rather than silently fixing here: the recursive
   * calls pass @p level down unchanged (never level + 1), so
   * Decorators::depth and Height() never reflect real tree depth -- every
   * vertex winds up recorded at the caller's original @p level. The only
   * consumer of depth (UpdateExtremes' "which extreme subtree is deeper"
   * comparison) always sees a tie and always takes its else branch as a
   * result. This doesn't corrupt the geometry the existing C test suite
   * checks (Embed's Y coordinate comes from position accumulation, not
   * from this depth field), but it does mean the deeper-extreme heuristic
   * the algorithm is supposed to implement never actually runs. Not fixed
   * here to keep this port behavior-identical to the C original; worth a
   * follow-up against both.
   */
  void CalculateOffsets(size_t root, size_t level);

  /**
   * Recursively assigns 2D positions starting from @p root at @p position
   * (Dim() == 2 doubles). Call CalculateOffsets first.
   */
  void Embed(size_t root, const double *position);

  /** The calculated height of the whole tree. See CalculateOffsets' doc
   *  comment -- currently always 0 (ported bug, not a new one). */
  size_t Height() const noexcept { return height_; }

private:
  // Per-vertex decorators, one per raw vertex id -- direct port of
  // gvizRTDecorators. offsets is a plain per-vertex std::vector<float>
  // rather than the old C's single arena-allocated block that every
  // vertex's offsets pointer sliced into: that arena existed purely to
  // replace V small mallocs with one big one (nothing else depended on the
  // slices being contiguous across vertices -- every access is scoped to
  // one vertex's own offsets array). This is a one-shot O(V) construction
  // algorithm, not a per-frame hot loop, so the extra allocation count from
  // V independent vectors is not worth the bookkeeping (offsetsOrigin +
  // offsetsOffset) the arena required.
  struct Decorators {
    std::vector<float> offsets;
    size_t lMost = 0;
    size_t rMost = 0;
    size_t depth = 0;
    /** Root of the subtree (if known) this vertex belongs to. size_t, not
     *  the old C `int`: every value ever stored here is a vertex id. */
    size_t ancestor = 0;
    size_t threadTo = kNoThread;
  };

  struct SeparationResult {
    float lOffset;
    float rOffset;
    float totalNewSeparation;
    float rmostSeparation;
  };

  struct SubtreePairExtremes {
    size_t ll;
    size_t lr;
    size_t rl;
    size_t rr;
  };

  bool IsThreaded(size_t v) const noexcept { return dec_[v].threadTo != kNoThread; }
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
   *  CalculateOffsets (SeparateAlongContours' `parents_[lrContour]` root
   *  lookup) -- not merely a one-time validation byproduct. */
  std::vector<int> parents_;
  size_t height_ = 0;
  size_t defaultAncestor_ = 0;
};

} // namespace gviz::layout

#endif
