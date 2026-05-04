/**
 * POSIX Pthreads 一元二次方程求解
 *
 * 使用 POSIX pthreads 和条件变量并行求解一元二次方程 ax^2 + bx + c = 0
 * 求根公式: x = (-b ± √(b²-4ac)) / 2a
 *
 * 线程分工：
 *   线程1: 计算判别式 Δ = b² - 4ac
 *   线程2: 计算 -b
 *   线程3: 计算 2a
 *   主线程: 等待所有中间结果，计算最终解
 *
 * 编译: gcc -O2 -o bin/pthread_quadratic src/pthread_quadratic.c -lpthread -lm
 * 运行: bin/pthread_quadratic <a> <b> <c>
 */

#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* ==================== 全局变量 ==================== */

double A, B, C;     // 方程系数
double delta = 0.0; // 判别式 Δ = b² - 4ac
double neg_b = 0.0; // -b
double two_a = 0.0; // 2a

/* 条件变量和互斥锁 */
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int completed_tasks = 0; // 已完成的任务数

/* ==================== 工具函数 ==================== */

double get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ==================== 线程函数 ==================== */

/* 线程1: 计算判别式 Δ = b² - 4ac */
void *compute_delta(void *arg) {
  (void)arg;
  delta = B * B - 4.0 * A * C;

  pthread_mutex_lock(&mutex);
  completed_tasks++;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  return NULL;
}

/* 线程2: 计算 -b */
void *compute_neg_b(void *arg) {
  (void)arg;
  neg_b = -B;

  pthread_mutex_lock(&mutex);
  completed_tasks++;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  return NULL;
}

/* 线程3: 计算 2a */
void *compute_two_a(void *arg) {
  (void)arg;
  two_a = 2.0 * A;

  pthread_mutex_lock(&mutex);
  completed_tasks++;
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  return NULL;
}

/* ==================== 主函数 ==================== */

int main(int argc, char **argv) {
  if (argc != 4) {
    fprintf(stderr, "Usage: pthread_quadratic <a> <b> <c>\n");
    fprintf(stderr, "   Solves: ax^2 + bx + c = 0\n");
    fprintf(stderr, "   a, b, c in range [-100, 100]\n");
    return 1;
  }

  A = atof(argv[1]);
  B = atof(argv[2]);
  C = atof(argv[3]);

  if (A < -100 || A > 100 || B < -100 || B > 100 || C < -100 || C > 100) {
    fprintf(stderr, "Error: a, b, c must be in range [-100, 100]\n");
    return 1;
  }

  if (A == 0.0) {
    fprintf(stderr, "Error: a cannot be 0 (not a quadratic equation)\n");
    return 1;
  }

  printf("Equation: %.2f*x^2 + %.2f*x + %.2f = 0\n", A, B, C);
  printf("Threads: 3 (delta, -b, 2a) + 1 (main)\n");

  /* ===== 串行版本计时 ===== */
  double t_serial_start = get_time();
  double serial_delta = B * B - 4.0 * A * C;
  double serial_neg_b = -B;
  double serial_two_a = 2.0 * A;
  double x1_serial, x2_serial;
  int has_real_roots_serial = (serial_delta >= 0);

  if (has_real_roots_serial) {
    x1_serial = (serial_neg_b + sqrt(serial_delta)) / serial_two_a;
    x2_serial = (serial_neg_b - sqrt(serial_delta)) / serial_two_a;
  }
  double t_serial_end = get_time();
  double serial_time = t_serial_end - t_serial_start;
  printf("Serial Time:  %.6f seconds\n", serial_time);

  /* ===== 并行版本计时 ===== */
  pthread_t thread_delta, thread_neg_b, thread_two_a;
  completed_tasks = 0;

  double t_parallel_start = get_time();

  // 创建三个工作线程
  pthread_create(&thread_delta, NULL, compute_delta, NULL);
  pthread_create(&thread_neg_b, NULL, compute_neg_b, NULL);
  pthread_create(&thread_two_a, NULL, compute_two_a, NULL);

  // 主线程等待所有中间结果完成
  pthread_mutex_lock(&mutex);
  while (completed_tasks < 3) {
    pthread_cond_wait(&cond, &mutex);
  }
  pthread_mutex_unlock(&mutex);

  // 计算最终解
  double x1, x2;
  int has_real_roots = (delta >= 0);

  if (has_real_roots) {
    x1 = (neg_b + sqrt(delta)) / two_a;
    x2 = (neg_b - sqrt(delta)) / two_a;
  }

  double t_parallel_end = get_time();
  double parallel_time = t_parallel_end - t_parallel_start;

  // 等待所有线程结束
  pthread_join(thread_delta, NULL);
  pthread_join(thread_neg_b, NULL);
  pthread_join(thread_two_a, NULL);

  printf("Parallel Time: %.6f seconds\n", parallel_time);

  /* 输出结果 */
  printf("\n--- Results ---\n");
  printf("Discriminant (Δ): %.6f\n", delta);

  if (has_real_roots) {
    if (delta == 0.0) {
      printf("One real root (double root):\n");
      printf("  x1 = x2 = %.6f\n", x1);
    } else {
      printf("Two distinct real roots:\n");
      printf("  x1 = %.6f\n", x1);
      printf("  x2 = %.6f\n", x2);
    }
  } else {
    printf("No real roots (complex roots)\n");
    double real_part = neg_b / two_a;
    double imag_part = sqrt(-delta) / two_a;
    printf("  x1 = %.6f + %.6fi\n", real_part, imag_part);
    printf("  x2 = %.6f - %.6fi\n", real_part, imag_part);
  }

  /* 验证正确性 */
  if (has_real_roots && has_real_roots_serial) {
    double err1 = fabs(x1 - x1_serial);
    double err2 = fabs(x2 - x2_serial);
    double max_err = (err1 > err2) ? err1 : err2;
    if (max_err < 1e-10)
      printf("\nVerification: PASSED (max error = %.2e)\n", max_err);
    else
      printf("\nVerification: FAILED (max error = %.2e)\n", max_err);
  } else if (has_real_roots == has_real_roots_serial) {
    printf("\nVerification: PASSED (both have no real roots)\n");
  } else {
    printf("\nVerification: FAILED (root type mismatch)\n");
  }

  /* 性能分析 */
  if (parallel_time > 0) {
    double speedup = serial_time / parallel_time;
    printf("Speedup:      %.2fx\n", speedup);
  }

  /* 释放资源 */
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);

  return 0;
}