#ifndef GVIZ_KNEAREST_HPP
#define GVIZ_KNEAREST_HPP

#include "BitSet.hpp"
#include "Subgraph.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <span>
#include <vector>

namespace gviz::search {

/** Max visible seeds for the batched KNN path (see KNearestFromVisibleBatch). */
inline constexpr size_t kKnnBatchVisibleMax = 256;
/** Avg degree above this: per-vertex KNN tends to scan the whole component. */
inline constexpr double kKnnBatchMinAvgDegree = 32.0;
/** On sparse graphs, batch only when targets dwarf the visible seed count. */
inline constexpr size_t kKnnBatchTargetRatio = 8;

/** One BFS hit: a vertex and its edge-count distance from the search source.
 *  Direct port of the old gvizFoundVertex. */
struct FoundVertex {
  size_t v;
  size_t dist;
};

/**
 * Reusable search state: an epoch-stamped visited array and a BFS queue,
 * reset in O(1) per query instead of memsetting an O(N) array or allocating
 * a fresh queue on every call. One scratch buffer per thread when searching
 * in parallel -- the pattern GRIP's per-thread KNN scratch buffers are built
 * around (see gvizGRIPInternal.h's knnScratch).
 *
 * Sized against Subgraph::VertexCapacity(), not the parent Graph's vertex
 * count directly. Subgraph deliberately never exposes its parent Graph
 * (KNearest.cpp only ever reaches it through Subgraph's own public surface),
 * and every vertex id a Subgraph can ever yield -- as a BFS neighbor, or via
 * HasVertex(u) == true -- is guaranteed < VertexCapacity() by Subgraph's own
 * invariants (HasVertex/the neighbor iterators both bail out at
 * vertexBits_.Size()). So VertexCapacity() is exactly the id space this
 * scratch needs to cover; pass it (or the largest VertexCapacity() among
 * several subgraphs a scratch will be reused across) to the constructor.
 *
 * The low-level BeginEpoch/Visited/MarkVisited/Queue members below exist so
 * KNearest()/KNearestFromVisibleBatch() (and no one else) can drive a
 * traversal against this state; typical callers just construct a
 * KNearestScratch and hand it to those functions.
 */
class KNearestScratch {
public:
  /** Allocates state for subgraphs with vertex ids in [0, nvertices).
   *  @throws std::bad_alloc on allocation failure. */
  explicit KNearestScratch(size_t nvertices) : stamp_(nvertices, size_t{0}) {}

  KNearestScratch(const KNearestScratch &) = delete;
  KNearestScratch &operator=(const KNearestScratch &) = delete;
  KNearestScratch(KNearestScratch &&) noexcept = default;
  KNearestScratch &operator=(KNearestScratch &&) noexcept = default;
  ~KNearestScratch() = default;

  /** The vertex id space this scratch can address (the nvertices it was
   *  constructed with). */
  size_t Capacity() const noexcept { return stamp_.size(); }

  /** Starts a new epoch: clears the queue and returns a stamp value that
   *  compares unequal to every vertex's previous stamp, resetting the O(N)
   *  stamp array first if the epoch counter itself wrapped (extremely rare;
   *  preserves the old gvizKNearestScratch's exact wraparound handling). */
  size_t BeginEpoch() noexcept {
    size_t epoch = ++epoch_;
    if (epoch == 0) {
      std::fill(stamp_.begin(), stamp_.end(), size_t{0});
      epoch = epoch_ = 1;
    }
    queue_.clear();
    return epoch;
  }

