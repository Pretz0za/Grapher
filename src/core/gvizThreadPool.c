#include "core/gvizThreadPool.h"
#include "core/alloc.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#define GVIZ_THREAD_POOL_STACK_SIZE (8u * 1024u * 1024u)

static _Thread_local size_t g_gvizThreadPoolWorkerSlot = SIZE_MAX;

typedef struct gvizThreadPoolWorkerCtx {
  gvizThreadPool *pool;
  size_t workerIndex;
} gvizThreadPoolWorkerCtx;

typedef struct gvizThreadPoolTaskNode {
  gvizThreadPoolTask task;
  void *arg;
  struct gvizThreadPoolTaskNode *next;
} gvizThreadPoolTaskNode;

struct gvizThreadPool {
  pthread_mutex_t lock;
  pthread_cond_t hasWork;
  pthread_cond_t allIdle;
  gvizThreadPoolTaskNode *queueHead;
  gvizThreadPoolTaskNode *queueTail;
  size_t pendingCount;
  int shuttingDown;
  pthread_t *threads;
  size_t threadCount;
};

static gvizThreadPoolTaskNode *popTaskLocked(gvizThreadPool *pool) {
  gvizThreadPoolTaskNode *node = pool->queueHead;
  pool->queueHead = node->next;
  if (!pool->queueHead)
    pool->queueTail = NULL;
  return node;
}

static void *workerMain(void *arg) {
  gvizThreadPoolWorkerCtx *wctx = arg;
  gvizThreadPool *pool = wctx->pool;

  g_gvizThreadPoolWorkerSlot = wctx->workerIndex;

  pthread_mutex_lock(&pool->lock);
  for (;;) {
    while (!pool->queueHead && !pool->shuttingDown)
      pthread_cond_wait(&pool->hasWork, &pool->lock);
    if (!pool->queueHead)
      break;

    gvizThreadPoolTaskNode *node = popTaskLocked(pool);
    pthread_mutex_unlock(&pool->lock);

    node->task(node->arg);
    GVIZ_DEALLOC(node);

    pthread_mutex_lock(&pool->lock);
    pool->pendingCount--;
    if (pool->pendingCount == 0)
      pthread_cond_broadcast(&pool->allIdle);
  }
  pthread_mutex_unlock(&pool->lock);

  g_gvizThreadPoolWorkerSlot = SIZE_MAX;
  GVIZ_DEALLOC(wctx);
  return NULL;
}

gvizThreadPool *gvizThreadPoolCreate(size_t threadCount) {
  if (threadCount == 0) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    threadCount = n > 0 ? (size_t)n : 1;
  }

  gvizThreadPool *pool = GVIZ_ALLOC(sizeof(gvizThreadPool));
  if (!pool)
    return NULL;

  pool->queueHead = NULL;
  pool->queueTail = NULL;
  pool->pendingCount = 0;
  pool->shuttingDown = 0;
  pool->threadCount = 0;

  pool->threads = GVIZ_ALLOC(sizeof(pthread_t) * threadCount);
  if (!pool->threads) {
    GVIZ_DEALLOC(pool);
    return NULL;
  }

  if (pthread_mutex_init(&pool->lock, NULL) != 0) {
    GVIZ_DEALLOC(pool->threads);
    GVIZ_DEALLOC(pool);
    return NULL;
  }
  if (pthread_cond_init(&pool->hasWork, NULL) != 0) {
    pthread_mutex_destroy(&pool->lock);
    GVIZ_DEALLOC(pool->threads);
    GVIZ_DEALLOC(pool);
    return NULL;
  }
  if (pthread_cond_init(&pool->allIdle, NULL) != 0) {
    pthread_cond_destroy(&pool->hasWork);
    pthread_mutex_destroy(&pool->lock);
    GVIZ_DEALLOC(pool->threads);
    GVIZ_DEALLOC(pool);
    return NULL;
  }

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, GVIZ_THREAD_POOL_STACK_SIZE);

  for (size_t i = 0; i < threadCount; i++) {
    gvizThreadPoolWorkerCtx *wctx =
        GVIZ_ALLOC(sizeof(gvizThreadPoolWorkerCtx));
    if (!wctx)
      break;
    wctx->pool = pool;
    wctx->workerIndex = i;
    if (pthread_create(&pool->threads[i], &attr, workerMain, wctx) != 0) {
      GVIZ_DEALLOC(wctx);
      break;
    }
    pool->threadCount++;
  }
  pthread_attr_destroy(&attr);

  if (pool->threadCount == 0) {
    gvizThreadPoolDestroy(pool);
    return NULL;
  }

  return pool;
}

