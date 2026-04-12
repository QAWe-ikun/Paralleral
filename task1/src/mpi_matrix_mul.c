#include "D:\Microsoft SDKs\MPI\Include\mpi.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * MPI 点对点通信实现并行通用矩阵乘法 C = A × B
 * A: m×n, B: n×k, C: m×k
 * Rank 0 为主进程，负责生成矩阵、分配任务、收集结果和输出
 * 其余进程为工作进程，负责计算被分配的矩阵块
 *
 * 计时说明：
 *   - 串行时间：master 本地计算完整 C = A×B 的纯计算时间
 *   - 并行时间：worker 接收到数据后开始计算到发送结果完成的时间
 *               master 侧记录从发送开始到接收完所有结果的总时间
 */

static double *alloc_matrix(int rows, int cols) {
  double *m = (double *)malloc((size_t)rows * cols * sizeof(double));
  if (!m) {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  return m;
}

static void fill_random(double *mat, int rows, int cols) {
  for (int i = 0; i < rows * cols; i++)
    mat[i] = (double)rand() / RAND_MAX;
}

static void print_matrix(const char *name, double *mat, int rows, int cols) {
  printf("=== %s (%d x %d) ===\n", name, rows, cols);
  int max_print = 8;
  int pr = (rows <= max_print) ? rows : max_print;
  int pc = (cols <= max_print) ? cols : max_print;
  for (int i = 0; i < pr; i++) {
    for (int j = 0; j < pc; j++)
      printf("%8.4f ", mat[i * cols + j]);
    if (pc < cols)
      printf("... ");
    printf("\n");
  }
  if (pr < rows)
    printf("...\n");
}

/* 串行矩阵乘法 C = A × B (ikj 顺序，缓存友好) */
static void mat_mul_serial(double *A, double *B, double *C, int m, int n,
                           int k) {
  for (int i = 0; i < m; i++)
    for (int j = 0; j < k; j++)
      C[i * k + j] = 0.0;
  for (int i = 0; i < m; i++)
    for (int p = 0; p < n; p++)
      for (int j = 0; j < k; j++)
        C[i * k + j] += A[i * n + p] * B[p * k + j];
}

/* 计算 worker 的分片信息 */
static void get_worker_info(int w_id, int num_workers, int m, int n, int k,
                            int *out_start_row, int *out_rows) {
  /* 循环分配：第 w_id 个 worker 从 start_row 开始，分配 rows 行 */
  int rows_per_worker = m / num_workers;
  int remainder = m % num_workers;
  int rows = rows_per_worker + (w_id < remainder ? 1 : 0);
  int start_row = w_id * rows_per_worker + (w_id < remainder ? w_id : remainder);
  *out_start_row = start_row;
  *out_rows = rows;
}

int main(int argc, char **argv) {
  int m, n, k;

  /* 解析参数 */
  if (argc >= 4) {
    m = atoi(argv[1]);
    n = atoi(argv[2]);
    k = atoi(argv[3]);
    if (m < 128 || m > 2048 || n < 128 || n > 2048 || k < 128 || k > 2048) {
      fprintf(stderr, "Error: m, n, k must be in range [128, 2048]\n");
      return 1;
    }
  } else if (argc == 2) {
    int size = atoi(argv[1]);
    if (size < 128 || size > 2048) {
      fprintf(stderr, "Error: matrix size must be in range [128, 2048]\n");
      return 1;
    }
    m = n = k = size;
  } else {
    fprintf(stderr, "Usage: mpi_matrix_mul <m> <n> <k>\n");
    fprintf(stderr, "   or: mpi_matrix_mul <size>  (m=n=k=size)\n");
    return 1;
  }

  /* MPI 初始化 */
  MPI_Init(&argc, &argv);
  int rank, num_procs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

  if (num_procs < 2) {
    if (rank == 0)
      fprintf(stderr,
              "Error: need at least 2 processes (1 master + 1 worker)\n");
    MPI_Finalize();
    return 1;
  }

  int num_workers = num_procs - 1;

  if (rank == 0) {
    /* ==================== Rank 0: 主进程 ==================== */
    srand(42);

    /* 生成矩阵 A 和 B */
    double *A = alloc_matrix(m, n);
    double *B = alloc_matrix(n, k);
    double *C = alloc_matrix(m, k);
    fill_random(A, m, n);
    fill_random(B, n, k);

    printf("Matrix size: A(%d x %d) * B(%d x %d) = C(%d x %d)\n", m, n, n, k, m,
           k);
    printf("Processes: %d (1 master + %d workers)\n", num_procs, num_workers);

    /* 打印输入矩阵（小规模时） */
    if (m <= 16 && n <= 16 && k <= 16) {
      print_matrix("Matrix A", A, m, n);
      print_matrix("Matrix B", B, n, k);
    }

    /* ===== 第一阶段：分发数据 ===== */
    /* 使用非阻塞发送避免死锁 */
    MPI_Request *reqs =
        (MPI_Request *)malloc(num_workers * 3 * sizeof(MPI_Request));
    int ri = 0;

    for (int w = 1; w < num_procs; w++) {
      int w_id = w - 1;
      int start_row, rows;
      get_worker_info(w_id, num_workers, m, n, k, &start_row, &rows);

      /* 1) 发送 header [m, n, k, my_rows] */
      int header[4] = {m, n, k, rows};
      MPI_Isend(header, 4, MPI_INT, w, 1, MPI_COMM_WORLD, &reqs[ri++]);
      /* 2) 发送 B 矩阵 */
      MPI_Isend(B, n * k, MPI_DOUBLE, w, 2, MPI_COMM_WORLD, &reqs[ri++]);
      /* 3) 发送 A 子块 */
      MPI_Isend(A + start_row * n, rows * n, MPI_DOUBLE, w, 3, MPI_COMM_WORLD,
                &reqs[ri++]);
    }
    MPI_Waitall(ri, reqs, MPI_STATUSES_IGNORE);

    /* ===== 第二阶段：收集结果 ===== */
    double t_start = MPI_Wtime();

    double max_compute_time = 0.0;
    for (int w = 1; w < num_procs; w++) {
      int w_id = w - 1;
      int start_row, rows;
      get_worker_info(w_id, num_workers, m, n, k, &start_row, &rows);

      double worker_compute_time;
      MPI_Recv(&worker_compute_time, 1, MPI_DOUBLE, w, 11, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
      if (worker_compute_time > max_compute_time)
        max_compute_time = worker_compute_time;

      MPI_Recv(C + start_row * k, rows * k, MPI_DOUBLE, w, 10, MPI_COMM_WORLD,
               MPI_STATUS_IGNORE);
    }
    double t_end = MPI_Wtime();
    double end_to_end_time = t_end - t_start;

    printf("End-to-End Time: %.6f seconds (incl. communication)\n",
           end_to_end_time);
    printf("Compute Time:    %.6f seconds (max worker pure computation)\n",
           max_compute_time);

    /* 打印结果矩阵（小规模时） */
    if (m <= 16 && k <= 16) {
      print_matrix("Matrix C", C, m, k);
    }

    /* 验证正确性：串行计算并比较 */
    if (m <= 512 && n <= 512 && k <= 512) {
      double *C_check = alloc_matrix(m, k);
      mat_mul_serial(A, B, C_check, m, n, k);

      double max_err = 0.0;
      for (int i = 0; i < m * k; i++) {
        double err = fabs(C[i] - C_check[i]);
        if (err > max_err)
          max_err = err;
      }
      if (max_err < 1e-8)
        printf("Verification: PASSED (max error = %.2e)\n", max_err);
      else
        printf("Verification: FAILED (max error = %.2e)\n", max_err);
      free(C_check);
    }

    /* 性能分析：对比串行版本 */
    double t_serial_start = MPI_Wtime();
    {
      double *C_serial = alloc_matrix(m, k);
      mat_mul_serial(A, B, C_serial, m, n, k);
      free(C_serial);
    }
    double t_serial_end = MPI_Wtime();
    double serial_time = t_serial_end - t_serial_start;

    printf("Serial Time:   %.6f seconds\n", serial_time);
    if (end_to_end_time > 0.00001)
      printf("Speedup:         %.2fx\n", serial_time / end_to_end_time);
    else
      printf("Speedup:         N/A (too fast)\n");

    free(A);
    free(B);
    free(C);
    free(reqs);
  } else {
    /* ==================== 工作进程 ==================== */
    /* 接收数据: [header] + [B] + [A_sub] — 顺序必须与 master 一致 */
    int header[4]; /* m, n, k, my_rows */
    MPI_Recv(header, 4, MPI_INT, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int m = header[0];
    int n = header[1];
    int k = header[2];
    int my_rows = header[3];

    double *B = alloc_matrix(n, k);
    MPI_Recv(B, n * k, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    double *A_sub = alloc_matrix(my_rows, n);
    MPI_Recv(A_sub, my_rows * n, MPI_DOUBLE, 0, 3, MPI_COMM_WORLD,
             MPI_STATUS_IGNORE);

    /* ===== 计算 C_sub = A_sub × B ===== */
    double *C_sub = alloc_matrix(my_rows, k);
    double t_compute_start = MPI_Wtime();
    mat_mul_serial(A_sub, B, C_sub, my_rows, n, k);
    double t_compute_end = MPI_Wtime();
    double compute_time = t_compute_end - t_compute_start;

    /* 将计算时间和结果发送回主进程 */
    MPI_Send(&compute_time, 1, MPI_DOUBLE, 0, 11, MPI_COMM_WORLD);
    MPI_Send(C_sub, my_rows * k, MPI_DOUBLE, 0, 10, MPI_COMM_WORLD);

    free(A_sub);
    free(B);
    free(C_sub);
  }

  MPI_Finalize();
  return 0;
}
