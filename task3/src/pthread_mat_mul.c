/**
 * POSIX Pthreads 并行矩阵乘法
 *
 * 使用 POSIX pthreads 实现并行矩阵乘法 C = A × B
 * 支持不同的数据划分方式和线程数量
 * 可在 WSL (Windows Subsystem for Linux) 中运行
 *
 * 编译: gcc -O2 -o bin/pthread_mat_mul src/pthread_mat_mul.c -lpthread -lm
 * 运行: bin/pthread_mat_mul <m> <n> <k> [num_threads] [strategy]
 *       bin/pthread_mat_mul <size> [num_threads] [strategy]  (m=n=k=size)
 *
 * 策略:
 *   0 - 行划分 (默认)
 *   1 - 分块划分
 */

#define _GNU_SOURCE
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ==================== 全局变量 ==================== */

int M, N, K;         // 矩阵维度: A(M×N), B(N×K), C(M×K)
int num_threads = 4; // 线程数量
int strategy = 0;    // 0=行划分, 1=分块划分

double *A = NULL; // 矩阵 A (M×N)
double *B = NULL; // 矩阵 B (N×K)
double *C = NULL; // 结果矩阵 C (M×K)

/* ==================== 分块划分参数 ==================== */

int block_size = 64; // 分块大小

/* ==================== 线程参数结构体 ==================== */

typedef struct {
  int thread_id;
  int start_row; // 起始行
  int end_row;   // 结束行 (exclusive)
} ThreadParam;

/* ==================== 工具函数 ==================== */

double get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

double *alloc_matrix(int rows, int cols) {
  double *m = (double *)malloc((size_t)rows * cols * sizeof(double));
  if (!m) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return m;
}

void fill_random(double *mat, int rows, int cols) {
  for (int i = 0; i < rows * cols; i++) {
    mat[i] = (double)rand() / RAND_MAX;
  }
}

/* 串行矩阵乘法 C = A × B (ikj 顺序，缓存友好) */
void mat_mul_serial(double *A, double *B, double *C, int m, int n, int k) {
  for (int i = 0; i < m; i++)
    for (int j = 0; j < k; j++)
      C[i * k + j] = 0.0;
  for (int i = 0; i < m; i++)
    for (int p = 0; p < n; p++)
      for (int j = 0; j < k; j++)
        C[i * k + j] += A[i * n + p] * B[p * k + j];
}

/* ==================== 行划分线程函数 ==================== */

void *row_division_worker(void *arg) {
  ThreadParam *param = (ThreadParam *)arg;
  int start = param->start_row;
  int end = param->end_row;

  // 初始化 C 的指定行范围为 0
  for (int i = start; i < end; i++) {
    for (int j = 0; j < K; j++) {
      C[i * K + j] = 0.0;
    }
  }

  // 使用 ikj 循环顺序（缓存友好）
  for (int i = start; i < end; i++) {
    for (int p = 0; p < N; p++) {
      double a_val = A[i * N + p];
      for (int j = 0; j < K; j++) {
        C[i * K + j] += a_val * B[p * K + j];
      }
    }
  }

  return NULL;
}

/* ==================== 分块划分线程函数 ==================== */

void *block_division_worker(void *arg) {
  ThreadParam *param = (ThreadParam *)arg;
  int tid = param->thread_id;

  // 计算该线程负责的块范围
  int num_blocks_m = (M + block_size - 1) / block_size;
  int num_blocks_k = (K + block_size - 1) / block_size;
  int total_blocks = num_blocks_m * num_blocks_k;

  int blocks_per_thread = total_blocks / num_threads;
  int remainder = total_blocks % num_threads;

  int start_block =
      tid * blocks_per_thread + (tid < remainder ? tid : remainder);
  int end_block = start_block + blocks_per_thread + (tid < remainder ? 1 : 0);

  for (int b = start_block; b < end_block; b++) {
    int block_m = b / num_blocks_k;
    int block_k = b % num_blocks_k;

    int i_start = block_m * block_size;
    int i_end = (block_m == num_blocks_m - 1) ? M : (block_m + 1) * block_size;
    int j_start = block_k * block_size;
    int j_end = (block_k == num_blocks_k - 1) ? K : (block_k + 1) * block_size;

    // 初始化块为 0
    for (int i = i_start; i < i_end; i++) {
      for (int j = j_start; j < j_end; j++) {
        C[i * K + j] = 0.0;
      }
    }

    // 使用 ikj 循环顺序（缓存友好）
    for (int i = i_start; i < i_end; i++) {
      for (int p = 0; p < N; p++) {
        double a_val = A[i * N + p];
        for (int j = j_start; j < j_end; j++) {
          C[i * K + j] += a_val * B[p * K + j];
        }
      }
    }
  }

  return NULL;
}

/* ==================== 主函数 ==================== */

