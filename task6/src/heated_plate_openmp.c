/**
 * @file heated_plate_openmp.c
 * @brief OpenMP 参考实现：稳态热传导模拟
 *
 * 原始 OpenMP 实现，作为 Pthreads 版本的性能对比基准。
 * 边界条件：上边界 = 0，其余三边 = 100
 * 迭代公式：W[i][j] = (1/4) * (W[i-1][j] + W[i+1][j] + W[i][j-1] + W[i][j+1])
 *
 * 编译：
 *   gcc -O2 -fopenmp -o heated_plate_openmp.exe heated_plate_openmp.c -lm
 * 运行：
 *   .\heated_plate_openmp.exe [num_threads]
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define M 500
#define N 500

static double u[M][N];
static double w[M][N];

static inline double wtime(void) {
    return omp_get_wtime();
}

int main(int argc, char *argv[]) {
    double epsilon = 0.001;
    int num_threads = 4;

    if (argc > 1) num_threads = atoi(argv[1]);
    omp_set_num_threads(num_threads);

    printf("\n");
    printf("HEATED_PLATE_OPENMP\n");
    printf("  C/OpenMP version (reference)\n");
    printf("  A program to solve for the steady state temperature distribution\n");
    printf("  over a rectangular plate.\n");
    printf("\n");
    printf("  Spatial grid of %d by %d points.\n", M, N);
    printf("  The iteration will be repeated until the change is <= %e\n", epsilon);
    printf("  Number of threads = %d\n", num_threads);
    printf("\n");

    /* ----- 初始化边界条件 ----- */

    /* 上边界 w[0][j] = 0 */
    #pragma omp parallel for
    for (int j = 0; j < N; j++) {
        w[0][j] = 0.0;
    }

    /* 下边界 w[M-1][j] = 100 */
    #pragma omp parallel for
    for (int j = 0; j < N; j++) {
        w[M - 1][j] = 100.0;
    }

    /* 左右边界 */
    #pragma omp parallel for
    for (int i = 1; i < M - 1; i++) {
        w[i][0]     = 100.0;
        w[i][N - 1] = 100.0;
    }

    /* 计算边界平均值 */
    double boundary_sum = 0.0;
    #pragma omp parallel for reduction(+:boundary_sum)
    for (int j = 0; j < N; j++) {
        boundary_sum += w[0][j] + w[M - 1][j];
    }
    #pragma omp parallel for reduction(+:boundary_sum)
    for (int i = 1; i < M - 1; i++) {
        boundary_sum += w[i][0] + w[i][N - 1];
    }

    double mean = boundary_sum / (double)(2 * M + 2 * N - 4);
    printf("  MEAN = %f\n", mean);

    /* 初始化内部点 */
    #pragma omp parallel for
    for (int i = 1; i < M - 1; i++) {
        for (int j = 1; j < N - 1; j++) {
            w[i][j] = mean;
        }
    }

    /* ----- 迭代直到收敛 ----- */
    int iterations       = 0;
    int iterations_print = 1;
    double diff;

    printf("\n");
    printf(" Iteration  Change\n");
    printf("\n");

    double start_time = wtime();
    diff = epsilon;

    while (epsilon <= diff) {
        /* 保存旧解 */
        #pragma omp parallel for
        for (int i = 0; i < M; i++) {
            for (int j = 0; j < N; j++) {
                u[i][j] = w[i][j];
            }
        }

        /* 更新内部点 */
        #pragma omp parallel for
        for (int i = 1; i < M - 1; i++) {
            for (int j = 1; j < N - 1; j++) {
                w[i][j] = (u[i - 1][j] + u[i + 1][j]
                         + u[i][j - 1] + u[i][j + 1]) / 4.0;
            }
        }

        /* 计算全局最大差值 */
        diff = 0.0;
        #pragma omp parallel for reduction(max:diff)
        for (int i = 1; i < M - 1; i++) {
            for (int j = 1; j < N - 1; j++) {
                double d = fabs(w[i][j] - u[i][j]);
                if (d > diff) diff = d;
            }
        }

        iterations++;
        if (iterations == iterations_print) {
            printf("  %8d  %f\n", iterations, diff);
            iterations_print = 2 * iterations_print;
        }
    }

    double end_time      = wtime();
    double wtime_elapsed = end_time - start_time;

    printf("\n");
    printf("  %8d  %f\n", iterations, diff);
    printf("\n");
    printf("  Error tolerance achieved.\n");
    printf("  Wallclock time = %f\n", wtime_elapsed);
    printf("\n");
    printf("HEATED_PLATE_OPENMP:\n");
    printf("  Normal end of execution.\n");

    return 0;
}
