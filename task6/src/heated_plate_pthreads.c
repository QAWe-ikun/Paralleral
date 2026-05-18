/**
 * @file heated_plate_pthreads.c
 * @brief 基于 Pthreads 线程池 parallel_for_pool 的稳态热传导模拟
 *
 * 将 heated_plate_openmp.c 改造为基于 Pthreads 的并行应用
 * 使用 parallel_for_pool 线程池动态链接库实现并行化
 *
 * 物理问题：矩形板上的稳态热传导
 * 边界条件：
 *           W = 0
 *     +------------------+
 *     |                  |
 * W = 100 |                  | W = 100
 *     |                  |
 *     +------------------+
 *           W = 100
 *
 * 迭代公式：W[i][j] = (1/4) * (W[i-1][j] + W[i+1][j] + W[i][j-1] + W[i][j+1])
 *
 * 编译：
 *   gcc -shared -fPIC -O2 -o libparallel_for_pool.so src/parallel_for_pool.c
 * -lpthread
 *   gcc -O3 -march=native -ftree-vectorize -funroll-loops \
 *       -Isrc -o bin/heated_plate_pthreads src/heated_plate_pthreads.c \
 *       -Llib -lparallel_for_pool -lpthread -lm
 * 运行：
 *   ./bin/heated_plate_pthreads [num_threads]
 * [schedule:0=static,1=dynamic,2=guided] [chunk_size]
 */

#define _POSIX_C_SOURCE 199309L
#include "parallel_for_pool.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define M 500
#define N 500

/**
 * @brief 迭代参数结构体
 */
typedef struct {
  double (*u)[N];      /* 上一次迭代的解 */
  double (*w)[N];      /* 当前迭代的解 */
  int n_dim;           /* 列数 */
  double *local_diffs; /* 每行的局部最大差值 */
  double mean;         /* 边界平均值 */
} iter_args_t;

/* ============================================================
 *  Functors —— 与 OpenMP 的 3 次 #pragma omp parallel for 一一对应
 * ============================================================ */

/** @brief 初始化上边界行 (w[0][j] = 0) */
void *init_top_row(int idx, void *arg) {
  (void)idx;
  iter_args_t *a = (iter_args_t *)arg;
  for (int j = 0; j < a->n_dim; j++)
    a->w[0][j] = 0.0;
  return NULL;
}

/** @brief 初始化下边界行 (w[M-1][j] = 100) */
void *init_bottom_row(int idx, void *arg) {
  (void)idx;
  iter_args_t *a = (iter_args_t *)arg;
  for (int j = 0; j < a->n_dim; j++)
    a->w[M - 1][j] = 100.0;
  return NULL;
}

/** @brief 初始化第 idx 行的左右边界 */
void *init_side_boundary(int idx, void *arg) {
  iter_args_t *a = (iter_args_t *)arg;
  a->w[idx][0] = 100.0;
  a->w[idx][a->n_dim - 1] = 100.0;
  return NULL;
}

/** @brief 初始化第 idx 行的内部点 */
void *init_interior_row(int idx, void *arg) {
  iter_args_t *a = (iter_args_t *)arg;
  for (int j = 1; j < a->n_dim - 1; j++)
    a->w[idx][j] = a->mean;
  return NULL;
}

/**
 * @brief 保存第 idx 行: u[idx] = w[idx]
 * 对应 OpenMP: #pragma omp parallel for (copy 循环)
 */
void *copy_w_to_u_row(int idx, void *arg) {
  iter_args_t *a = (iter_args_t *)arg;
  for (int j = 0; j < a->n_dim; j++)
    a->u[idx][j] = a->w[idx][j];
  return NULL;
}

/**
 * @brief 更新第 idx 行的内部点 (Jacobi 迭代)
 * 对应 OpenMP: #pragma omp parallel for (update 循环)
 */
void *update_interior_row(int idx, void *arg) {
  iter_args_t *a = (iter_args_t *)arg;
  for (int j = 1; j < a->n_dim - 1; j++) {
    a->w[idx][j] = (a->u[idx - 1][j] + a->u[idx + 1][j] + a->u[idx][j - 1] +
                    a->u[idx][j + 1]) /
                   4.0;
  }
  return NULL;
}

/**
 * @brief 计算第 idx 行的局部最大差值 |w - u|
 * 对应 OpenMP: #pragma omp parallel for reduction(max:diff)
 */
void *compute_local_diff_row(int idx, void *arg) {
  iter_args_t *a = (iter_args_t *)arg;
  double local_max = 0.0;
  for (int j = 1; j < a->n_dim - 1; j++) {
    double d = a->w[idx][j] - a->u[idx][j];
    if (d < 0)
      d = -d;
    if (d > local_max)
      local_max = d;
  }
  a->local_diffs[idx] = local_max;
  return NULL;
}

/** @brief 墙上时间 */
static inline double wtime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + 1e-9 * ts.tv_nsec;
}

