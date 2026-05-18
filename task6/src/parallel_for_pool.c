/**
 * @file parallel_for_pool.c
 * @brief 基于 Pthreads 线程池的并行 for 循环动态链接库
 *
 * 固定创建 MAX_THREADS 个线程，使用 phase 计数器同步。
 * 每次 parallel_for 调用 phase 递增，工人线程通过 phase 变化感知新工作。
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "parallel_for_pool.h"
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_THREADS 64

typedef struct {
  pthread_t thread;
  int thread_id;
  void *(*functor)(int, void *);
  void *arg;
  int work_start, work_end, work_increment;
  int mode;
  char _pad[48];
} pool_worker_t;

typedef struct {
  pool_worker_t workers[MAX_THREADS];
  pthread_mutex_t mutex;
  pthread_cond_t work_cv;
  pthread_cond_t done_cv;
  int phase; /* 每次调用递增 */
  _Atomic(int) active_count;
  _Atomic(int) dyn_next;
  int dyn_end, dyn_inc, dyn_chunk;
  int num_active; /* 本轮激活的线程数 */
  int exit_flag;
  int initialized;
} thread_pool_t;

static thread_pool_t g_pool = {0};

static void *worker_thread(void *arg) {
  pool_worker_t *w = (pool_worker_t *)arg;

  while (1) {
    pthread_mutex_lock(&g_pool.mutex);

    /* 等待 phase 变化 */
    int my_phase = g_pool.phase;
    while (g_pool.phase == my_phase && !g_pool.exit_flag) {
      pthread_cond_wait(&g_pool.work_cv, &g_pool.mutex);
    }
    if (g_pool.exit_flag) {
      pthread_mutex_unlock(&g_pool.mutex);
      return NULL;
    }

    /* 只有被激活的线程才执行工作并递减计数 */
    if (w->thread_id < g_pool.num_active) {
      void *(*fn)(int, void *) = w->functor;
      void *farg = w->arg;
      int ws = w->work_start, we = w->work_end, wi = w->work_increment;
      int mode = w->mode;
      pthread_mutex_unlock(&g_pool.mutex);

      if (mode == 0) {
        for (int i = ws; i < we; i += wi)
          fn(i, farg);
      } else {
        int end = g_pool.dyn_end, inc = g_pool.dyn_inc;
        while (1) {
          int next =
              atomic_load_explicit(&g_pool.dyn_next, memory_order_relaxed);
          if (next >= end)
            break;
          int chunk;
          if (mode == 2) {
            int rem = end - next;
            chunk = rem / (2 * g_pool.num_active);
            if (chunk < inc)
              chunk = inc;
          } else {
            chunk = g_pool.dyn_chunk;
          }
          int ls = atomic_fetch_add_explicit(&g_pool.dyn_next, chunk,
                                             memory_order_relaxed);
          if (ls >= end)
            break;
          int le = ls + chunk;
          if (le > end)
            le = end;
          for (int i = ls; i < le; i += inc)
            fn(i, farg);
        }
      }

      /* 完成：递减活跃线程数 */
      int c = atomic_fetch_sub_explicit(&g_pool.active_count, 1,
                                        memory_order_release) -
              1;
      if (c <= 0) {
        pthread_mutex_lock(&g_pool.mutex);
        pthread_cond_signal(&g_pool.done_cv);
        pthread_mutex_unlock(&g_pool.mutex);
      }
    } else {
      pthread_mutex_unlock(&g_pool.mutex);
    }
  }
  return NULL;
}

static int pool_init(void) {
  if (g_pool.initialized)
    return 0;
  pthread_mutex_init(&g_pool.mutex, NULL);
  pthread_cond_init(&g_pool.work_cv, NULL);
  pthread_cond_init(&g_pool.done_cv, NULL);
  g_pool.phase = 0;
  g_pool.exit_flag = 0;
  g_pool.initialized = 1;
  memset(g_pool.workers, 0, sizeof(g_pool.workers));
  for (int t = 0; t < MAX_THREADS; t++) {
    g_pool.workers[t].thread_id = t;
    g_pool.workers[t].mode = 0;
    if (pthread_create(&g_pool.workers[t].thread, NULL, worker_thread,
                       &g_pool.workers[t]) != 0) {
      fprintf(stderr, "parallel_for_pool: 线程 %d 创建失败\n", t);
      g_pool.exit_flag = 1;
      pthread_cond_broadcast(&g_pool.work_cv);
      for (int j = 0; j < t; j++)
        pthread_join(g_pool.workers[j].thread, NULL);
      g_pool.initialized = 0;
      return -1;
    }
  }
  return 0;
}

