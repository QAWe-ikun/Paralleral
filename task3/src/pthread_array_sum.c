/**
 * POSIX Pthreads 并行数组求和
 *
 * 使用 POSIX pthreads 实现并行数组求和 s = ΣA[i]
 * 支持不同的聚合方式
 * 可在 WSL (Windows Subsystem for Linux) 中运行
 *
 * 编译: gcc -O2 -o bin/pthread_array_sum src/pthread_array_sum.c -lpthread
 * 运行: bin/pthread_array_sum <n> [num_threads] [method]
 *
 * method:
 *   0 - 直接累加 (默认)
 *   1 - 树形聚合
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 全局变量 ==================== */

long long N;         // 数组长度
int num_threads = 4; // 线程数量
int method = 0;      // 0=直接累加, 1=树形聚合

long long *A = NULL;                                   // 数组 A
long long global_sum = 0;                              // 全局和
pthread_mutex_t sum_mutex = PTHREAD_MUTEX_INITIALIZER; // 互斥锁

/* ==================== 线程参数结构体 ==================== */

typedef struct {
  int thread_id;
  long long start_idx; // 起始索引
  long long end_idx;   // 结束索引 (exclusive)
  long long local_sum; // 局部和
} ThreadParam;

/* ==================== 工具函数 ==================== */

double get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ==================== 直接累加线程函数 ==================== */

void *direct_sum_worker(void *arg) {
  ThreadParam *param = (ThreadParam *)arg;
  long long start = param->start_idx;
  long long end = param->end_idx;
  long long sum = 0;

  // 计算局部和
  for (long long i = start; i < end; i++) {
    sum += A[i];
  }

  param->local_sum = sum;

  // 使用互斥锁保护全局和
  pthread_mutex_lock(&sum_mutex);
  global_sum += sum;
  pthread_mutex_unlock(&sum_mutex);

  return NULL;
}

/* ==================== 树形聚合线程函数 ==================== */

// 用于存储每个线程的局部和
long long *thread_sums = NULL;

void *tree_sum_worker(void *arg) {
  ThreadParam *param = (ThreadParam *)arg;
  int tid = param->thread_id;
  long long start = param->start_idx;
  long long end = param->end_idx;
  long long sum = 0;

  // 计算局部和
  for (long long i = start; i < end; i++) {
    sum += A[i];
  }

  param->local_sum = sum;

  // 存储到线程专属位置（无需锁，各线程写入不同位置）
  thread_sums[tid] = sum;

  return NULL;
}

/* 树形聚合：在主线程中合并各线程的局部和 */
long long tree_aggregate(int nthreads) {
  long long sum = 0;
  for (int i = 0; i < nthreads; i++) {
    sum += thread_sums[i];
  }
  return sum;
}

/* ==================== 主函数 ==================== */

int main(int argc, char **argv) {
  /* 解析参数 */
  if (argc < 2) {
    fprintf(stderr, "Usage: pthread_array_sum <n> [num_threads] [method]\n");
    fprintf(stderr, "   n: array size in range [1M, 128M]\n");
    fprintf(stderr, "   method: 0=direct sum (default), 1=tree aggregation\n");
    return 1;
  }

  N = atoll(argv[1]);
  if (N < 1000000 || N > 128000000) {
    fprintf(stderr, "Error: n must be in range [1M, 128M]\n");
    return 1;
  }

  // 可选参数
  if (argc >= 3) {
    num_threads = atoi(argv[2]);
    if (num_threads < 1 || num_threads > 16) {
      fprintf(stderr, "Warning: num_threads out of range [1, 16], using 4\n");
      num_threads = 4;
    }
  }
  if (argc >= 4) {
    method = atoi(argv[3]);
  }

  /* 分配数组 */
  A = (long long *)malloc((size_t)N * sizeof(long long));
  if (!A) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  /* 填充随机数据 */
  srand(42);
  for (long long i = 0; i < N; i++) {
    A[i] = rand() % 1000;
  }

  printf("Array size: %lld\n", N);
  printf("Threads: %d\n", num_threads);
  printf("Method: %s\n", method == 0 ? "Direct Sum" : "Tree Aggregation");

  /* ===== 串行版本计时 ===== */
  double t_serial_start = get_time();
  long long serial_sum = 0;
  for (long long i = 0; i < N; i++) {
    serial_sum += A[i];
  }
  double t_serial_end = get_time();
  double serial_time = t_serial_end - t_serial_start;
  printf("Serial Time:  %.6f seconds\n", serial_time);
  printf("Serial Sum:   %lld\n", serial_sum);

  /* ===== 并行版本计时 ===== */
  pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
  ThreadParam *params =
      (ThreadParam *)malloc(num_threads * sizeof(ThreadParam));

  if (method == 1) {
    thread_sums = (long long *)malloc(num_threads * sizeof(long long));
  }

  global_sum = 0;
  double t_parallel_start = get_time();

  /* 计算每个线程的工作范围 */
  long long elements_per_thread = N / num_threads;
  long long remainder = N % num_threads;

  for (int t = 0; t < num_threads; t++) {
    params[t].thread_id = t;
    params[t].start_idx =
        t * elements_per_thread + (t < remainder ? t : remainder);
    params[t].end_idx =
        params[t].start_idx + elements_per_thread + (t < remainder ? 1 : 0);

    if (method == 0) {
      pthread_create(&threads[t], NULL, direct_sum_worker, &params[t]);
    } else {
      pthread_create(&threads[t], NULL, tree_sum_worker, &params[t]);
    }
  }

  // 等待所有线程完成
  for (int t = 0; t < num_threads; t++) {
    pthread_join(threads[t], NULL);
  }

  double t_parallel_end = get_time();
  double parallel_time = t_parallel_end - t_parallel_start;

  // 计算最终结果
  long long parallel_sum;
  if (method == 0) {
    parallel_sum = global_sum;
  } else {
    parallel_sum = tree_aggregate(num_threads);
  }

  printf("Parallel Time: %.6f seconds\n", parallel_time);
  printf("Parallel Sum:  %lld\n", parallel_sum);

  /* 验证正确性 */
  if (parallel_sum == serial_sum) {
    printf("Verification: PASSED\n");
  } else {
    printf("Verification: FAILED (serial=%lld, parallel=%lld)\n", serial_sum,
           parallel_sum);
  }

  /* 性能分析 */
  double speedup = serial_time / parallel_time;
  double efficiency = speedup / num_threads;
  printf("Speedup:      %.2fx\n", speedup);
  printf("Efficiency:   %.2f%%\n", efficiency * 100);

  /* 释放资源 */
  free(threads);
  free(params);
  if (method == 1) {
    free(thread_sums);
  }
  free(A);
  pthread_mutex_destroy(&sum_mutex);

  return 0;
}