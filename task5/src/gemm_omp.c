/**
 * @file gemm_omp.c
 * @brief 基于 OpenMP 的通用矩阵乘法（GEMM）并行实现
 *
 * 功能：C = A × B
 * 其中 A 是 M×N 矩阵，B 是 N×K 矩阵，C 是 M×K 矩阵
 *
 * 编译：gcc -fopenmp -O2 -o bin/gemm_omp src/gemm_omp.c -lm
 * 运行：./bin/gemm_omp <M> <N> <K> <num_threads>
 */

#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


/**
 * @brief 初始化矩阵（随机生成）
 * @param mat 矩阵指针
 * @param rows 行数
 * @param cols 列数
 */
void init_matrix(float *mat, int rows, int cols) {
  for (int i = 0; i < rows * cols; i++) {
    mat[i] = (float)(rand() % 100) / 10.0f;
  }
}

/**
 * @brief 串行矩阵乘法（用于验证）
 * @param A 输入矩阵 A (M×N)
 * @param B 输入矩阵 B (N×K)
 * @param C 输出矩阵 C (M×K)
 * @param M 行数
 * @param N 内维
 * @param K 列数
 */
void gemm_serial(float *A, float *B, float *C, int M, int N, int K) {
  // 初始化 C 为零矩阵
  for (int i = 0; i < M * K; i++) {
    C[i] = 0.0f;
  }

  // 串行矩阵乘法
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < K; j++) {
      float sum = 0.0f;
      for (int p = 0; p < N; p++) {
        sum += A[i * N + p] * B[p * K + j];
      }
      C[i * K + j] = sum;
    }
  }
}

/**
 * @brief OpenMP 并行矩阵乘法 - 默认调度
 * @param A 输入矩阵 A (M×N)
 * @param B 输入矩阵 B (N×K)
 * @param C 输出矩阵 C (M×K)
 * @param M 行数
 * @param N 内维
 * @param K 列数
 * @param num_threads 线程数
 */
void gemm_omp_default(float *A, float *B, float *C, int M, int N, int K,
                      int num_threads) {
  // 初始化 C 为零矩阵
#pragma omp parallel for num_threads(num_threads)
  for (int i = 0; i < M * K; i++) {
    C[i] = 0.0f;
  }

  // OpenMP 并行矩阵乘法（默认调度）
#pragma omp parallel for num_threads(num_threads) collapse(2)
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < K; j++) {
      float sum = 0.0f;
      for (int p = 0; p < N; p++) {
        sum += A[i * N + p] * B[p * K + j];
      }
      C[i * K + j] = sum;
    }
  }
}

/**
 * @brief OpenMP 并行矩阵乘法 - 静态调度 schedule(static, 1)
 * @param A 输入矩阵 A (M×N)
 * @param B 输入矩阵 B (N×K)
 * @param C 输出矩阵 C (M×K)
 * @param M 行数
 * @param N 内维
 * @param K 列数
 * @param num_threads 线程数
 */
void gemm_omp_static(float *A, float *B, float *C, int M, int N, int K,
                     int num_threads) {
  // 初始化 C 为零矩阵
#pragma omp parallel for num_threads(num_threads) schedule(static, 1)
  for (int i = 0; i < M * K; i++) {
    C[i] = 0.0f;
  }

  // OpenMP 并行矩阵乘法（静态调度）
#pragma omp parallel for num_threads(num_threads) schedule(static, 1)          \
    collapse(2)
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < K; j++) {
      float sum = 0.0f;
      for (int p = 0; p < N; p++) {
        sum += A[i * N + p] * B[p * K + j];
      }
      C[i * K + j] = sum;
    }
  }
}

/**
 * @brief OpenMP 并行矩阵乘法 - 动态调度 schedule(dynamic, 1)
 * @param A 输入矩阵 A (M×N)
 * @param B 输入矩阵 B (N×K)
 * @param C 输出矩阵 C (M×K)
 * @param M 行数
 * @param N 内维
 * @param K 列数
 * @param num_threads 线程数
 */
void gemm_omp_dynamic(float *A, float *B, float *C, int M, int N, int K,
                      int num_threads) {
  // 初始化 C 为零矩阵
#pragma omp parallel for num_threads(num_threads) schedule(dynamic, 1)
  for (int i = 0; i < M * K; i++) {
    C[i] = 0.0f;
  }

  // OpenMP 并行矩阵乘法（动态调度）
#pragma omp parallel for num_threads(num_threads) schedule(dynamic, 1)         \
    collapse(2)
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < K; j++) {
      float sum = 0.0f;
      for (int p = 0; p < N; p++) {
        sum += A[i * N + p] * B[p * K + j];
      }
      C[i * K + j] = sum;
    }
  }
}

/**
 * @brief 验证矩阵乘法结果
 * @param C_serial 串行结果
 * @param C_parallel 并行结果
 * @param M 行数
 * @param K 列数
 * @param tolerance 容差
 * @return 0 表示验证通过，-1 表示验证失败
 */
int verify_result(float *C_serial, float *C_parallel, int M, int K,
                  float tolerance) {
  for (int i = 0; i < M * K; i++) {
    float diff = C_serial[i] - C_parallel[i];
    if (diff < 0)
      diff = -diff;
    if (diff > tolerance) {
      printf("验证失败: C[%d] = %f, 期望 %f, 差值 %f\n", i, C_parallel[i],
             C_serial[i], diff);
      return -1;
    }
  }
  return 0;
}