int main(int argc, char *argv[]) {
  double epsilon = 0.001;
  int num_threads = 4;
  schedule_type_t schedule = SCHEDULE_STATIC;
  int chunk_size = 1;

  if (argc > 1)
    num_threads = atoi(argv[1]);
  if (argc > 2) {
    int s = atoi(argv[2]);
    if (s == 0)
      schedule = SCHEDULE_STATIC;
    else if (s == 1)
      schedule = SCHEDULE_DYNAMIC;
    else if (s == 2)
      schedule = SCHEDULE_GUIDED;
  }
  if (argc > 3)
    chunk_size = atoi(argv[3]);

  printf("\n");
  printf("HEATED_PLATE_PTHREADS\n");
  printf("  C/Pthreads version using parallel_for_pool (thread pool)\n");
  printf(
      "  A program to solve for the steady state temperature distribution\n");
  printf("  over a rectangular plate.\n");
  printf("\n");
  printf("  Spatial grid of %d by %d points.\n", M, N);
  printf("  The iteration will be repeated until the change is <= %e\n",
         epsilon);
  printf("  Number of threads = %d\n", num_threads);
  printf("  Schedule type = ");
  if (schedule == SCHEDULE_STATIC)
    printf("STATIC");
  else if (schedule == SCHEDULE_DYNAMIC)
    printf("DYNAMIC");
  else if (schedule == SCHEDULE_GUIDED)
    printf("GUIDED");
  printf("\n");
  printf("  Chunk size = %d\n", chunk_size);
  printf("\n");

  parallel_config_t config;
  config.num_threads = num_threads;
  config.schedule = schedule;
  config.chunk_size = chunk_size;

  /* 分配网格 */
  double (*u)[N] = (double (*)[N])malloc(sizeof(double) * M * N);
  double (*w)[N] = (double (*)[N])malloc(sizeof(double) * M * N);
  if (!u || !w) {
    fprintf(stderr, "内存分配失败\n");
    return 1;
  }

  double *local_diffs = (double *)calloc(M, sizeof(double));

  iter_args_t args;
  args.u = u;
  args.w = w;
  args.n_dim = N;
  args.local_diffs = local_diffs;

  /* ----- 初始化边界条件 ----- */
  parallel_for_advanced(0, 1, 1, init_top_row, &args, &config);
  parallel_for_advanced(M - 1, M, 1, init_bottom_row, &args, &config);
  parallel_for_advanced(1, M - 1, 1, init_side_boundary, &args, &config);

  /* 计算边界平均值 */
  double boundary_sum = 0.0;
  for (int j = 0; j < N; j++)
    boundary_sum += w[0][j];
  for (int j = 0; j < N; j++)
    boundary_sum += w[M - 1][j];
  for (int i = 1; i < M - 1; i++)
    boundary_sum += w[i][0];
  for (int i = 1; i < M - 1; i++)
    boundary_sum += w[i][N - 1];

  double mean = boundary_sum / (double)(2 * M + 2 * N - 4);
  printf("  MEAN = %f\n", mean);
  args.mean = mean;

  parallel_for_advanced(1, M - 1, 1, init_interior_row, &args, &config);

  /* ----- 迭代直到收敛 ----- */
  int iterations = 0;
  int iterations_print = 1;
  double diff;

  printf("\n");
  printf(" Iteration  Change\n");
  printf("\n");

  double start_time = wtime();
  diff = epsilon;

  while (epsilon <= diff) {
    /* 第 1 次调用：保存旧解 */
    parallel_for_advanced(0, M, 1, copy_w_to_u_row, &args, &config);

    /* 第 2 次调用：更新内部点 */
    parallel_for_advanced(1, M - 1, 1, update_interior_row, &args, &config);

    /* 第 3 次调用：计算每行最大差值 */
    memset(local_diffs, 0, M * sizeof(double));
    parallel_for_advanced(1, M - 1, 1, compute_local_diff_row, &args, &config);

    /* 归约：全局最大差值（串行） */
    diff = 0.0;
    for (int i = 1; i < M - 1; i++) {
      if (local_diffs[i] > diff)
        diff = local_diffs[i];
    }

    iterations++;
    if (iterations == iterations_print) {
      printf("  %8d  %f\n", iterations, diff);
      iterations_print = 2 * iterations_print;
    }
  }

  double end_time = wtime();
  double wtime_elapsed = end_time - start_time;

  printf("\n");
  printf("  %8d  %f\n", iterations, diff);
  printf("\n");
  printf("  Error tolerance achieved.\n");
  printf("  Wallclock time = %f\n", wtime_elapsed);
  printf("\n");
  printf("HEATED_PLATE_PTHREADS:\n");
  printf("  Normal end of execution.\n");

  parallel_for_pool_destroy();

  free(u);
  free(w);
  free(local_diffs);
  return 0;
}
