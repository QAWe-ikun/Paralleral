/**
 * @file gemm_pthreads.c
 * @brief 基于 parallel_for 动态链接库的矩阵乘法
 *
 * 使用 parallel_for 函数实现矩阵乘法 C = A × B
 * 演示如何将串行矩阵乘法改造为基于 parallel_for 的并行版本
 *
 * 编译：
 *   1. 编译动态链接库：gcc -shared -fPIC -o lib/libparallel_for.so
 * src/parallel_for.c -lpthread
 *   2. 编译主程序：gcc -O2 -o bin/gemm_pthreads src/gemm_pthreads.c -Llib
 * -lparallel_for -lpthread -lm
 *   3. 设置库路径：export LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH
 * 运行：./bin/gemm_pthreads <M> <N> <K> <num_threads>
 */

#include "parallel_for.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/**
 * @brief 矩阵乘法 functor 参数结构体
 */
typedef struct {
  float *A;  ///< 矩阵 A (M×N)
  float *B;  ///< 矩阵 B (N×K)
  float *Bt; ///< 矩阵 B 的转置 (K×N)
  float *C;  ///< 矩阵 C (M×K)
  int N;     ///< 内维
  int K;     ///< 列数
} gemm_args_t;

/**
 * @brief 转置矩阵 B
 *
 * Bt[j][p] = B[p][j]
 * 转置后，Bt 的行优先访问等价于 B 的列优先访问
 */
void transpose_B(float *B, float *Bt, int N, int K) {
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < K; j++) {
      Bt[j * N + i] = B[i * K + j];
    }
  }
}

/**
 * @brief 矩阵乘法 functor 函数（优化版：使用 B 转置）
 *
 * 计算矩阵 C 的第 idx 行
 * C[idx, :] = A[idx, :] × B
 *
 * 优化：使用 B 的转置 Bt，将 B[p][j] 的列访问转换为 Bt[j][p] 的行访问
 * 这样在内层循环中，Bt[j][p] 是连续内存访问，缓存命中率更高
 */
void *gemm_functor(int idx, void *args) {
  gemm_args_t *gemm_args = (gemm_args_t *)args;
  float *A = gemm_args->A;
  float *Bt = gemm_args->Bt; // 使用转置后的 B
  float *C = gemm_args->C;
  int N = gemm_args->N;
  int K = gemm_args->K;

  // 计算 C 的第 idx 行
  for (int j = 0; j < K; j++) {
    float sum = 0.0f;
    // 使用 Bt[j][p] 代替 B[p][j]，实现连续内存访问
    for (int p = 0; p < N; p++) {
      sum += A[idx * N + p] * Bt[j * N + p];
    }
    C[idx * K + j] = sum;
  }

  return NULL;
}

/**
 * @brief 初始化矩阵（随机生成）
 */
void init_matrix(float *mat, int rows, int cols) {
  for (int i = 0; i < rows * cols; i++) {
    mat[i] = (float)(rand() % 100) / 10.0f;
  }
}

/**
 * @brief 串行矩阵乘法（用于验证）
 */