/**
 * @brief 打印矩阵（仅用于小规模矩阵）
 * @param mat 矩阵指针
 * @param rows 行数
 * @param cols 列数
 * @param name 矩阵名称
 */
void print_matrix(float *mat, int rows, int cols, const char *name) {
  printf("\n矩阵 %s (%d×%d):\n", name, rows, cols);
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
      printf("%8.2f ", mat[i * cols + j]);
    }
    printf("\n");
  }
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf("用法: %s <M> <N> <K> [num_threads]\n", argv[0]);
    printf("  M, N, K: 矩阵维度 (512~2048)\n");
    printf("  num_threads: 线程数 (默认: 4)\n");
    return 1;
  }

  int M = atoi(argv[1]);
  int N = atoi(argv[2]);
  int K = atoi(argv[3]);
  int num_threads = (argc > 4) ? atoi(argv[4]) : 4;

  // 验证维度范围
  if (M < 512 || M > 2048 || N < 512 || N > 2048 || K < 512 || K > 2048) {
    printf("警告: 矩阵维度建议在 512~2048 范围内\n");
  }

  printf("========================================\n");
  printf("  OpenMP 通用矩阵乘法 (GEMM)\n");
  printf("========================================\n");
  printf("矩阵规模: A(%d×%d) × B(%d×%d) = C(%d×%d)\n", M, N, N, K, M, K);
  printf("线程数: %d\n", num_threads);
  printf("========================================\n");

  // 分配内存
  float *A = (float *)malloc(M * N * sizeof(float));
  float *B = (float *)malloc(N * K * sizeof(float));
  float *C_serial = (float *)malloc(M * K * sizeof(float));
  float *C_omp_default = (float *)malloc(M * K * sizeof(float));
  float *C_omp_static = (float *)malloc(M * K * sizeof(float));
  float *C_omp_dynamic = (float *)malloc(M * K * sizeof(float));

  if (!A || !B || !C_serial || !C_omp_default || !C_omp_static ||
      !C_omp_dynamic) {
    printf("内存分配失败!\n");
    return 1;
  }

  // 初始化随机种子
  srand(time(NULL));

  // 初始化矩阵
  printf("\n初始化矩阵...\n");
  init_matrix(A, M, N);
  init_matrix(B, N, K);

  // 小规模矩阵打印
  if (M <= 8 && K <= 8) {
    print_matrix(A, M, N, "A");
    print_matrix(B, N, K, "B");
  }

  double start_time, end_time;

  // 1. 串行矩阵乘法（验证用）
  printf("\n[1] 串行矩阵乘法...\n");
  start_time = omp_get_wtime();
  gemm_serial(A, B, C_serial, M, N, K);
  end_time = omp_get_wtime();
  double serial_time = end_time - start_time;
  printf("    串行时间: %.6f 秒\n", serial_time);

  // 2. OpenMP 默认调度
  printf("\n[2] OpenMP 默认调度...\n");
  start_time = omp_get_wtime();
  gemm_omp_default(A, B, C_omp_default, M, N, K, num_threads);
  end_time = omp_get_wtime();
  double default_time = end_time - start_time;
  printf("    默认调度时间: %.6f 秒\n", default_time);
  printf("    加速比: %.4fx\n", serial_time / default_time);

  // 验证默认调度结果
  if (verify_result(C_serial, C_omp_default, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 3. OpenMP 静态调度
  printf("\n[3] OpenMP 静态调度 (static, 1)...\n");
  start_time = omp_get_wtime();
  gemm_omp_static(A, B, C_omp_static, M, N, K, num_threads);
  end_time = omp_get_wtime();
  double static_time = end_time - start_time;
  printf("    静态调度时间: %.6f 秒\n", static_time);
  printf("    加速比: %.4fx\n", serial_time / static_time);

  // 验证静态调度结果
  if (verify_result(C_serial, C_omp_static, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 4. OpenMP 动态调度
  printf("\n[4] OpenMP 动态调度 (dynamic, 1)...\n");
  start_time = omp_get_wtime();
  gemm_omp_dynamic(A, B, C_omp_dynamic, M, N, K, num_threads);
  end_time = omp_get_wtime();
  double dynamic_time = end_time - start_time;
  printf("    动态调度时间: %.6f 秒\n", dynamic_time);
  printf("    加速比: %.4fx\n", serial_time / dynamic_time);

  // 验证动态调度结果
  if (verify_result(C_serial, C_omp_dynamic, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 性能总结
  printf("\n========================================\n");
  printf("  性能总结\n");
  printf("========================================\n");
  printf("调度方式          | 时间 (秒)   | 加速比\n");
  printf("------------------|-------------|--------\n");
  printf("串行              | %-11.6f | 1.0000x\n", serial_time);
  printf("OpenMP 默认       | %-11.6f | %.4fx\n", default_time,
         serial_time / default_time);
  printf("OpenMP 静态       | %-11.6f | %.4fx\n", static_time,
         serial_time / static_time);
  printf("OpenMP 动态       | %-11.6f | %.4fx\n", dynamic_time,
         serial_time / dynamic_time);
  printf("========================================\n");

  // 小规模矩阵打印结果
  if (M <= 8 && K <= 8) {
    print_matrix(C_serial, M, K, "C (串行)");
    print_matrix(C_omp_default, M, K, "C (OpenMP 默认)");
  }

  // 释放内存
  free(A);
  free(B);
  free(C_serial);
  free(C_omp_default);
  free(C_omp_static);
  free(C_omp_dynamic);

  printf("\n完成!\n");
  return 0;
}