  bool Visited(size_t v, size_t epoch) const noexcept { return stamp_[v] == epoch; }
  void MarkVisited(size_t v, size_t epoch) noexcept { stamp_[v] = epoch; }
  std::deque<FoundVertex> &Queue() noexcept { return queue_; }

private:
  std::vector<size_t> stamp_;
  size_t epoch_ = 1;
  std::deque<FoundVertex> queue_;
};

/**
 * Finds up to @p k nearest vertices within @p sg from @p source by edge
 * count, writing results (nearest first) into @p out and returning the
 * count actually written (<= k). Only vertices marked in @p filter count
 * toward @p k; @p filter == nullptr means every vertex in @p sg is eligible
 * (mirrors the old gvizVertexSubset filter's NULL-means-unfiltered
 * contract).
 *
 * @p k == 0 returns 0 immediately without touching @p out or allocating
 * scratch -- matches the old gvizSearchKNearest.
 *
 * If @p source is not present in @p sg (HasVertex(source) == false), this
 * quietly returns 0 -- the same "routine, checkable, not exceptional"
 * treatment BreadthFirst/DepthFirst give an absent source, and it falls out
 * for free here: Subgraph::Neighbors(source) already yields nothing for a
 * vertex that isn't shown (see Subgraph.hpp), so no extra check is needed
 * for that case specifically.
 *
 * Preconditions that ARE unchecked/UB (same hot-path contract as
 * Graph::Neighbor, BitSet::Test, etc. elsewhere in this port -- this
 * function sits in a loop that can run millions of times during GRIP
 * filtration/placement, so it does not spend a branch re-deriving what the
 * caller already guarantees):
 *   - @p out.size() >= k.
 *   - @p filter, if non-null, is sized to cover sg.VertexCapacity().
 *   - @p source < sg.VertexCapacity() (an absent-but-in-range source is
 *     fine, per above; a source id that isn't addressable at all is not).
 *
 * Allocates and discards its own KNearestScratch sized to @p sg -- for
 * repeated queries against the same (or growing) subgraph, e.g. GRIP's
 * per-vertex refinement loop, use the KNearestScratch overload below to
 * reuse that allocation across calls instead.
 */
size_t KNearest(const Subgraph &sg, std::span<FoundVertex> out, size_t k, size_t source,
                 const BitSet *filter = nullptr);

/**
 * Same as KNearest but reuses @p scratch for visited tracking and the BFS
 * queue instead of allocating a fresh KNearestScratch -- the reusable-scratch
 * path GRIP's per-thread scratch buffers are built around (see
 * KNearestScratch's class doc).
 *
 * Additional unchecked/UB precondition beyond the other overload's:
 * scratch.Capacity() >= sg.VertexCapacity(). A scratch sized for a smaller
 * graph is a caller wiring bug (e.g. reusing a per-thread scratch across
 * subgraphs of different sizes without re-sizing it), same class of
 * violation as passing an undersized @p out.
 */
size_t KNearest(const Subgraph &sg, std::span<FoundVertex> out, size_t k, size_t source,
                 const BitSet *filter, KNearestScratch &scratch);

/** One multi-target batch query: find up to k nearest @p visible vertices to
 *  @p vertex. @p out must have out.size() >= k (same contract as
 *  KNearest's @p out); @p count is set by KNearestFromVisibleBatch to the
 *  number of neighbors actually written (may be less than k if fewer
 *  visible vertices are reachable). Direct port of gvizKNNBatchTarget,
 *  with the old fixed-capacity pointer replaced by a span so the capacity
 *  travels with the buffer instead of being an implicit caller contract. */
struct KnnBatchTarget {
  size_t vertex;
  std::span<FoundVertex> out;
  size_t count = 0;
};

/**
 * Outcome of KNearestFromVisibleBatch. The old C function returned a tri-state
 * int (0 success / -1 error / 1 "doesn't apply, fall back to per-vertex
 * KNearest") -- collapsing that into a bool would silently merge "call
 * KNearest instead, this is normal" with "something is actually wrong",
 * which callers need to tell apart, so it stays a real three-way enum.
 */
enum class BatchResult { Applied, Error, NotApplicable };

/**
 * Finds up to @p k nearest @p visible vertices for each entry in @p targets
 * by running one BFS per visible seed instead of one BFS per target.
 * Intended when |visible| is much smaller than |targets| (early GRIP
 * layers) -- see KNearestPreferBatch for the heuristic that decides this.
 *
 * @p visible (if non-null) and each target's @p out must be sized/capacitied
 * the same way as KNearest's @p filter/@p out -- unchecked/UB if not, same
 * hot-path contract.
 *
 * @p visible == nullptr, empty, larger than kKnnBatchVisibleMax, or not
 * smaller than targets.size() all mean the batch path doesn't apply here;
 * returns BatchResult::NotApplicable and leaves every targets[i].count
 * unmodified (the caller is expected to fall back to per-vertex KNearest,
 * exactly as with the old int return of 1).
 *
 * Returns BatchResult::Error (and leaves targets untouched) if @p scratch is
 * too small for @p sg (scratch.Capacity() < sg.VertexCapacity()) -- unlike
 * KNearest's scratch-sizing precondition, this one gets a real checked
 * return here rather than being UB, because it's cheap to test once per
 * batch call (not once per BFS-visited-vertex) and the caller explicitly
 * needs a non-exceptional way to detect it (see BatchResult's doc comment).
 * Genuine allocation failure (the old code's other -1 cases: the target-map
 * allocation, or a deque push during BFS) now throws std::bad_alloc instead,
 * same as everywhere else vector/deque allocate under the hood --
 * BatchResult intentionally does not have a case for that.
 */
BatchResult KNearestFromVisibleBatch(const Subgraph &sg, const BitSet *visible, size_t k,
                                      std::span<KnnBatchTarget> targets,
                                      KNearestScratch &scratch);

/**
 * Heuristic for whether KNearestFromVisibleBatch is likely faster than
 * calling KNearest once per target. On sparse graphs, per-vertex BFS stops
 * early once k neighbors are found; batch always runs |visible| full sweeps
 * of the subgraph, so it's only a win when the graph is hub-heavy (average
 * degree above kKnnBatchMinAvgDegree) or there are many more targets than
 * visible seeds (targetCount >= kKnnBatchTargetRatio * visibleCount).
 */
bool KNearestPreferBatch(const Subgraph &sg, size_t visibleCount, size_t targetCount) noexcept;

/** Resets counters used when GVIZ_KNN_PROFILE is set in the environment. */
void KnnProfileReset() noexcept;

/** Reads cumulative KNN BFS visit stats accumulated since the last
 *  KnnProfileReset(). Any of the three pointers may be nullptr to skip that
 *  field, matching the old gvizKNNProfileSnapshot. */
void KnnProfileSnapshot(unsigned long long *queries, unsigned long long *visited,
                         unsigned long long *maxVisited) noexcept;

} // namespace gviz::search

#endif
