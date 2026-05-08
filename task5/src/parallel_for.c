/**
 * @file parallel_for.c
 * @brief 基于 Pthreads 的并行 for 循环动态链接库实现
 *
 * 实现 parallel_for 函数，模仿 OpenMP 的 omp parallel for 构造
 * 支持静态调度、动态调度和引导调度
 */

#include "parallel_for.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * @brief 线程工作函数（静态调度）
 *
 * 每个线程负责一段连续的索引范围
 */
static void *thread_work_static(void *arg) {
  thread_work_t *work = (thread_work_t *)arg;

  for (int i = work->start; i < work->end; i += work->increment) {
    work->functor(i, work->arg);
  }

  return NULL;
}

/**
 * @brief 线程工作函数（动态调度）
 *
 * 线程从全局任务队列中动态获取任务块
 */
static pthread_mutex_t dynamic_mutex = PTHREAD_MUTEX_INITIALIZER;
static int dynamic_next_start = 0;
static int dynamic_end = 0;
static int dynamic_increment = 1;
static int dynamic_chunk_size = 1;

static void *thread_work_dynamic(void *arg) {
  thread_work_t *work = (thread_work_t *)arg;
  int local_start, local_end;

  while (1) {
    // 获取锁，从全局队列中取任务
    pthread_mutex_lock(&dynamic_mutex);

    if (dynamic_next_start >= dynamic_end) {
      pthread_mutex_unlock(&dynamic_mutex);
      break; // 没有更多任务
    }

    local_start = dynamic_next_start;
    local_end = local_start + dynamic_chunk_size * dynamic_increment;
    if (local_end > dynamic_end) {
      local_end = dynamic_end;
    }
    dynamic_next_start = local_end;

    pthread_mutex_unlock(&dynamic_mutex);

    // 执行本地任务
    for (int i = local_start; i < local_end; i += dynamic_increment) {
      work->functor(i, work->arg);
    }
  }

  return NULL;
}

/**
 * @brief 线程工作函数（引导调度）
 *
 * 块大小逐渐减小，开始分配大块，后来分配小块
 */
static pthread_mutex_t guided_mutex = PTHREAD_MUTEX_INITIALIZER;
static int guided_next_start = 0;
static int guided_end = 0;
static int guided_increment = 1;
static int guided_remaining = 0;
static int guided_num_threads = 1;

static void *thread_work_guided(void *arg) {
  thread_work_t *work = (thread_work_t *)arg;
  int local_start, local_end, chunk;

  while (1) {
    // 获取锁，计算当前块大小
    pthread_mutex_lock(&guided_mutex);

    if (guided_next_start >= guided_end) {
      pthread_mutex_unlock(&guided_mutex);
      break; // 没有更多任务
    }

    // 引导调度：块大小 = 剩余任务数 / (2 * 线程数)
    int remaining = guided_end - guided_next_start;
    chunk = remaining / (2 * guided_num_threads);
    if (chunk < guided_increment) {
      chunk = guided_increment; // 最小块大小为 increment
    }

    local_start = guided_next_start;
    local_end = local_start + chunk * guided_increment;
    if (local_end > guided_end) {
      local_end = guided_end;
    }
    guided_next_start = local_end;

    pthread_mutex_unlock(&guided_mutex);

    // 执行本地任务
    for (int i = local_start; i < local_end; i += guided_increment) {
      work->functor(i, work->arg);
    }
  }

  return NULL;
}

/**
 * @brief 基础并行 for 循环函数
 *
 * 使用静态调度策略
 */
int parallel_for(int start, int end, int increment,
                 void *(*functor)(int, void *), void *arg, int num_threads) {
  parallel_config_t config;
  config.num_threads = num_threads;
  config.schedule = SCHEDULE_STATIC;
  config.chunk_size = 1;

  return parallel_for_advanced(start, end, increment, functor, arg, &config);
}

/**
 * @brief 高级并行 for 循环函数（支持调度策略）
 */
