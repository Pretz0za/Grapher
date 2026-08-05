#ifndef GVIZ_THREAD_POOL_HPP
#define GVIZ_THREAD_POOL_HPP

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <pthread.h>
#include <vector>

namespace gviz {

/**
 * A fixed-size pool of worker threads with a FIFO task queue and a blocking
 * data-parallel range primitive. The pool is fully self-contained: it knows
 * nothing about graphs or embeddings and can be reused by any subsystem.
 *
 * RAII: the constructor spins up the workers and leaves the pool immediately
 * usable, or throws (std::system_error, if a worker thread fails to start)
 * without leaving a partially-usable pool behind. The destructor drains any
 * queued/running tasks and joins every worker automatically — there is no
 * separate Destroy().
 *
 * Thread-safety contract: Submit may be called from any thread, including
 * from tasks running on the pool. Wait and ForRange block, and must
 * therefore only be called from threads outside the pool (a task blocking on
 * its own pool can deadlock once every worker does the same).
 *
 * Worker threads are created with 8 MiB stacks so that tasks may use the
 * same stack-heavy patterns (VLAs sized by vertex count) as the main thread.
 * std::thread has no portable way to request a stack size before the thread
 * starts running, so workers are raw pthread_t under the hood; the public
 * interface is pure C++ (std::function callables, no thread handles
 * exposed).
 */
class ThreadPool {
public:
  /**
   * Creates a pool with @p threadCount workers. Pass 0 (the default) to use
   * the number of logical processors available at startup.
   *
   * @throws std::system_error if a worker thread fails to start. Unlike the
   * old C gvizThreadPoolCreate (which tolerated partial startup failure and
   * only failed if zero workers came up), this throws immediately on the
   * first failure and leaves no pool object behind — RAII either hands back
   * a fully-usable pool or nothing, never a partially-staffed one.
   */
  explicit ThreadPool(size_t threadCount = 0);

  /** Drains all queued and running tasks and joins the workers. */
  ~ThreadPool();

  ThreadPool(const ThreadPool &) = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = delete;
  ThreadPool &operator=(ThreadPool &&) = delete;

  /** Returns the number of worker threads. */
  size_t ThreadCount() const noexcept;

  /**
   * Returns the worker slot for the calling thread: 0 .. ThreadCount()-1 on
   * a pool worker thread, or ThreadCount() on any other thread (including
   * the thread that calls ForRange). Backed by a thread_local, so — as in
   * the old C implementation — the slot reflects whichever pool the calling
   * thread happens to be a worker of, not necessarily the pool this method
   * is called on; only one pool's workers are normally live on a given
   * thread at a time in practice. Use this to index per-thread scratch
   * buffers sized ThreadCount()+1.
   */
  size_t WorkerSlot() const noexcept;

  /**
   * Enqueues @p task to run on a worker thread. An empty std::function is a
   * silent no-op (the old C API's "task is NULL" failure case, which had no
   * work to enqueue either way — there is no longer a failure code to
   * report since std::function has no separate allocation-failure path
   * distinct from std::bad_alloc, which propagates on its own).
   */
  void Submit(std::function<void()> task);

  /** Blocks until every queued and running task has completed. */
  void Wait();

  /**
   * Runs @p task over the half-open range [@p begin, @p end) in parallel and
   * blocks until the whole range has been processed. The range is dealt out
   * in chunks of at most @p grain indices (pass 0 for a grain of 1); idle
   * workers and the calling thread grab chunks until none remain, which
   * load-balances uneven per-index costs.
   *
   * Runs the range serially on the calling thread when the range does not
   * exceed one grain, so callers need no serial fallback path. An empty
   * @p task is a silent no-op, same reasoning as Submit.
   */
  void ForRange(size_t begin, size_t end, size_t grain,
                std::function<void(size_t, size_t)> task);

private:
  struct WorkerCtx {
    ThreadPool *pool;
    size_t index;
  };

  static void *WorkerTrampoline(void *arg);
  void RunWorkerLoop(size_t index);
  void JoinAll();

  std::mutex lock_;
  std::condition_variable hasWork_;
  std::condition_variable allIdle_;
  std::deque<std::function<void()>> queue_;
  size_t pendingCount_ = 0;
  bool shuttingDown_ = false;
  std::vector<pthread_t> threads_;
  size_t threadCount_ = 0;
};

} // namespace gviz

#endif
