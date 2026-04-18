#include "D:\Microsoft SDKs\MPI\Include\mpi.h"
#include <iostream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

using namespace std;

/*
 * MPI 集合通信实现并行通用矩阵乘法 C = A × B
 * 使用 2D Block Cyclic Distribution 进行数据划分
 * A: m×n, B: n×k, C: m×k
 * 使用 MPI_Type_create_struct 聚合进程内变量后通信
 */

class MatrixParams
{
public:
    int m, n, k;
    int rows;  // 该进程需要处理的行数
    int cols;  // 该进程需要处理的列数
    double compute_time;  // 计算时间

    static MPI_Datatype MPI_type;

    static void buildMPIType();
};

MPI_Datatype MatrixParams::MPI_type;

void MatrixParams::buildMPIType()
{
    // 创建一个临时对象来获取字段地址
    MatrixParams temp;

    // 使用两个块：5 个连续的 int (m,n,k,rows,cols) + 1 个 double (compute_time)
    MPI_Datatype types[2] = {MPI_INT, MPI_DOUBLE};
    int block_lengths[2] = {5, 1};
    MPI_Aint displacements[2];

    // 获取各个字段相对于结构体起始地址的偏移
    MPI_Aint base_address;
    MPI_Get_address(&temp, &base_address);
    MPI_Get_address(&temp.m, &displacements[0]);            // 第一个 int 的位置
    MPI_Get_address(&temp.compute_time, &displacements[1]); // double 的位置

    // 调整偏移量为相对偏移
    displacements[0] -= base_address;
    displacements[1] -= base_address;

    // 创建结构化 MPI 数据类型
    MPI_Type_create_struct(2, block_lengths, displacements, types, &MPI_type);
    MPI_Type_commit(&MPI_type);
}

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

/*
 * 2D Block Cyclic Distribution for Matrix A (m x n)
 * Distribute among sqrt(num_procs) x sqrt(num_procs) processor grid
 */
