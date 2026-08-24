#ifndef GVIZ_KNEAREST_HPP
#define GVIZ_KNEAREST_HPP

#include "BitSet.hpp"
#include "GraphLike.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <span>
#include <utility>
#include <vector>

namespace gviz::search {

/** Max visible seeds for the batched KNN path (see KNearestFromVisibleBatch). */
inline constexpr size_t kKnnBatchVisibleMax = 256;
/** Avg degree above this: per-vertex KNN tends to scan the whole component. */
inline constexpr double kKnnBatchMinAvgDegree = 32.0;
/** On sparse graphs, batch only when targets dwarf the visible seed count. */
inline constexpr size_t kKnnBatchTargetRatio = 8;

/** One BFS hit: a vertex and its edge-count distance from the search source. */
struct FoundVertex {
  size_t v;
  size_t dist;
};

/**
 * Reusable search state: an epoch-stamped visited array and a BFS queue,
 * reset in O(1) per query instead of memsetting an O(N) array or allocating
 * a fresh queue on every call. One scratch buffer per thread when searching
 * in parallel.
 *
 * Sized against gviz::GraphLikeVertexCapacity(g), not necessarily the
 * view's actual (dense) vertex count -- every vertex handle a GraphLike view
 * can ever yield is guaranteed < that capacity. Stamped directly by native
 * handle (no DenseIndex translation).
 */
class KNearestScratch {
public:
  /** Allocates state for views with vertex handles in [0, nvertices).
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
   *  stamp array first if the epoch counter itself wrapped (extremely
   *  rare). */
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

/** Resets counters used when GVIZ_KNN_PROFILE is set in the environment. */
void KnnProfileReset() noexcept;

/** Reads cumulative KNN BFS visit stats accumulated since the last
 *  KnnProfileReset(). Any of the three pointers may be nullptr to skip that
 *  field. */
void KnnProfileSnapshot(unsigned long long *queries, unsigned long long *visited,
                         unsigned long long *maxVisited) noexcept;

namespace detail {
void RecordKnnProfile(size_t visited);

/** Insertion-sorts (v, dist) into out[0, k) by ascending dist, tracking the
 *  live count in *count. */
inline void InsertSorted(std::span<FoundVertex> out, size_t &count, size_t k, size_t v,
                          size_t dist) {
  FoundVertex cand{v, dist};
  size_t n = count;
  if (n < k) {
    out[n] = cand;
    count++;
    for (size_t i = n; i > 0 && out[i].dist < out[i - 1].dist; i--)
      std::swap(out[i], out[i - 1]);
    return;
  }

  if (dist >= out[k - 1].dist)
    return;

  out[k - 1] = cand;
  for (size_t i = k - 1; i > 0 && out[i].dist < out[i - 1].dist; i--)
    std::swap(out[i], out[i - 1]);
}
} // namespace detail

/**
 * Same as the single-source KNearest below but reuses @p scratch for
 * visited tracking and the BFS queue instead of allocating a fresh
 * KNearestScratch.
 *
 * Additional unchecked/UB precondition beyond the other overload's:
 * scratch.Capacity() >= gviz::GraphLikeVertexCapacity(g).
 */
template <GraphLike G>
size_t KNearest(const G &g, std::span<FoundVertex> out, size_t k, size_t source,
                 const BitSet *filter, KNearestScratch &scratch) {
  if (k == 0)
    return 0;

  size_t epoch = scratch.BeginEpoch();
  scratch.MarkVisited(source, epoch);

  auto &queue = scratch.Queue();
  queue.push_back(FoundVertex{source, 0});

  size_t count = 0;
  size_t visited = 1;
  while (!queue.empty()) {
    FoundVertex curr = queue.front();
    queue.pop_front();

    for (size_t neighbor : g.Neighbors(curr.v)) {
      if (scratch.Visited(neighbor, epoch))
        continue;
      scratch.MarkVisited(neighbor, epoch);
      visited++;

      FoundVertex next{neighbor, curr.dist + 1};
      if (!filter || filter->Test(neighbor)) {
        out[count++] = next;
        if (count >= k) {
          detail::RecordKnnProfile(visited);
          return k;
        }
      }

      queue.push_back(next);
    }
  }

  detail::RecordKnnProfile(visited);
  return count;
}

/**
 * Finds up to @p k nearest vertices within @p g from @p source by edge
 * count, writing results (nearest first) into @p out and returning the
 * count actually written (<= k). Only vertices marked in @p filter count
 * toward @p k; @p filter == nullptr means every vertex in @p g is eligible.
 *
 * @p k == 0 returns 0 immediately without touching @p out or allocating
 * scratch.
 *
 * If @p source is not present in @p g (HasVertex(source) == false), this
 * quietly returns 0.
 *
 * Unchecked/UB preconditions:
 *   - @p out.size() >= k.
 *   - @p filter, if non-null, is sized to cover GraphLikeVertexCapacity(g).
 *   - @p source < GraphLikeVertexCapacity(g).
 *
 * Allocates and discards its own KNearestScratch sized to @p g -- for
 * repeated queries against the same view, use the KNearestScratch overload
 * above to reuse that allocation across calls instead.
 */
template <GraphLike G>
size_t KNearest(const G &g, std::span<FoundVertex> out, size_t k, size_t source,
                 const BitSet *filter = nullptr) {
  KNearestScratch scratch(GraphLikeVertexCapacity(g));
  return KNearest(g, out, k, source, filter, scratch);
}

/** One multi-target batch query: find up to k nearest @p visible vertices to
 *  @p vertex. @p out must have out.size() >= k (same contract as
 *  KNearest's @p out); @p count is set by KNearestFromVisibleBatch to the
 *  number of neighbors actually written (may be less than k if fewer
 *  visible vertices are reachable). */
struct KnnBatchTarget {
  size_t vertex;
  std::span<FoundVertex> out;
  size_t count = 0;
};

/** Outcome of KNearestFromVisibleBatch: NotApplicable means "call KNearest
 *  per-vertex instead, this is a normal fallback", distinct from Error. */
enum class BatchResult { Applied, Error, NotApplicable };

namespace detail {

inline bool AllTargetsFull(std::span<const KnnBatchTarget> targets, size_t k) {
  for (const auto &t : targets) {
    if (t.count < k)
      return false;
  }
  return true;
}

template <GraphLike G>
void KnnBfsFromSeed(const G &g, size_t seed, const std::vector<size_t> &targetMap,
                     std::span<KnnBatchTarget> targets, size_t k, KNearestScratch &scratch) {
  constexpr size_t kNoTarget = static_cast<size_t>(-1);

  size_t epoch = scratch.BeginEpoch();
  scratch.MarkVisited(seed, epoch);

  auto &queue = scratch.Queue();
  queue.push_back(FoundVertex{seed, 0});

  size_t visited = 1;
  while (!queue.empty()) {
    FoundVertex curr = queue.front();
    queue.pop_front();

    for (size_t neighbor : g.Neighbors(curr.v)) {
      if (scratch.Visited(neighbor, epoch))
        continue;
      scratch.MarkVisited(neighbor, epoch);
      visited++;

      size_t dist = curr.dist + 1;
      size_t ti = targetMap[neighbor];
      if (ti != kNoTarget)
        InsertSorted(targets[ti].out, targets[ti].count, k, seed, dist);

      queue.push_back(FoundVertex{neighbor, dist});
    }
  }

  RecordKnnProfile(visited);
}

} // namespace detail

/**
 * Finds up to @p k nearest @p visible vertices for each entry in @p targets
 * by running one BFS per visible seed instead of one BFS per target.
 * Intended when |visible| is much smaller than |targets| -- see
 * KNearestPreferBatch for the heuristic that decides this.
 *
 * @p visible (if non-null) and each target's @p out must be sized/capacitied
 * the same way as KNearest's @p filter/@p out -- unchecked/UB if not.
 *
 * @p visible == nullptr, empty, larger than kKnnBatchVisibleMax, or not
 * smaller than targets.size() all mean the batch path doesn't apply here;
 * returns BatchResult::NotApplicable and leaves every targets[i].count
 * unmodified.
 *
 * Returns BatchResult::Error (and leaves targets untouched) if @p scratch is
 * too small for @p g (scratch.Capacity() < GraphLikeVertexCapacity(g)).
 * Allocation failure throws std::bad_alloc, same as elsewhere.
 */
template <GraphLike G>
BatchResult KNearestFromVisibleBatch(const G &g, const BitSet *visible, size_t k,
                                      std::span<KnnBatchTarget> targets,
                                      KNearestScratch &scratch) {
  constexpr size_t kNoTarget = static_cast<size_t>(-1);

  if (k == 0 || !visible || targets.empty())
    return BatchResult::NotApplicable;

  size_t n = GraphLikeVertexCapacity(g);
  if (scratch.Capacity() < n)
    return BatchResult::Error;

  size_t visibleCount = visible->Popcount();
  if (visibleCount == 0 || visibleCount > kKnnBatchVisibleMax || visibleCount >= targets.size())
    return BatchResult::NotApplicable;

  std::vector<size_t> targetMap(n, kNoTarget);
  for (size_t i = 0; i < targets.size(); i++) {
    if (targets[i].vertex < n)
      targetMap[targets[i].vertex] = i;
  }
  for (auto &t : targets)
    t.count = 0;

  for (size_t seed : *visible) {
    detail::KnnBfsFromSeed(g, seed, targetMap, targets, k, scratch);
    if (detail::AllTargetsFull(targets, k))
      break;
  }

  return BatchResult::Applied;
}

/**
 * Heuristic for whether KNearestFromVisibleBatch is likely faster than
 * calling KNearest once per target. On sparse graphs, per-vertex BFS stops
 * early once k neighbors are found; batch always runs |visible| full sweeps
 * of the view, so it's only a win when the graph is hub-heavy (average
 * degree above kKnnBatchMinAvgDegree) or there are many more targets than
 * visible seeds (targetCount >= kKnnBatchTargetRatio * visibleCount).
 */
template <GraphLike G>
bool KNearestPreferBatch(const G &g, size_t visibleCount, size_t targetCount) noexcept {
  if (visibleCount == 0 || visibleCount >= targetCount || visibleCount > kKnnBatchVisibleMax)
    return false;

  size_t vc = GraphLikeVertexCount(g);
  if (vc == 0)
    return false;

  // ec is the sum of degrees (each edge counted from both endpoints), so
  // 2*ec/vc is the average degree.
  size_t ec = 0;
  for (size_t u : g)
    ec += g.Degree(u);
  double avgDeg = (2.0 * static_cast<double>(ec)) / static_cast<double>(vc);
  if (avgDeg > kKnnBatchMinAvgDegree)
    return true;

  return targetCount >= kKnnBatchTargetRatio * visibleCount;
}

} // namespace gviz::search

#endif