void gemm_serial(float *A, float *B, float *C, int M, int N, int K) {
  // 初始化 C 为零矩阵
  memset(C, 0, M * K * sizeof(float));

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
 * @brief 基于 parallel_for 的并行矩阵乘法（优化版：使用 B 转置）
 */
void gemm_parallel(float *A, float *B, float *C, int M, int N, int K,
                   int num_threads) {
  // 初始化 C 为零矩阵
  memset(C, 0, M * K * sizeof(float));

  // 分配 B 转置矩阵
  float *Bt = (float *)malloc(K * N * sizeof(float));
  if (!Bt) {
    fprintf(stderr, "gemm_parallel: Bt 内存分配失败\n");
    return;
  }

  // 转置 B 矩阵
  transpose_B(B, Bt, N, K);

  // 设置 functor 参数
  gemm_args_t args;
  args.A = A;
  args.B = B;
  args.Bt = Bt;
  args.C = C;
  args.N = N;
  args.K = K;

  // 使用 parallel_for 并行计算每一行
  parallel_for(0, M, 1, gemm_functor, (void *)&args, num_threads);

  // 释放 Bt
  free(Bt);
}

/**
 * @brief 基于 parallel_for 高级 API 的矩阵乘法（支持调度策略，优化版：使用 B
 * 转置）
 */
void gemm_parallel_advanced(float *A, float *B, float *C, int M, int N, int K,
                            int num_threads, schedule_type_t schedule) {
  // 初始化 C 为零矩阵
  memset(C, 0, M * K * sizeof(float));

  // 分配 B 转置矩阵
  float *Bt = (float *)malloc(K * N * sizeof(float));
  if (!Bt) {
    fprintf(stderr, "gemm_parallel_advanced: Bt 内存分配失败\n");
    return;
  }

  // 转置 B 矩阵
  transpose_B(B, Bt, N, K);

  // 设置 functor 参数
  gemm_args_t args;
  args.A = A;
  args.B = B;
  args.Bt = Bt;
  args.C = C;
  args.N = N;
  args.K = K;

  // 设置并行配置
  parallel_config_t config;
  config.num_threads = num_threads;
  config.schedule = schedule;
  config.chunk_size = 1;

  // 使用 parallel_for_advanced 并行计算每一行
  parallel_for_advanced(0, M, 1, gemm_functor, (void *)&args, &config);

  // 释放 Bt
  free(Bt);
}

/**
 * @brief 验证矩阵乘法结果
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

  printf("========================================\n");
  printf("  Pthreads parallel_for 矩阵乘法 (GEMM)\n");
  printf("========================================\n");
  printf("矩阵规模: A(%d×%d) × B(%d×%d) = C(%d×%d)\n", M, N, N, K, M, K);
  printf("线程数: %d\n", num_threads);
  printf("========================================\n");

  // 分配内存
  float *A = (float *)malloc(M * N * sizeof(float));
  float *B = (float *)malloc(N * K * sizeof(float));
  float *C_serial = (float *)malloc(M * K * sizeof(float));
  float *C_parallel = (float *)malloc(M * K * sizeof(float));
  float *C_static = (float *)malloc(M * K * sizeof(float));
  float *C_dynamic = (float *)malloc(M * K * sizeof(float));
  float *C_guided = (float *)malloc(M * K * sizeof(float));

  if (!A || !B || !C_serial || !C_parallel || !C_static || !C_dynamic ||
      !C_guided) {
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

  clock_t start_time, end_time;

  // 1. 串行矩阵乘法（验证用）
  printf("\n[1] 串行矩阵乘法...\n");
  start_time = clock();
  gemm_serial(A, B, C_serial, M, N, K);
  end_time = clock();
  double serial_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
  printf("    串行时间: %.6f 秒\n", serial_time);

  // 2. 基于 parallel_for 的并行矩阵乘法（静态调度）
  printf("\n[2] parallel_for 静态调度...\n");
  start_time = clock();
  gemm_parallel(A, B, C_parallel, M, N, K, num_threads);
  end_time = clock();
  double parallel_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
  printf("    并行时间: %.6f 秒\n", parallel_time);
  printf("    加速比: %.4fx\n", serial_time / parallel_time);

  // 验证结果
  if (verify_result(C_serial, C_parallel, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 3. 静态调度（高级 API）
  printf("\n[3] parallel_for_advanced 静态调度...\n");
  start_time = clock();
  gemm_parallel_advanced(A, B, C_static, M, N, K, num_threads, SCHEDULE_STATIC);
  end_time = clock();
  double static_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
  printf("    静态调度时间: %.6f 秒\n", static_time);
  printf("    加速比: %.4fx\n", serial_time / static_time);

  if (verify_result(C_serial, C_static, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 4. 动态调度
  printf("\n[4] parallel_for_advanced 动态调度...\n");
  start_time = clock();
  gemm_parallel_advanced(A, B, C_dynamic, M, N, K, num_threads,
                         SCHEDULE_DYNAMIC);
  end_time = clock();
  double dynamic_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
  printf("    动态调度时间: %.6f 秒\n", dynamic_time);
  printf("    加速比: %.4fx\n", serial_time / dynamic_time);

  if (verify_result(C_serial, C_dynamic, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 5. 引导调度
  printf("\n[5] parallel_for_advanced 引导调度...\n");
  start_time = clock();
  gemm_parallel_advanced(A, B, C_guided, M, N, K, num_threads, SCHEDULE_GUIDED);
  end_time = clock();
  double guided_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;
  printf("    引导调度时间: %.6f 秒\n", guided_time);
  printf("    加速比: %.4fx\n", serial_time / guided_time);

  if (verify_result(C_serial, C_guided, M, K, 1e-3) == 0) {
    printf("    结果验证: 通过 ✓\n");
  }

  // 性能总结
  printf("\n========================================\n");
  printf("  性能总结\n");
  printf("========================================\n");
  printf("调度方式          | 时间 (秒)   | 加速比\n");
  printf("------------------|-------------|--------\n");
  printf("串行              | %-11.6f | 1.0000x\n", serial_time);
  printf("parallel_for 默认 | %-11.6f | %.4fx\n", parallel_time,
         serial_time / parallel_time);
  printf("parallel_for 静态 | %-11.6f | %.4fx\n", static_time,
         serial_time / static_time);
  printf("parallel_for 动态 | %-11.6f | %.4fx\n", dynamic_time,
         serial_time / dynamic_time);
  printf("parallel_for 引导 | %-11.6f | %.4fx\n", guided_time,
         serial_time / guided_time);
  printf("========================================\n");

  // 小规模矩阵打印结果
  if (M <= 8 && K <= 8) {
    print_matrix(C_serial, M, K, "C (串行)");
    print_matrix(C_parallel, M, K, "C (parallel_for)");
  }

  // 释放内存
  free(A);
  free(B);
  free(C_serial);
  free(C_parallel);
  free(C_static);
  free(C_dynamic);
  free(C_guided);

  printf("\n完成!\n");
  return 0;
}