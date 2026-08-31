#include "KNearest.hpp"

#include <atomic>
#include <cstdlib>

namespace gviz::search {

namespace {

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

} // namespace

namespace detail {

void RecordKnnProfile(size_t visited) {
  if (!KnnProfileEnabled())
    return;
  gKnnQueries.fetch_add(1, std::memory_order_relaxed);
  gKnnVisited.fetch_add(visited, std::memory_order_relaxed);
  unsigned long long prev = gKnnMaxVisited.load(std::memory_order_relaxed);
  while (visited > prev &&
         !gKnnMaxVisited.compare_exchange_weak(prev, visited, std::memory_order_relaxed))
    ;
}

} // namespace detail

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