static void get_2d_block_info(int rank, int num_procs, int m, int n, int k,
                              int *out_start_row, int *out_rows,
                              int *out_start_col, int *out_cols,
                              int *p_rows, int *p_cols) {
    // Calculate grid dimensions
    int sqrt_p = (int)sqrt(num_procs);
    for (int i = sqrt_p; i >= 1; i--) {
        if (num_procs % i == 0) {
            *p_rows = i;
            *p_cols = num_procs / i;
            break;
        }
    }

    // If we can't factorize evenly, use closest factors
    if (*p_rows * *p_cols != num_procs) {
        // Find best rectangular decomposition
        int best_diff = num_procs;
        int best_i = 1;
        for (int i = 1; i <= num_procs; i++) {
            if (num_procs % i == 0) {
                int j = num_procs / i;
                if (abs(i - j) < best_diff) {
                    best_diff = abs(i - j);
                    best_i = i;
                }
            }
        }
        *p_rows = best_i;
        *p_cols = num_procs / best_i;
    }

    // Processor coordinates in grid
    int proc_row = rank / *p_cols;
    int proc_col = rank % *p_cols;

    // Block size for each dimension
    int block_size_row = (m + *p_rows - 1) / *p_rows;  // Ceiling division
    int block_size_col = (n + *p_cols - 1) / *p_cols;  // Ceiling division

    // Starting indices for this processor's block
    *out_start_row = proc_row * block_size_row;
    *out_start_col = proc_col * block_size_col;

    // Actual number of rows/columns for this processor
    *out_rows = (proc_row == *p_rows - 1) ? m - *out_start_row : block_size_row;
    *out_cols = (proc_col == *p_cols - 1) ? n - *out_start_col : block_size_col;

    // Ensure we don't go out of bounds
    if (*out_start_row + *out_rows > m) *out_rows = m - *out_start_row;
    if (*out_start_col + *out_cols > n) *out_cols = n - *out_start_col;

    if (*out_rows < 0) *out_rows = 0;
    if (*out_cols < 0) *out_cols = 0;
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
    fprintf(stderr, "Usage: mpi_2d_block_mat_mul <m> <n> <k>\n");
    fprintf(stderr, "   or: mpi_2d_block_mat_mul <size>  (m=n=k=size)\n");
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
              "Error: need at least 2 processes (require multiple processes for collective operations)\n");
    MPI_Finalize();
    return 1;
  }

  // 初始化自定义MPI数据类型 (必须在所有进程上执行)
  MatrixParams::buildMPIType();

  // 计算2D块划分信息
  int p_rows, p_cols;
  int start_row, rows, start_col, cols;
  get_2d_block_info(rank, num_procs, m, n, k, &start_row, &rows, &start_col, &cols, &p_rows, &p_cols);

  if (rank == 0) {
    /* ==================== Rank 0: Root process ==================== */
    srand(42);

    /* 生成矩阵 A 和 B */
    double *A = alloc_matrix(m, n);
    double *B = alloc_matrix(n, k);
    double *C = alloc_matrix(m, k);
    fill_random(A, m, n);
    fill_random(B, n, k);

    printf("Matrix size: A(%d x %d) * B(%d x %d) = C(%d x %d)\n", m, n, n, k, m, k);
    printf("Processes: %d (%d x %d processor grid)\n", num_procs, p_rows, p_cols);

    /* ===== 集合通信分发数据 ===== */
    double t_comm_start = MPI_Wtime();

    // 广播 B 矩阵给所有进程
    MPI_Bcast(B, n * k, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // 广播矩阵参数（分别广播每个字段，避免 MPI 类型大小不一致问题）
    int params_arr[5];
    params_arr[0] = m;
    params_arr[1] = n;
    params_arr[2] = k;
    params_arr[3] = rows;
    params_arr[4] = cols;
    MPI_Bcast(params_arr, 5, MPI_INT, 0, MPI_COMM_WORLD);

    // 发送 grid dimensions
    int grid_dims[2] = {p_rows, p_cols};
    MPI_Bcast(grid_dims, 2, MPI_INT, 0, MPI_COMM_WORLD);

    // 准备 Scatterv 参数：按行块分发 A 矩阵
    int *sendcounts = (int *)malloc(num_procs * sizeof(int));
    int *displs = (int *)malloc(num_procs * sizeof(int));

    for (int r = 0; r < num_procs; r++) {
      int r_start_row, r_rows, r_start_col, r_cols;
      int r_p_rows, r_p_cols;
      get_2d_block_info(r, num_procs, m, n, k, &r_start_row, &r_rows,
                        &r_start_col, &r_cols, &r_p_rows, &r_p_cols);
      sendcounts[r] = r_rows * n;  // r_rows 行，每行 n 个元素
      displs[r] = r_start_row * n; // 起始行偏移
    }

    // Root 进程分配 A_full 用于 Scatterv
    double *A_full = A; // 直接使用已分配的 A 矩阵

    // 从数组恢复参数
    MatrixParams params;
    params.m = params_arr[0];
    params.n = params_arr[1];
    params.k = params_arr[2];
    params.rows = params_arr[3];
    params.cols = params_arr[4];

    /* ===== 各进程独立计算 ===== */
    // 每个进程只计算其负责的 A 块与整个 B 矩阵的乘积部分
    // 使用 Scatterv 分发 A 的行块
    double *A_local = alloc_matrix(rows, n); // 整行数据

    // 开始 Scatterv 通信计时
    double t_scatter_start = MPI_Wtime();
    MPI_Scatterv(A_full, sendcounts, displs, MPI_DOUBLE, A_local, rows * n,
                 MPI_DOUBLE, 0, MPI_COMM_WORLD);
    double t_scatter_end = MPI_Wtime();

    free(sendcounts);
    free(displs);

    double t_comm_end = MPI_Wtime();
    double comm_time = t_scatter_end - t_scatter_start;

    double *C_local = alloc_matrix(rows, k);
    double t_compute_start = MPI_Wtime();
    mat_mul_serial(A_local, B, C_local, rows, n, k);
    double t_compute_end = MPI_Wtime();
    double compute_time = t_compute_end - t_compute_start;

    /* 更新参数结构体并发送回根进程 */
    params.compute_time = compute_time;

    /* ===== 收集结果 ===== */
    double t_gather_start = MPI_Wtime();

    // 非 Root 进程先发送 C_local 到 Root
    if (rank != 0) {
      MPI_Send(C_local, rows * k, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    }

    // Root 进程接收所有其他进程的数据并组装到 C
    if (rank == 0) {
      // 先复制自己的部分
      for (int i = 0; i < rows; i++) {
        for (int j = 0; j < k; j++) {
          C[(start_row + i) * k + j] = C_local[i * k + j];
        }
      }
      // 接收其他进程的数据
      for (int r = 1; r < num_procs; r++) {
        int r_start_row, r_rows, r_start_col, r_cols;
        int r_p_rows, r_p_cols;
        get_2d_block_info(r, num_procs, m, n, k, &r_start_row, &r_rows,
                          &r_start_col, &r_cols, &r_p_rows, &r_p_cols);
        double *recv_buf = alloc_matrix(r_rows, k);
        MPI_Recv(recv_buf, r_rows * k, MPI_DOUBLE, r, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        // 复制到 C 的正确位置
        for (int i = 0; i < r_rows; i++) {
          for (int j = 0; j < k; j++) {
            C[(r_start_row + i) * k + j] = recv_buf[i * k + j];
          }
        }
        free(recv_buf);
      }
    }

    double t_gather_end = MPI_Wtime();
    double gather_time = t_gather_end - t_gather_start;
    double end_to_end_time = comm_time + compute_time + gather_time;

    printf("Comm Time:     %.6f seconds\n", comm_time);
    printf("Compute Time:  %.6f seconds (pure computation)\n", compute_time);
    printf("Gather Time:   %.6f seconds\n", gather_time);
    printf("Total Time:    %.6f seconds\n", end_to_end_time);

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

    // Use MPI_Reduce to get the maximum compute time across all processes
    double max_compute_time;
    MPI_Reduce(&compute_time, &max_compute_time, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);

    printf("Serial Time:   %.6f seconds\n", serial_time);
    // Speedup based on pure compute time (consistent with task1)
    if (max_compute_time > 0.00001)
      printf("Speedup (pure compute):  %.2fx\n",
             serial_time / max_compute_time);
    else
      printf("Speedup (pure compute):  N/A (too fast)\n");

    free(A);
    free(B);
    free(C);
    free(A_local);
    free(C_local);
  } else {
    /* ==================== 非 Root 进程 ==================== */
    // 先分配 B 矩阵缓冲区（与 Root 进程同步执行 MPI_Bcast）
    // m, n, k 已经在 main 函数开头由所有进程解析
    double *B_local = alloc_matrix(n, k);
    MPI_Bcast(B_local, n * k, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // 接收矩阵参数（分别接收每个字段）
    int params_arr[5];
    MPI_Bcast(params_arr, 5, MPI_INT, 0, MPI_COMM_WORLD);

    // 接收 grid dimensions
    int grid_dims[2];
    MPI_Bcast(grid_dims, 2, MPI_INT, 0, MPI_COMM_WORLD);
    int p_rows = grid_dims[0];
    int p_cols = grid_dims[1];

    // 从数组恢复参数
    MatrixParams params;
    params.m = params_arr[0];
    params.n = params_arr[1];
    params.k = params_arr[2];
    params.rows = params_arr[3];
    params.cols = params_arr[4];

    int m_local = params.m, n_local = params.n, k_local = params.k;
    int rows = params.rows, cols = params.cols;

    // 重新计算自己的块信息
    int start_row, start_col;
    get_2d_block_info(rank, num_procs, m_local, n_local, k_local,
                     &start_row, &rows, &start_col, &cols, &p_rows, &p_cols);

    // 准备 Scatterv 参数（与 Root 进程相同）
    int *sendcounts = (int *)malloc(num_procs * sizeof(int));
    int *displs = (int *)malloc(num_procs * sizeof(int));

    for (int r = 0; r < num_procs; r++) {
      int r_start_row, r_rows, r_start_col, r_cols;
      int r_p_rows, r_p_cols;
      get_2d_block_info(r, num_procs, m_local, n_local, k_local, &r_start_row,
                        &r_rows, &r_start_col, &r_cols, &r_p_rows, &r_p_cols);
      sendcounts[r] = r_rows * n_local;
      displs[r] = r_start_row * n_local;
    }

    // 接收 A 矩阵的行块
    double *A_local = alloc_matrix(rows, n_local);
    MPI_Scatterv(NULL, sendcounts, displs, MPI_DOUBLE, A_local, rows * n_local,
                 MPI_DOUBLE, 0, MPI_COMM_WORLD);

    free(sendcounts);
    free(displs);

    // 计算 C_local = A_local × B_local
    double *C_local = alloc_matrix(rows, k_local);
    double t_compute_start = MPI_Wtime();
    mat_mul_serial(A_local, B_local, C_local, rows, n_local, k_local);
    double t_compute_end = MPI_Wtime();
    double compute_time = t_compute_end - t_compute_start;

    // 更新参数结构体
    params.compute_time = compute_time;

    // 发送 C_local 到 Root 进程（与 Root 进程的 MPI_Recv 配对）
    MPI_Send(C_local, rows * k_local, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);

    // Participate in MPI_Reduce to get max compute time
    double max_compute_time_dummy;
    MPI_Reduce(&compute_time, &max_compute_time_dummy, 1, MPI_DOUBLE, MPI_MAX,
               0, MPI_COMM_WORLD);

    free(A_local);
    free(B_local);
    free(C_local);
  }

  // 同步所有进程
  MPI_Barrier(MPI_COMM_WORLD);

  // 清理自定义数据类型
  MPI_Type_free(&MatrixParams::MPI_type);

  MPI_Finalize();
  return 0;
}