static void destroy_pool(void) {
  if (!g_pool.initialized)
    return;
  pthread_mutex_lock(&g_pool.mutex);
  g_pool.exit_flag = 1;
  g_pool.phase++;
  pthread_cond_broadcast(&g_pool.work_cv);
  pthread_mutex_unlock(&g_pool.mutex);
  for (int t = 0; t < MAX_THREADS; t++)
    pthread_join(g_pool.workers[t].thread, NULL);
  pthread_mutex_destroy(&g_pool.mutex);
  pthread_cond_destroy(&g_pool.work_cv);
  pthread_cond_destroy(&g_pool.done_cv);
  g_pool.initialized = 0;
}

typedef struct {
  int start, end, increment;
} chunk_t;
static void split_static(int ts, int te, int inc, int nt, chunk_t *c) {
  int total = (te - ts + inc - 1) / inc;
  int base = total / nt, rem = total % nt, cur = ts;
  for (int t = 0; t < nt; t++) {
    int count = base + (t < rem ? 1 : 0);
    c[t].start = cur;
    c[t].end = cur + count * inc;
    c[t].increment = inc;
    cur = c[t].end;
  }
}

int parallel_for(int start, int end, int increment,
                 void *(*functor)(int, void *), void *arg, int num_threads) {
  parallel_config_t config;
  config.num_threads = num_threads;
  config.schedule = SCHEDULE_STATIC;
  config.chunk_size = 1;
  return parallel_for_advanced(start, end, increment, functor, arg, &config);
}

int parallel_for_advanced(int start, int end, int increment,
                          void *(*functor)(int, void *), void *arg,
                          parallel_config_t *config) {
  if (config == NULL || config->num_threads <= 0)
    return -1;
  int num_threads = config->num_threads;
  schedule_type_t schedule = config->schedule;
  int chunk_size = config->chunk_size;
  int total_iters = (end - start + increment - 1) / increment;
  if (total_iters <= 0)
    return 0;
  if (total_iters < num_threads)
    num_threads = total_iters;
  if (num_threads == 1) {
    for (int i = start; i < end; i += increment)
      functor(i, arg);
    return 0;
  }
  if (pool_init() != 0)
    return -1;

  int nt = num_threads;
  chunk_t chunks[MAX_THREADS];
  for (int t = 0; t < nt; t++) {
    g_pool.workers[t].functor = functor;
    g_pool.workers[t].arg = arg;
  }

  pthread_mutex_lock(&g_pool.mutex);
  g_pool.phase++;
  g_pool.num_active = nt;
  atomic_store(&g_pool.active_count, nt);

  switch (schedule) {
  case SCHEDULE_STATIC: {
    split_static(start, end, increment, nt, chunks);
    for (int t = 0; t < nt; t++) {
      g_pool.workers[t].work_start = chunks[t].start;
      g_pool.workers[t].work_end = chunks[t].end;
      g_pool.workers[t].work_increment = chunks[t].increment;
      g_pool.workers[t].mode = 0;
    }
    break;
  }
  case SCHEDULE_DYNAMIC: {
    if (chunk_size <= 0)
      chunk_size = 1;
    atomic_store(&g_pool.dyn_next, start);
    g_pool.dyn_end = end;
    g_pool.dyn_inc = increment;
    g_pool.dyn_chunk = chunk_size;
    for (int t = 0; t < nt; t++)
      g_pool.workers[t].mode = 1;
    break;
  }
  case SCHEDULE_GUIDED: {
    atomic_store(&g_pool.dyn_next, start);
    g_pool.dyn_end = end;
    g_pool.dyn_inc = increment;
    g_pool.dyn_chunk = 1;
    for (int t = 0; t < nt; t++)
      g_pool.workers[t].mode = 2;
    break;
  }
  }

    pthread_cond_broadcast(&g_pool.work_cv);
    while (atomic_load_explicit(&g_pool.active_count, memory_order_acquire) > 0)
      pthread_cond_wait(&g_pool.done_cv, &g_pool.mutex);
    pthread_mutex_unlock(&g_pool.mutex);
    return 0;
}

void parallel_for_pool_destroy(void) { destroy_pool(); }