int parallel_for_advanced(int start, int end, int increment,
                          void *(*functor)(int, void *), void *arg,
                          parallel_config_t *config) {
  if (config == NULL || config->num_threads <= 0) {
    fprintf(stderr, "parallel_for: 无效的配置参数\n");
    return -1;
  }

  int num_threads = config->num_threads;
  schedule_type_t schedule = config->schedule;
  int chunk_size = config->chunk_size;

  // 计算总迭代次数
  int total_iterations = (end - start + increment - 1) / increment;

  // 如果迭代次数少于线程数，减少线程数
  if (total_iterations < num_threads) {
    num_threads = (total_iterations > 0) ? total_iterations : 1;
  }

  // 分配线程和线程参数
  pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
  thread_work_t *works =
      (thread_work_t *)malloc(num_threads * sizeof(thread_work_t));

  if (!threads || !works) {
    fprintf(stderr, "parallel_for: 内存分配失败\n");
    free(threads);
    free(works);
    return -1;
  }

  switch (schedule) {
  case SCHEDULE_STATIC: {
    // 静态调度：预先平均分配任务
    int base_chunk = total_iterations / num_threads;
    int remainder = total_iterations % num_threads;

    int current_start = start;
    for (int t = 0; t < num_threads; t++) {
      // 计算当前线程的任务量（余数分配给前面的线程）
      int chunk = base_chunk + (t < remainder ? 1 : 0);
      int current_end = current_start + chunk * increment;

      works[t].start = current_start;
      works[t].end = current_end;
      works[t].increment = increment;
      works[t].functor = functor;
      works[t].arg = arg;
      works[t].thread_id = t;

      current_start = current_end;
    }

    // 创建线程
    for (int t = 0; t < num_threads; t++) {
      if (pthread_create(&threads[t], NULL, thread_work_static, &works[t]) !=
          0) {
        fprintf(stderr, "parallel_for: 线程 %d 创建失败\n", t);
        // 清理已创建的线程
        for (int j = 0; j < t; j++) {
          pthread_join(threads[j], NULL);
        }
        free(threads);
        free(works);
        return -1;
      }
    }

    // 等待所有线程完成
    for (int t = 0; t < num_threads; t++) {
      pthread_join(threads[t], NULL);
    }
    break;
  }

  case SCHEDULE_DYNAMIC: {
    // 动态调度：初始化全局变量
    if (chunk_size <= 0)
      chunk_size = 1;
    dynamic_next_start = start;
    dynamic_end = end;
    dynamic_increment = increment;
    dynamic_chunk_size = chunk_size;

    // 创建线程
    for (int t = 0; t < num_threads; t++) {
      works[t].start = start;
      works[t].end = end;
      works[t].increment = increment;
      works[t].functor = functor;
      works[t].arg = arg;
      works[t].thread_id = t;

      if (pthread_create(&threads[t], NULL, thread_work_dynamic, &works[t]) !=
          0) {
        fprintf(stderr, "parallel_for: 线程 %d 创建失败\n", t);
        for (int j = 0; j < t; j++) {
          pthread_join(threads[j], NULL);
        }
        free(threads);
        free(works);
        return -1;
      }
    }

    // 等待所有线程完成
    for (int t = 0; t < num_threads; t++) {
      pthread_join(threads[t], NULL);
    }
    break;
  }

  case SCHEDULE_GUIDED: {
    // 引导调度：初始化全局变量
    if (chunk_size <= 0)
      chunk_size = 1;
    guided_next_start = start;
    guided_end = end;
    guided_increment = increment;
    guided_remaining = total_iterations;
    guided_num_threads = num_threads;

    // 创建线程
    for (int t = 0; t < num_threads; t++) {
      works[t].start = start;
      works[t].end = end;
      works[t].increment = increment;
      works[t].functor = functor;
      works[t].arg = arg;
      works[t].thread_id = t;

      if (pthread_create(&threads[t], NULL, thread_work_guided, &works[t]) !=
          0) {
        fprintf(stderr, "parallel_for: 线程 %d 创建失败\n", t);
        for (int j = 0; j < t; j++) {
          pthread_join(threads[j], NULL);
        }
        free(threads);
        free(works);
        return -1;
      }
    }

    // 等待所有线程完成
    for (int t = 0; t < num_threads; t++) {
      pthread_join(threads[t], NULL);
    }
    break;
  }
  }

  free(threads);
  free(works);
  return 0;
}