#include "D:\Microsoft SDKs\MPI\Include\mpi.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace std;

/*
 * MPI 集合通信实现并行通用矩阵乘法 C = A × B
 * 使用 Column-wise Distribution 进行数据划分（与行划分对比）
 * A: m×n, B: n×k, C: m×k
 * 使用 MPI_Type_create_struct 聚合进程内变量后通信
 */

class MatrixParams {
public:
  int m, n, k;
  int rows;            // 该进程需要处理的行数（对于A矩阵）
  int cols;            // 该进程需要处理的列数（对于C矩阵）
  double compute_time; // 计算时间

  static MPI_Datatype MPI_type;

  static void buildMPIType();
};

MPI_Datatype MatrixParams::MPI_type;

void MatrixParams::buildMPIType() {
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
 * 按列划分 C 矩阵 (m x k) -> 每个进程计算 C 的一部分列
 * 每个进程拥有完整的 A (m x n) 和部分 B (n x k/p)
 */
static void get_columnwise_distribution(int rank, int num_procs, int m, int n,
                                        int k, int *out_start_col,
                                        int *out_cols) {
  /* 块循环分配：将 k 列按进程数平均分配 */
  int cols_per_proc = k / num_procs;
  int remainder = k % num_procs;

  // 每个进程的列数：前 remainder 个进程各多一列
  int cols = cols_per_proc + (rank < remainder ? 1 : 0);

  // 起始列位置 = 前面所有进程分配的列数总和
  // 前 remainder 个进程各有 (cols_per_proc + 1) 列，其余有 cols_per_proc 列
  int start_col;
  if (rank < remainder) {
    // 当前进程在前 remainder 个中，前面有 rank 个进程，每个有 (cols_per_proc +
    // 1) 列
    start_col = rank * (cols_per_proc + 1);
  } else {
    // 当前进程不在前 remainder 个中
    // 前面有 remainder 个进程各有 (cols_per_proc + 1) 列，加上 (rank -
    // remainder) 个进程各有 cols_per_proc 列
    start_col =
        remainder * (cols_per_proc + 1) + (rank - remainder) * cols_per_proc;
  }

  *out_start_col = start_col;
  *out_cols = cols;
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
    fprintf(stderr, "Usage: mpi_col_distrib_mat_mul <m> <n> <k>\n");
    fprintf(stderr, "   or: mpi_col_distrib_mat_mul <size>  (m=n=k=size)\n");
    return 1;
  }

  /* MPI 初始化 */
  MPI_Init(&argc, &argv);
  int rank, num_procs;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

  if (num_procs < 2) {
    if (rank == 0)
      fprintf(stderr, "Error: need at least 2 processes (require multiple "
                      "processes for collective operations)\n");
    MPI_Finalize();
    return 1;
  }

  // 初始化自定义MPI数据类型 (必须在所有进程上执行)
  MatrixParams::buildMPIType();

  if (rank == 0) {
    /* ==================== Rank 0: Root process ==================== */
    srand(42);

    /* 生成矩阵 A 和 B */
    double *A = alloc_matrix(m, n);
    double *B = alloc_matrix(n, k);
    double *C = alloc_matrix(m, k);
    fill_random(A, m, n);
    fill_random(B, n, k);

    printf("Matrix size: A(%d x %d) * B(%d x %d) = C(%d x %d)\n", m, n, n, k, m,
           k);
    printf("Processes: %d (Column-wise distribution)\n", num_procs);

    /* ===== 集合通信分发数据 ===== */
    double t_comm_start = MPI_Wtime();

    // 计算每个进程的列分配
    int *start_cols = (int *)malloc(num_procs * sizeof(int));
    int *col_counts = (int *)malloc(num_procs * sizeof(int));

    for (int r = 0; r < num_procs; r++) {
      get_columnwise_distribution(r, num_procs, m, n, k, &start_cols[r],
                                  &col_counts[r]);
    }

    // 广播 A 矩阵给所有进程（因为每个进程需要完整A来计算其C的部分列）
    MPI_Bcast(A, m * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // 分发 B 矩阵的列给各进程
    // 由于 B 是 row-major 存储，列数据不连续，需要使用 MPI_Type_vector
    // 或手动复制 这里采用简单方法：先广播整个 B
    // 矩阵，然后每个进程取自己需要的列
    int my_start_col, my_cols;
    get_columnwise_distribution(rank, num_procs, m, n, k, &my_start_col,
                                &my_cols);

    // 将 B 转置为 B_T（列优先存储），然后使用 Scatterv 分发
    // B_T[j * n + i] = B[i * k + j]，即 B 的第 j 列变成 B_T 的第 j 行
    double *B_T = NULL;
    if (rank == 0) {
      B_T = alloc_matrix(k, n); // k 行 n 列
      for (int i = 0; i < n; i++) {
        for (int j = 0; j < k; j++) {
          B_T[j * n + i] = B[i * k + j];
        }
      }
    }

    // 准备 Scatterv 参数：每个进程获取 B_T 的连续行块（对应 B 的连续列）
    int *sendcounts = (int *)malloc(num_procs * sizeof(int));
    int *displs = (int *)malloc(num_procs * sizeof(int));

    for (int r = 0; r < num_procs; r++) {
      int s, c;
      get_columnwise_distribution(r, num_procs, m, n, k, &s, &c);
      sendcounts[r] = c * n; // c 行，每行 n 个元素
      displs[r] = s * n;     // 起始行偏移
    }

    // 每个进程接收 B_T 的相应行块（即 B 的相应列）
    double *B_T_local = alloc_matrix(my_cols, n); // my_cols 行 n 列
    MPI_Scatterv(B_T, sendcounts, displs, MPI_DOUBLE, B_T_local, my_cols * n,
                 MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // B_T_local 是 B 的列的转置：B_T_local[j * n + i] = B[i * my_start_col + j]
    // 我们需要 B_local[i * my_cols + j] = B[i * (my_start_col + j)]
    // 所以 B_local[i * my_cols + j] = B_T_local[j * n + i]
    double *B_local = alloc_matrix(n, my_cols);
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < my_cols; j++) {
        B_local[i * my_cols + j] = B_T_local[j * n + i];
      }
    }
    free(B_T_local);
    if (rank == 0)
      free(B_T);
    free(sendcounts);
    free(displs);

    // 构造矩阵参数结构体
    MatrixParams params;
    params.m = m;
    params.n = n;
    params.k = k;
    params.rows = m;       // 每个进程都有完整的 m 行 A 矩阵
    params.cols = my_cols; // 每个进程计算 my_cols 列 C 矩阵
    params.compute_time = 0;

    // 广播矩阵参数（使用自定义结构体类型）
    MPI_Bcast(&params, 1, MatrixParams::MPI_type, 0, MPI_COMM_WORLD);

    double t_comm_end = MPI_Wtime();
    double comm_time = t_comm_end - t_comm_start;

    /* ===== 各进程独立计算 C(:, start_col:finish_col) = A * B(:,
     * start_col:finish_col) ===== */
    double *C_local = alloc_matrix(m, my_cols);
    double t_compute_start = MPI_Wtime();
    mat_mul_serial(A, B_local, C_local, m, n, my_cols);
    double t_compute_end = MPI_Wtime();
    double compute_time = t_compute_end - t_compute_start;

    /* 更新参数结构体并发送回根进程 */
    params.compute_time = compute_time;

    /* ===== 收集结果 ===== */
    double t_gather_start = MPI_Wtime();

    // 非 Root 进程先发送 C_local 到 Root
    if (rank != 0) {
      MPI_Send(C_local, m * my_cols, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    }

    // Root 进程接收所有其他进程的数据
    if (rank == 0) {
      // 先复制自己的部分
      for (int i = 0; i < m; i++) {
        for (int j = 0; j < my_cols; j++) {
          C[i * k + (my_start_col + j)] = C_local[i * my_cols + j];
        }
      }
      // 接收其他进程的数据
      for (int r = 1; r < num_procs; r++) {
        int r_start_col, r_cols;
        get_columnwise_distribution(r, num_procs, m, n, k, &r_start_col,
                                    &r_cols);
        double *recv_buf = alloc_matrix(m, r_cols);
        MPI_Recv(recv_buf, m * r_cols, MPI_DOUBLE, r, 0, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
        // 复制到 C 的正确位置
        for (int i = 0; i < m; i++) {
          for (int j = 0; j < r_cols; j++) {
            C[i * k + (r_start_col + j)] = recv_buf[i * r_cols + j];
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
    printf("Gather Time:   %.6f seconds (manual copy)\n", gather_time);
    printf("Total Time:    %.6f seconds\n", end_to_end_time);

    /* 验证正确性：串行计算并比较 */
    double *C_check = alloc_matrix(m, k);
    mat_mul_serial(A, B, C_check, m, n, k);

    double max_err = 0.0;
    int max_err_idx = -1;
    for (int i = 0; i < m * k; i++) {
      double err = fabs(C[i] - C_check[i]);
      if (err > max_err) {
        max_err = err;
        max_err_idx = i;
      }
    }

    if (max_err_idx >= 0) {
      int err_row = max_err_idx / k;
      int err_col = max_err_idx % k;
      printf("  Max error at C[%d][%d]: Parallel=%.6f, Serial=%.6f\n", err_row,
             err_col, C[max_err_idx], C_check[max_err_idx]);
    }

    if (max_err < 1e-8)
      printf("Verification: PASSED (max error = %.2e)\n", max_err);
    else
      printf("Verification: FAILED (max error = %.2e)\n", max_err);
    free(C_check);

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
    free(start_cols);
    free(col_counts);
    free(B_local);
    free(C_local);
  } else {
    /* ==================== 非 Root 进程 ==================== */
    // 计算基本参数（用于稍后接收数据）
    int my_start_col, my_cols;
    get_columnwise_distribution(rank, num_procs, m, n, k, &my_start_col,
                                &my_cols);

    // 先分配 A 矩阵缓冲区（与 Root 进程同步执行 MPI_Bcast）
    double *A_local = alloc_matrix(m, n);
    MPI_Bcast(A_local, m * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // 准备 Scatterv 参数（与 Root 进程相同）
    int *sendcounts = (int *)malloc(num_procs * sizeof(int));
    int *displs = (int *)malloc(num_procs * sizeof(int));

    for (int r = 0; r < num_procs; r++) {
      int s, c;
      get_columnwise_distribution(r, num_procs, m, n, k, &s, &c);
      sendcounts[r] = c * n; // c 行，每行 n 个元素
      displs[r] = s * n;     // 起始行偏移
    }

    // 接收 B_T 的相应行块（即 B 的相应列）
    double *B_T_local = alloc_matrix(my_cols, n); // my_cols 行 n 列
    MPI_Scatterv(NULL, sendcounts, displs, MPI_DOUBLE, B_T_local, my_cols * n,
                 MPI_DOUBLE, 0, MPI_COMM_WORLD);

    // B_T_local[j * n + i] = B[i * (my_start_col + j)]
    // 所以 B_local[i * my_cols + j] = B_T_local[j * n + i]
    double *B_local = alloc_matrix(n, my_cols);
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < my_cols; j++) {
        B_local[i * my_cols + j] = B_T_local[j * n + i];
      }
    }
    free(B_T_local);
    free(sendcounts);
    free(displs);

    // 接收矩阵参数（使用自定义结构体类型）- 与 Root 进程同步
    MatrixParams params;
    params.m = 0;
    params.n = 0;
    params.k = 0;
    params.rows = 0;
    params.cols = 0;
    params.compute_time = 0;
    MPI_Bcast(&params, 1, MatrixParams::MPI_type, 0, MPI_COMM_WORLD);

    // 计算 C_local = A_local * B_local (使用全局 m,n,k 而非 params 中的值)
    double *C_local = alloc_matrix(m, my_cols);
    double t_compute_start = MPI_Wtime();
    mat_mul_serial(A_local, B_local, C_local, m, n, my_cols);
    double t_compute_end = MPI_Wtime();
    double compute_time = t_compute_end - t_compute_start;

    // 发送 C_local 到 Root 进程
    MPI_Send(C_local, m * my_cols, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);

    // Participate in MPI_Reduce to get max compute time
    double max_compute_time;
    MPI_Reduce(&compute_time, &max_compute_time, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);

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