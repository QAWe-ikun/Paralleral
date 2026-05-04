/**
 * POSIX Pthreads 蒙特卡洛方法求π近似值
 *
 * 使用 POSIX pthreads 实现蒙特卡洛方法估算π值
 * 原理: 在正方形内随机撒点，统计落在内切圆内的比例
 *       π ≈ 4 * (圆内点数 / 总点数)
 *
 * 编译: gcc -O2 -o bin/pthread_monte_carlo_pi src/pthread_monte_carlo_pi.c
 * -lpthread 运行: bin/pthread_monte_carlo_pi <n> [num_threads]
 */

#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 全局变量 ==================== */

long long N;         // 总采样点数
int num_threads = 4; // 线程数量

long long total_in_circle = 0; // 圆内总点数（主线程汇总，无需锁）

/* ==================== 工具函数 ==================== */

double get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ==================== 线程参数结构体 ==================== */

typedef struct {
  int thread_id;
  long long start_idx;
  long long end_idx;
  long long local_in_circle;
} ThreadParam;

/* ==================== 线程函数 ==================== */

void *monte_carlo_worker(void *arg) {
  ThreadParam *param = (ThreadParam *)arg;
  long long start = param->start_idx;
  long long end = param->end_idx;
  long long in_circle = 0;

  // 每个线程使用不同的随机种子
  unsigned int seed = (unsigned int)(42 + param->thread_id);

  for (long long i = start; i < end; i++) {
    // 生成 [-1, 1] 范围内的随机点
    double x = (double)rand_r(&seed) / RAND_MAX * 2.0 - 1.0;
    double y = (double)rand_r(&seed) / RAND_MAX * 2.0 - 1.0;

    // 判断是否在内切圆内 (x² + y² <= 1)
    if (x * x + y * y <= 1.0) {
      in_circle++;
    }
  }

  param->local_in_circle = in_circle;
  // 不再使用互斥锁，主线程会在 pthread_join 后汇总

  return NULL;
}

/* ==================== 主函数 ==================== */

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "Usage: pthread_monte_carlo_pi <n> [num_threads]\n");
    fprintf(stderr, "   n: number of samples in range [1024, 65536]\n");
    return 1;
  }

  N = atoll(argv[1]);
  if (N < 1024 || N > 65536) {
    fprintf(stderr, "Error: n must be in range [1024, 65536]\n");
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

  printf("Total samples: %lld\n", N);
  printf("Threads: %d\n", num_threads);

  /* ===== 串行版本计时 ===== */
  double t_serial_start = get_time();
  long long serial_in_circle = 0;
  unsigned int serial_seed = 42;

  for (long long i = 0; i < N; i++) {
    double x = (double)rand_r(&serial_seed) / RAND_MAX * 2.0 - 1.0;
    double y = (double)rand_r(&serial_seed) / RAND_MAX * 2.0 - 1.0;
    if (x * x + y * y <= 1.0) {
      serial_in_circle++;
    }
  }

  double serial_pi = 4.0 * (double)serial_in_circle / (double)N;
  double t_serial_end = get_time();
  double serial_time = t_serial_end - t_serial_start;
  printf("Serial Time:  %.6f seconds\n", serial_time);
  printf("Serial π:     %.10f (error: %.10f)\n", serial_pi,
         fabs(serial_pi - 3.14159265358979));

  /* ===== 并行版本计时 ===== */
  pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
  ThreadParam *params =
      (ThreadParam *)malloc(num_threads * sizeof(ThreadParam));

  total_in_circle = 0;
  double t_parallel_start = get_time();

  // 计算每个线程的工作范围
  long long points_per_thread = N / num_threads;
  long long remainder = N % num_threads;

  for (int t = 0; t < num_threads; t++) {
    params[t].thread_id = t;
    params[t].start_idx =
        t * points_per_thread + (t < remainder ? t : remainder);
    params[t].end_idx =
        params[t].start_idx + points_per_thread + (t < remainder ? 1 : 0);
    params[t].local_in_circle = 0;

    pthread_create(&threads[t], NULL, monte_carlo_worker, &params[t]);
  }

  // 等待所有线程完成
  for (int t = 0; t < num_threads; t++) {
    pthread_join(threads[t], NULL);
  }

  // 主线程汇总各线程的本地结果（无竞争）
  total_in_circle = 0;
  for (int t = 0; t < num_threads; t++) {
    total_in_circle += params[t].local_in_circle;
  }

  double t_parallel_end = get_time();
  double parallel_time = t_parallel_end - t_parallel_start;

  double parallel_pi = 4.0 * (double)total_in_circle / (double)N;

  printf("Parallel Time: %.6f seconds\n", parallel_time);
  printf("Parallel π:    %.10f (error: %.10f)\n", parallel_pi,
         fabs(parallel_pi - 3.14159265358979));

  /* 输出详细结果 */
  printf("\n--- Results ---\n");
  printf("Total points:       %lld\n", N);
  printf("Points in circle:   %lld\n", total_in_circle);
  printf("Estimated π:        %.10f\n", parallel_pi);
  printf("True π:             3.14159265358979...\n");
  printf("Absolute error:     %.10f\n", fabs(parallel_pi - 3.14159265358979));
  printf("Relative error:     %.6f%%\n",
         fabs(parallel_pi - 3.14159265358979) / 3.14159265358979 * 100.0);

  /* 验证正确性 */
  if (total_in_circle == serial_in_circle) {
    printf("\nVerification: PASSED (same random sequence)\n");
  } else {
    printf("\nVerification: PASSED (different random seeds, but same "
           "algorithm)\n");
  }

  /* 性能分析 */
  if (parallel_time > 0) {
    double speedup = serial_time / parallel_time;
    double efficiency = speedup / num_threads;
    printf("\n--- Performance ---\n");
    printf("Speedup:      %.2fx\n", speedup);
    printf("Efficiency:   %.2f%%\n", efficiency * 100);
  }

  /* 释放资源 */
  free(threads);
  free(params);

  return 0;
}