int main(int argc, char **argv) {
  /* 解析参数 */
  if (argc == 2) {
    // 只有一个参数: size (m=n=k=size)
    int size = atoi(argv[1]);
    if (size < 128 || size > 2048) {
      fprintf(stderr, "Error: matrix size must be in range [128, 2048]\n");
      return 1;
    }
    M = N = K = size;
  } else if (argc == 3) {
    // 两个参数: size + num_threads 或 m + n
    int val1 = atoi(argv[1]);
    int val2 = atoi(argv[2]);
    if (val1 >= 128 && val1 <= 2048 && val2 >= 1 && val2 <= 16) {
      // size + num_threads
      M = N = K = val1;
      num_threads = val2;
    } else if (val1 >= 128 && val1 <= 2048 && val2 >= 128 && val2 <= 2048) {
      // m + n (k 使用默认值或从后续参数获取)
      M = val1;
      N = val2;
      K = val1; // 默认 k = m
    } else {
      fprintf(stderr, "Error: invalid arguments\n");
      return 1;
    }
  } else if (argc == 4) {
    // 三个参数: size + num_threads + strategy 或 m + n + k
    int val1 = atoi(argv[1]);
    int val2 = atoi(argv[2]);
    int val3 = atoi(argv[3]);
    if (val1 >= 128 && val1 <= 2048 && val2 >= 1 && val2 <= 16 && val3 >= 0 &&
        val3 <= 1) {
      // size + num_threads + strategy
      M = N = K = val1;
      num_threads = val2;
      strategy = val3;
    } else if (val1 >= 128 && val1 <= 2048 && val2 >= 128 && val2 <= 2048 &&
               val3 >= 128 && val3 <= 2048) {
      // m + n + k
      M = val1;
      N = val2;
      K = val3;
    } else {
      fprintf(stderr, "Error: invalid arguments\n");
      return 1;
    }
  } else if (argc >= 5) {
    // 四个或以上参数: m + n + k + num_threads + strategy
    M = atoi(argv[1]);
    N = atoi(argv[2]);
    K = atoi(argv[3]);
    if (M < 128 || M > 2048 || N < 128 || N > 2048 || K < 128 || K > 2048) {
      fprintf(stderr, "Error: m, n, k must be in range [128, 2048]\n");
      return 1;
    }
    num_threads = atoi(argv[4]);
    if (argc >= 6) {
      strategy = atoi(argv[5]);
    }
  } else {
    fprintf(stderr, "Usage: pthread_mat_mul <size> [num_threads] [strategy]\n");
    fprintf(stderr, "   or: pthread_mat_mul <m> <n> <k> [num_threads] "
                    "[strategy]\n");
    fprintf(stderr,
            "   strategy: 0=row division (default), 1=block division\n");
    return 1;
  }

  // 可选参数
  if (argc >= 5) {
    num_threads = atoi(argv[4]);
    if (num_threads < 1 || num_threads > 16) {
      fprintf(stderr, "Warning: num_threads out of range [1, 16], using 4\n");
      num_threads = 4;
    }
  }
  if (argc >= 6) {
    strategy = atoi(argv[5]);
  }

  // 初始化随机数种子
  srand(42);

  /* 分配矩阵 */
  A = alloc_matrix(M, N);
  B = alloc_matrix(N, K);
  C = alloc_matrix(M, K);

  /* 填充随机数据 */
  fill_random(A, M, N);
  fill_random(B, N, K);

  printf("Matrix size: A(%d x %d) * B(%d x %d) = C(%d x %d)\n", M, N, N, K, M,
         K);
  printf("Threads: %d\n", num_threads);
  printf("Strategy: %s\n", strategy == 0 ? "Row Division" : "Block Division");
  if (strategy == 1) {
    printf("Block size: %d\n", block_size);
  }

  /* ===== 串行版本计时 ===== */
  double *C_serial = alloc_matrix(M, K);
  double t_serial_start = get_time();
  mat_mul_serial(A, B, C_serial, M, N, K);
  double t_serial_end = get_time();
  double serial_time = t_serial_end - t_serial_start;
  printf("Serial Time:  %.6f seconds\n", serial_time);

  /* ===== 并行版本计时 ===== */
  pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
  ThreadParam *params =
      (ThreadParam *)malloc(num_threads * sizeof(ThreadParam));

  double t_parallel_start = get_time();

  if (strategy == 0) {
    /* 行划分 */
    int rows_per_thread = M / num_threads;
    int remainder = M % num_threads;

    for (int t = 0; t < num_threads; t++) {
      params[t].thread_id = t;
      params[t].start_row =
          t * rows_per_thread + (t < remainder ? t : remainder);
      params[t].end_row =
          params[t].start_row + rows_per_thread + (t < remainder ? 1 : 0);
      pthread_create(&threads[t], NULL, row_division_worker, &params[t]);
    }
  } else {
    /* 分块划分 */
    for (int t = 0; t < num_threads; t++) {
      params[t].thread_id = t;
      pthread_create(&threads[t], NULL, block_division_worker, &params[t]);
    }
  }

  // 等待所有线程完成
  for (int t = 0; t < num_threads; t++) {
    pthread_join(threads[t], NULL);
  }

  double t_parallel_end = get_time();
  double parallel_time = t_parallel_end - t_parallel_start;
  printf("Parallel Time: %.6f seconds\n", parallel_time);

  /* 验证正确性 */
  if (M <= 512 && N <= 512 && K <= 512) {
    double max_err = 0.0;
    for (int i = 0; i < M * K; i++) {
      double err = fabs(C[i] - C_serial[i]);
      if (err > max_err)
        max_err = err;
    }
    if (max_err < 1e-8)
      printf("Verification: PASSED (max error = %.2e)\n", max_err);
    else
      printf("Verification: FAILED (max error = %.2e)\n", max_err);
  }

  /* 性能分析 */
  double speedup = serial_time / parallel_time;
  double efficiency = speedup / num_threads;
  printf("Speedup:      %.2fx\n", speedup);
  printf("Efficiency:   %.2f%%\n", efficiency * 100);

  /* 释放资源 */
  free(threads);
  free(params);
  free(A);
  free(B);
  free(C);
  free(C_serial);

  return 0;
}