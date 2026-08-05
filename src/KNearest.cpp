#include "KNearest.hpp"

#include <atomic>
#include <cstdlib>
#include <limits>
#include <utility>

namespace gviz::search {

namespace {

constexpr size_t kNoTarget = std::numeric_limits<size_t>::max();

std::atomic<unsigned long long> gKnnQueries{0};
std::atomic<unsigned long long> gKnnVisited{0};
std::atomic<unsigned long long> gKnnMaxVisited{0};
std::atomic<int> gKnnProfileEnabled{-1};

bool KnnProfileEnabled() {
  int e = gKnnProfileEnabled.load(std::memory_order_relaxed);
  if (e < 0) {
    e = std::getenv("GVIZ_KNN_PROFILE") != nullptr;
    gKnnProfileEnabled.store(e, std::memory_order_relaxed);
  }
  return e;
}

void RecordProfile(size_t visited) {
  if (!KnnProfileEnabled())
    return;
  gKnnQueries.fetch_add(1, std::memory_order_relaxed);
  gKnnVisited.fetch_add(visited, std::memory_order_relaxed);
  unsigned long long prev = gKnnMaxVisited.load(std::memory_order_relaxed);
  while (visited > prev &&
         !gKnnMaxVisited.compare_exchange_weak(prev, visited, std::memory_order_relaxed))
    ;
}

/** Insertion-sorts (v, dist) into out[0, k) by ascending dist, tracking the
 *  live count in *count. Only used by the batch path -- a single seed's BFS
 *  contributes candidate distances to many different targets' top-k lists
 *  out of distance order, unlike KNearest's own BFS, which visits vertices
 *  in nondecreasing distance order already and so never needs to re-sort. */
void InsertSorted(std::span<FoundVertex> out, size_t &count, size_t k, size_t v, size_t dist) {
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

bool AllTargetsFull(std::span<const KnnBatchTarget> targets, size_t k) {
  for (const auto &t : targets) {
    if (t.count < k)
      return false;
  }
  return true;
}

void KnnBfsFromSeed(const Subgraph &sg, size_t seed, const std::vector<size_t> &targetMap,
                     std::span<KnnBatchTarget> targets, size_t k, KNearestScratch &scratch) {
  size_t epoch = scratch.BeginEpoch();
  scratch.MarkVisited(seed, epoch);

  auto &queue = scratch.Queue();
  queue.push_back(FoundVertex{seed, 0});

  size_t visited = 1;
  while (!queue.empty()) {
    FoundVertex curr = queue.front();
    queue.pop_front();

    for (size_t neighbor : sg.Neighbors(curr.v)) {
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

  RecordProfile(visited);
}

} // namespace

size_t KNearest(const Subgraph &sg, std::span<FoundVertex> out, size_t k, size_t source,
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

    for (size_t neighbor : sg.Neighbors(curr.v)) {
      if (scratch.Visited(neighbor, epoch))
        continue;
      scratch.MarkVisited(neighbor, epoch);
      visited++;

      FoundVertex next{neighbor, curr.dist + 1};
      if (!filter || filter->Test(neighbor)) {
        out[count++] = next;
        if (count >= k) {
          RecordProfile(visited);
          return k;
        }
      }

      queue.push_back(next);
    }
  }

  RecordProfile(visited);
  return count;
}

size_t KNearest(const Subgraph &sg, std::span<FoundVertex> out, size_t k, size_t source,
                 const BitSet *filter) {
  KNearestScratch scratch(sg.VertexCapacity());
  return KNearest(sg, out, k, source, filter, scratch);
}

BatchResult KNearestFromVisibleBatch(const Subgraph &sg, const BitSet *visible, size_t k,
                                      std::span<KnnBatchTarget> targets,
                                      KNearestScratch &scratch) {
  if (k == 0 || !visible || targets.empty())
    return BatchResult::NotApplicable;

  size_t n = sg.VertexCapacity();
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
    KnnBfsFromSeed(sg, seed, targetMap, targets, k, scratch);
    if (AllTargetsFull(targets, k))
      break;
  }

  return BatchResult::Applied;
}

bool KNearestPreferBatch(const Subgraph &sg, size_t visibleCount, size_t targetCount) noexcept {
  if (visibleCount == 0 || visibleCount >= targetCount || visibleCount > kKnnBatchVisibleMax)
    return false;

  size_t vc = sg.VertexCount();
  if (vc == 0)
    return false;

  size_t ec = sg.EdgeCount();
  double avgDeg = (2.0 * static_cast<double>(ec)) / static_cast<double>(vc);
  if (avgDeg > kKnnBatchMinAvgDegree)
    return true;

  return targetCount >= kKnnBatchTargetRatio * visibleCount;
}

void KnnProfileReset() noexcept {
  gKnnQueries.store(0, std::memory_order_relaxed);
  gKnnVisited.store(0, std::memory_order_relaxed);
  gKnnMaxVisited.store(0, std::memory_order_relaxed);
}

void KnnProfileSnapshot(unsigned long long *queries, unsigned long long *visited,
                         unsigned long long *maxVisited) noexcept {
  if (queries)
    *queries = gKnnQueries.load(std::memory_order_relaxed);
  if (visited)
    *visited = gKnnVisited.load(std::memory_order_relaxed);
  if (maxVisited)
    *maxVisited = gKnnMaxVisited.load(std::memory_order_relaxed);
}

} // namespace gviz::search
