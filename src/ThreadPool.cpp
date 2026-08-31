#include "ThreadPool.hpp"

#include <algorithm>
#include <atomic>
#include <system_error>
#include <unistd.h>

namespace gviz {

namespace {
constexpr size_t kStackSize = 8u * 1024u * 1024u;
thread_local size_t tls_workerSlot = SIZE_MAX;
} // namespace

ThreadPool::ThreadPool(size_t threadCount) {
  if (threadCount == 0) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    threadCount = n > 0 ? static_cast<size_t>(n) : 1;
  }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, kStackSize);

  threads_.reserve(threadCount);
  for (size_t i = 0; i < threadCount; i++) {
    auto *ctx = new WorkerCtx{this, i};
    pthread_t t;
    int rc = pthread_create(&t, &attr, &ThreadPool::WorkerTrampoline, ctx);
    if (rc != 0) {
      delete ctx;
      pthread_attr_destroy(&attr);
      JoinAll();
      throw std::system_error(rc, std::generic_category(),
                               "gviz::ThreadPool: pthread_create failed");
    }
    threads_.push_back(t);
  }
  pthread_attr_destroy(&attr);

  threadCount_ = threads_.size();
}

ThreadPool::~ThreadPool() { JoinAll(); }

void ThreadPool::JoinAll() {
  {
    std::lock_guard<std::mutex> lk(lock_);
    shuttingDown_ = true;
  }
  hasWork_.notify_all();
  for (pthread_t t : threads_)
    pthread_join(t, nullptr);
  threads_.clear();
}

void *ThreadPool::WorkerTrampoline(void *arg) {
  auto *ctx = static_cast<WorkerCtx *>(arg);
  ThreadPool *pool = ctx->pool;
  size_t index = ctx->index;
  delete ctx;

  pool->RunWorkerLoop(index);
  return nullptr;
}

void ThreadPool::RunWorkerLoop(size_t index) {
  tls_workerSlot = index;

  std::unique_lock<std::mutex> lk(lock_);
  for (;;) {
    hasWork_.wait(lk, [this] { return !queue_.empty() || shuttingDown_; });
    if (queue_.empty())
      break;

    std::function<void()> task = std::move(queue_.front());
    queue_.pop_front();
    lk.unlock();

    task();

    lk.lock();
    pendingCount_--;
    if (pendingCount_ == 0)
      allIdle_.notify_all();
  }

  tls_workerSlot = SIZE_MAX;
}

size_t ThreadPool::ThreadCount() const noexcept { return threadCount_; }

size_t ThreadPool::WorkerSlot() const noexcept {
  if (tls_workerSlot != SIZE_MAX)
    return tls_workerSlot;
  return threadCount_;
}

void ThreadPool::Submit(std::function<void()> task) {
  if (!task)
    return;

  {
    std::lock_guard<std::mutex> lk(lock_);
    queue_.push_back(std::move(task));
    pendingCount_++;
  }
  hasWork_.notify_one();
}

void ThreadPool::Wait() {
  std::unique_lock<std::mutex> lk(lock_);
  allIdle_.wait(lk, [this] { return pendingCount_ == 0; });
}

void ThreadPool::ForRange(size_t begin, size_t end, size_t grain,
                           std::function<void(size_t, size_t)> task) {
  if (!task)
    return;
  if (begin >= end)
    return;
  if (grain == 0)
    grain = 1;

  size_t span = end - begin;
  if (threadCount_ == 0 || span <= grain) {
    task(begin, end);
    return;
  }

  size_t chunkCount = (span + grain - 1) / grain;
  size_t helperCount = threadCount_;
  if (helperCount > chunkCount - 1)
    helperCount = chunkCount - 1;

  std::atomic<size_t> nextIndex{begin};
  std::mutex jobLock;
  std::condition_variable helpersDone;
  size_t activeHelpers = helperCount;

  auto runChunks = [&]() {
    for (;;) {
      size_t chunkBegin =
          nextIndex.fetch_add(grain, std::memory_order_relaxed);
      if (chunkBegin >= end)
        return;
      size_t chunkEnd = std::min(chunkBegin + grain, end);
      task(chunkBegin, chunkEnd);
    }
  };

  for (size_t i = 0; i < helperCount; i++) {
    Submit([&runChunks, &jobLock, &helpersDone, &activeHelpers]() {
      runChunks();
      std::lock_guard<std::mutex> lk(jobLock);
      if (--activeHelpers == 0)
        helpersDone.notify_one();
    });
  }

  runChunks();

  std::unique_lock<std::mutex> lk(jobLock);
  helpersDone.wait(lk, [&] { return activeHelpers == 0; });
}

} // namespace gviz
