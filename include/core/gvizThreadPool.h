#ifndef GVIZ_THREAD_POOL_H
#define GVIZ_THREAD_POOL_H

#include <stddef.h>

/**
 * A fixed-size pool of worker threads with a FIFO task queue and a blocking
 * data-parallel range primitive. The pool is fully self-contained: it knows
 * nothing about graphs or embeddings and can be reused by any subsystem.
 *
 * Thread-safety contract: gvizThreadPoolSubmit may be called from any
 * thread, including from tasks running on the pool. gvizThreadPoolWait and
 * gvizThreadPoolForRange block, and must therefore only be called from
 * threads outside the pool (a task blocking on its own pool can deadlock
 * once every worker does the same). Create/Destroy must not race with any
 * other call on the same pool.
 *
 * Worker threads are created with 8 MiB stacks so that tasks may use the
 * same stack-heavy patterns (VLAs sized by vertex count) as the main thread.
 */
typedef struct gvizThreadPool gvizThreadPool;

/** A unit of work submitted to the pool. */
typedef void (*gvizThreadPoolTask)(void *arg);

/**
 * A slice of a parallel loop. Invoked with half-open index ranges
 * [begin, end) that together tile the full range passed to
 * gvizThreadPoolForRange. Slices run concurrently; the callback must only
 * perform writes that are disjoint across slices (or otherwise synchronized).
 */
typedef void (*gvizThreadPoolRangeTask)(void *ctx, size_t begin, size_t end);

/**
 * Creates a pool with @p threadCount workers. Pass 0 to use the number of
 * logical processors available at startup.
 *
 * @return the pool, or NULL on allocation/thread-creation failure.
 */
gvizThreadPool *gvizThreadPoolCreate(size_t threadCount);

/**
 * Drains all queued and running tasks, joins the workers, and frees the
 * pool. NULL-safe.
 */
void gvizThreadPoolDestroy(gvizThreadPool *pool);

/** Returns the number of worker threads. NULL-safe (returns 0). */
size_t gvizThreadPoolThreadCount(const gvizThreadPool *pool);

/**
 * Returns the worker slot for the calling thread: 0 .. threadCount-1 on pool
 * worker threads, or @p pool->threadCount on any other thread (including the
 * thread that calls gvizThreadPoolForRange). Returns SIZE_MAX when @p pool is
 * NULL. Use this to index per-thread scratch buffers sized threadCount+1.
 */
size_t gvizThreadPoolWorkerSlot(const gvizThreadPool *pool);

/**
 * Enqueues @p task to run on a worker thread. @p arg must stay valid until
 * the task has executed.
 *
 * @return 0 on success, -1 on allocation failure or NULL arguments.
 */
int gvizThreadPoolSubmit(gvizThreadPool *pool, gvizThreadPoolTask task,
                         void *arg);

/** Blocks until every queued and running task has completed. NULL-safe. */
void gvizThreadPoolWait(gvizThreadPool *pool);

/**
 * Runs @p task over the half-open range [@p begin, @p end) in parallel and
 * blocks until the whole range has been processed. The range is dealt out in
 * chunks of at most @p grain indices (pass 0 for a grain of 1); idle workers
 * and the calling thread grab chunks until none remain, which load-balances
 * uneven per-index costs.
 *
 * Runs the range serially on the calling thread when @p pool is NULL or the
 * range does not exceed one grain, so callers need no serial fallback path.
 *
 * @return 0 on success, -1 if @p task is NULL.
 */
int gvizThreadPoolForRange(gvizThreadPool *pool, size_t begin, size_t end,
                           size_t grain, gvizThreadPoolRangeTask task,
                           void *ctx);

#endif