void gvizThreadPoolDestroy(gvizThreadPool *pool) {
  if (!pool)
    return;

  pthread_mutex_lock(&pool->lock);
  pool->shuttingDown = 1;
  pthread_cond_broadcast(&pool->hasWork);
  pthread_mutex_unlock(&pool->lock);

  for (size_t i = 0; i < pool->threadCount; i++)
    pthread_join(pool->threads[i], NULL);

  while (pool->queueHead) {
    gvizThreadPoolTaskNode *node = popTaskLocked(pool);
    GVIZ_DEALLOC(node);
  }

  pthread_cond_destroy(&pool->allIdle);
  pthread_cond_destroy(&pool->hasWork);
  pthread_mutex_destroy(&pool->lock);
  GVIZ_DEALLOC(pool->threads);
  GVIZ_DEALLOC(pool);
}

size_t gvizThreadPoolThreadCount(const gvizThreadPool *pool) {
  return pool ? pool->threadCount : 0;
}

size_t gvizThreadPoolWorkerSlot(const gvizThreadPool *pool) {
  if (!pool)
    return SIZE_MAX;
  if (g_gvizThreadPoolWorkerSlot != SIZE_MAX)
    return g_gvizThreadPoolWorkerSlot;
  return pool->threadCount;
}

int gvizThreadPoolSubmit(gvizThreadPool *pool, gvizThreadPoolTask task,
                         void *arg) {
  if (!pool || !task)
    return -1;

  gvizThreadPoolTaskNode *node = GVIZ_ALLOC(sizeof(gvizThreadPoolTaskNode));
  if (!node)
    return -1;
  node->task = task;
  node->arg = arg;
  node->next = NULL;

  pthread_mutex_lock(&pool->lock);
  if (pool->queueTail)
    pool->queueTail->next = node;
  else
    pool->queueHead = node;
  pool->queueTail = node;
  pool->pendingCount++;
  pthread_cond_signal(&pool->hasWork);
  pthread_mutex_unlock(&pool->lock);

  return 0;
}

void gvizThreadPoolWait(gvizThreadPool *pool) {
  if (!pool)
    return;

  pthread_mutex_lock(&pool->lock);
  while (pool->pendingCount > 0)
    pthread_cond_wait(&pool->allIdle, &pool->lock);
  pthread_mutex_unlock(&pool->lock);
}

typedef struct {
  _Atomic size_t nextIndex;
  size_t end;
  size_t grain;
  gvizThreadPoolRangeTask task;
  void *ctx;
  pthread_mutex_t lock;
  pthread_cond_t helpersDone;
  size_t activeHelpers;
} gvizForRangeJob;

static void forRangeRunChunks(gvizForRangeJob *job) {
  for (;;) {
    size_t begin = atomic_fetch_add_explicit(&job->nextIndex, job->grain,
                                             memory_order_relaxed);
    if (begin >= job->end)
      return;
    size_t end = begin + job->grain;
    if (end > job->end)
      end = job->end;
    job->task(job->ctx, begin, end);
  }
}

static void forRangeHelper(void *arg) {
  gvizForRangeJob *job = arg;
  forRangeRunChunks(job);

  pthread_mutex_lock(&job->lock);
  job->activeHelpers--;
  if (job->activeHelpers == 0)
    pthread_cond_signal(&job->helpersDone);
  pthread_mutex_unlock(&job->lock);
}

int gvizThreadPoolForRange(gvizThreadPool *pool, size_t begin, size_t end,
                           size_t grain, gvizThreadPoolRangeTask task,
                           void *ctx) {
  if (!task)
    return -1;
  if (begin >= end)
    return 0;
  if (grain == 0)
    grain = 1;

  size_t span = end - begin;
  if (!pool || pool->threadCount == 0 || span <= grain) {
    task(ctx, begin, end);
    return 0;
  }

  size_t chunkCount = (span + grain - 1) / grain;
  size_t helperCount = pool->threadCount;
  if (helperCount > chunkCount - 1)
    helperCount = chunkCount - 1;

  gvizForRangeJob job;
  atomic_init(&job.nextIndex, begin);
  job.end = end;
  job.grain = grain;
  job.task = task;
  job.ctx = ctx;
  job.activeHelpers = helperCount;
  if (pthread_mutex_init(&job.lock, NULL) != 0 ||
      pthread_cond_init(&job.helpersDone, NULL) != 0) {
    task(ctx, begin, end);
    return 0;
  }

  size_t failedSubmissions = 0;
  for (size_t i = 0; i < helperCount; i++) {
    if (gvizThreadPoolSubmit(pool, forRangeHelper, &job) < 0)
      failedSubmissions++;
  }
  if (failedSubmissions) {
    pthread_mutex_lock(&job.lock);
    job.activeHelpers -= failedSubmissions;
    pthread_mutex_unlock(&job.lock);
  }

  forRangeRunChunks(&job);

  pthread_mutex_lock(&job.lock);
  while (job.activeHelpers > 0)
    pthread_cond_wait(&job.helpersDone, &job.lock);
  pthread_mutex_unlock(&job.lock);

  pthread_cond_destroy(&job.helpersDone);
  pthread_mutex_destroy(&job.lock);

  return 0;
}
