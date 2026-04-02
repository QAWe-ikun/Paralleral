/**
 * 矩阵乘法 - 全部版本对比 (含 Strassen 算法)
 *
 * 用法：matrix_all_mkl [version] [M] [K] [N]
 *
 * version:
 *   0 - ijk 基础版本
 *   1 - ikj 循环顺序优化
 *   2 - Strassen 分治算法
 *   3 - Intel MKL 版本
 *   4 - 全部版本对比 (默认)
 *
 * M, K, N 取值范围：512-2048
 * 默认：1024 x 1024 x 1024
 */

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace chrono;

// MKL 支持
#ifdef USE_MKL
#include <mkl.h>
#endif

// ============================================================================
// 工具函数
// ============================================================================

vector<vector<double>> createMatrix2D(int M, int N) {
  vector<vector<double>> mat(M, vector<double>(N));
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j++) {
      mat[i][j] = static_cast<double>(rand()) / RAND_MAX;
    }
  }
  return mat;
}

vector<double> createMatrix1D(int M, int N) {
  vector<double> mat(M * N);
  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j++) {
      mat[i * N + j] = static_cast<double>(rand()) / RAND_MAX;
    }
  }
  return mat;
}

double computeSum(const vector<vector<double>> &C) {
  double sum = 0.0;
  for (const auto &row : C) {
    for (double val : row) {
      sum += val;
    }
  }
  return sum;
}

double computeSum1D(const vector<double> &C) {
  double sum = 0.0;
  for (double val : C) {
    sum += val;
  }
  return sum;
}

bool validateParams(int M, int K, int N) {
  if (M < 512 || M > 2048 || K < 512 || K > 2048 || N < 512 || N > 2048) {
    return false;
  }
  return true;
}

// ============================================================================
// 版本 0: ijk 基础版本
// ============================================================================

vector<vector<double>> matrixMultiply_ijk(const vector<vector<double>> &A,
                                          const vector<vector<double>> &B,
                                          int M, int K, int N) {
  vector<vector<double>> C(M, vector<double>(N, 0.0));

  for (int i = 0; i < M; i++) {
    for (int j = 0; j < N; j++) {
      for (int k = 0; k < K; k++) {
        C[i][j] += A[i][k] * B[k][j];
      }
    }
  }

  return C;
}

// ============================================================================
// 版本 1: ikj 循环顺序优化
// ============================================================================

vector<vector<double>> matrixMultiply_ikj(const vector<vector<double>> &A,
                                          const vector<vector<double>> &B,
                                          int M, int K, int N) {
  vector<vector<double>> C(M, vector<double>(N, 0.0));

  for (int i = 0; i < M; i++) {
    for (int k = 0; k < K; k++) {
      double a_ik = A[i][k];
      for (int j = 0; j < N; j++) {
        C[i][j] += a_ik * B[k][j];
      }
    }
  }

  return C;
}

// ============================================================================
// 版本 2: Strassen 分治算法
// ============================================================================

// 基础矩阵乘法 (用于小规模矩阵)
void multiplyBase(const vector<vector<double>> &A,
                  const vector<vector<double>> &B, vector<vector<double>> &C,
                  int size) {
  for (int i = 0; i < size; i++) {
    for (int k = 0; k < size; k++) {
      double a_ik = A[i][k];
      for (int j = 0; j < size; j++) {
        C[i][j] += a_ik * B[k][j];
      }
    }
  }
}

// 矩阵加法
void addMatrix(const vector<vector<double>> &A, const vector<vector<double>> &B,
               vector<vector<double>> &C, int size) {
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      C[i][j] = A[i][j] + B[i][j];
    }
  }
}

// 矩阵减法
void subMatrix(const vector<vector<double>> &A, const vector<vector<double>> &B,
               vector<vector<double>> &C, int size) {
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      C[i][j] = A[i][j] - B[i][j];
    }
  }
}

// Strassen 递归实现
void strassenRecursive(const vector<vector<double>> &A,
                       const vector<vector<double>> &B,
                       vector<vector<double>> &C, int n, int threshold) {
  // 基础情况：小规模矩阵使用普通乘法
  if (n <= threshold) {
    multiplyBase(A, B, C, n);
    return;
  }

  int half = n / 2;

  // 创建子矩阵
  vector<vector<double>> A11(half, vector<double>(half)),
      A12(half, vector<double>(half));
  vector<vector<double>> A21(half, vector<double>(half)),
      A22(half, vector<double>(half));
  vector<vector<double>> B11(half, vector<double>(half)),
      B12(half, vector<double>(half));
  vector<vector<double>> B21(half, vector<double>(half)),
      B22(half, vector<double>(half));

  // 分割矩阵
  for (int i = 0; i < half; i++) {
    for (int j = 0; j < half; j++) {
      A11[i][j] = A[i][j];
      A12[i][j] = A[i][j + half];
      A21[i][j] = A[i + half][j];
      A22[i][j] = A[i + half][j + half];
      B11[i][j] = B[i][j];
      B12[i][j] = B[i][j + half];
      B21[i][j] = B[i + half][j];
      B22[i][j] = B[i + half][j + half];
    }
  }

  // 计算 7 个乘积 (Strassen 公式)
  vector<vector<double>> M1(half, vector<double>(half, 0));
  vector<vector<double>> M2(half, vector<double>(half, 0));
  vector<vector<double>> M3(half, vector<double>(half, 0));
  vector<vector<double>> M4(half, vector<double>(half, 0));
  vector<vector<double>> M5(half, vector<double>(half, 0));
  vector<vector<double>> M6(half, vector<double>(half, 0));
  vector<vector<double>> M7(half, vector<double>(half, 0));

  // M1 = (A11 + A22) * (B11 + B22)
  vector<vector<double>> T1(half, vector<double>(half)),
      T2(half, vector<double>(half));
  addMatrix(A11, A22, T1, half);
  addMatrix(B11, B22, T2, half);
  strassenRecursive(T1, T2, M1, half, threshold);

  // M2 = (A21 + A22) * B11
  addMatrix(A21, A22, T1, half);
  strassenRecursive(T1, B11, M2, half, threshold);

  // M3 = A11 * (B12 - B22)
  subMatrix(B12, B22, T1, half);
  strassenRecursive(A11, T1, M3, half, threshold);

  // M4 = A22 * (B21 - B11)
  subMatrix(B21, B11, T1, half);
  strassenRecursive(A22, T1, M4, half, threshold);

  // M5 = (A11 + A12) * B22
  addMatrix(A11, A12, T1, half);
  strassenRecursive(T1, B22, M5, half, threshold);

  // M6 = (A21 - A11) * (B11 + B12)
  subMatrix(A21, A11, T1, half);
  addMatrix(B11, B12, T2, half);
  strassenRecursive(T1, T2, M6, half, threshold);

  // M7 = (A12 - A22) * (B21 + B22)
  subMatrix(A12, A22, T1, half);
  addMatrix(B21, B22, T2, half);
  strassenRecursive(T1, T2, M7, half, threshold);

  // 计算结果子矩阵
  vector<vector<double>> C11(half, vector<double>(half, 0)),
      C12(half, vector<double>(half, 0));
  vector<vector<double>> C21(half, vector<double>(half, 0)),
      C22(half, vector<double>(half, 0));

  // C11 = M1 + M4 - M5 + M7
  addMatrix(M1, M4, T1, half);
  subMatrix(T1, M5, T2, half);
  addMatrix(T2, M7, C11, half);

  // C12 = M3 + M5
  addMatrix(M3, M5, C12, half);

  // C21 = M2 + M4
  addMatrix(M2, M4, C21, half);

  // C22 = M1 - M2 + M3 + M6
  subMatrix(M1, M2, T1, half);
  addMatrix(T1, M3, T2, half);
  addMatrix(T2, M6, C22, half);

  // 合并结果
  for (int i = 0; i < half; i++) {
    for (int j = 0; j < half; j++) {
      C[i][j] = C11[i][j];
      C[i][j + half] = C12[i][j];
      C[i + half][j] = C21[i][j];
      C[i + half][j + half] = C22[i][j];
    }
  }
}

// Strassen 算法入口 (处理非 2 的幂次方阵)
vector<vector<double>> matrixMultiply_strassen(const vector<vector<double>> &A,
                                               const vector<vector<double>> &B,
                                               int M, int K, int N) {
  // 简化：仅支持方阵且大小为 2 的幂次
  int size = max({M, K, N});
  int paddedSize = 1;
  while (paddedSize < size)
    paddedSize *= 2;

  // 创建填充后的矩阵
  vector<vector<double>> A_pad(paddedSize, vector<double>(paddedSize, 0));
  vector<vector<double>> B_pad(paddedSize, vector<double>(paddedSize, 0));
  vector<vector<double>> C_pad(paddedSize, vector<double>(paddedSize, 0));

  // 复制数据
  for (int i = 0; i < M; i++)
    for (int j = 0; j < K; j++)
      A_pad[i][j] = A[i][j];

  for (int i = 0; i < K; i++)
    for (int j = 0; j < N; j++)
      B_pad[i][j] = B[i][j];

  // 使用 Strassen 算法
  int threshold = 64; // 阈值：小于此大小使用普通乘法
  strassenRecursive(A_pad, B_pad, C_pad, paddedSize, threshold);

  // 提取结果
  vector<vector<double>> C(M, vector<double>(N, 0));
  for (int i = 0; i < M; i++)
    for (int j = 0; j < N; j++)
      C[i][j] = C_pad[i][j];

  return C;
}

// ============================================================================
// 版本 3: Intel MKL 版本
// ============================================================================

#ifdef USE_MKL
void matrixMultiply_mkl(const double *A, const double *B, double *C, int M,
                        int K, int N) {
  double alpha = 1.0;
  double beta = 0.0;

  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, M, N, K, alpha, A, K,
              B, N, beta, C, N);
}
#endif

// ============================================================================
// 性能测试框架
// ============================================================================

struct Result {
  string name;
  double time_ms;
  double sum;
};

void printHeader(int M, int K, int N) {
  cout << "========================================" << endl;
  cout << "矩阵乘法 - 性能对比" << endl;
  cout << "========================================" << endl;
  cout << "矩阵大小：A[" << M << "x" << K << "] * B[" << K << "x" << N
       << "] = C[" << M << "x" << N << "]" << endl;
  cout << "编译优化：";
#ifdef __OPTIMIZE__
  cout << "已启用 (-O2/-O3)" << endl;
#else
  cout << "未启用 (-O0/-Od)" << endl;
#endif
  cout << "----------------------------------------" << endl;
}

void printResults(const vector<Result> &results) {
  cout << "----------------------------------------" << endl;
  cout << left << setw(20) << "版本" << right << setw(12) << "时间 (ms)"
       << setw(15) << "sum(C)" << endl;
  cout << "----------------------------------------" << endl;

  for (const auto &r : results) {
    cout << left << setw(20) << r.name << right << setw(12) << fixed
         << setprecision(3) << r.time_ms << setw(15) << scientific
         << setprecision(6) << r.sum << endl;
  }

  if (results.size() >= 2) {
    cout << "----------------------------------------" << endl;
    cout << "加速比 (相对于 ijk 基础版):" << endl;
    double base = results[0].time_ms;
    for (size_t i = 1; i < results.size(); i++) {
      if (results[i].time_ms > 0) {
        cout << "  " << results[i].name << ": " << fixed << setprecision(2)
             << (base / results[i].time_ms) << "x" << endl;
      }
    }
  }
  cout << "========================================" << endl;
}

// ============================================================================
// 主函数
// ============================================================================

int main(int argc, char *argv[]) {
  int version = 4; // 默认运行全部
  int M = 1024, K = 1024, N = 1024;

  // 解析参数
  if (argc >= 2) {
    version = atoi(argv[1]);
  }
  if (argc >= 5) {
    M = atoi(argv[2]);
    K = atoi(argv[3]);
    N = atoi(argv[4]);
  } else if (argc == 3) {
    int size = atoi(argv[2]);
    M = K = N = size;
  }

  // 验证参数
  if (!validateParams(M, K, N)) {
    cerr << "错误：M, K, N 的取值范围必须在 512 到 2048 之间!" << endl;
    cerr << "用法：" << argv[0] << " [version] [M] [K] [N]" << endl;
    cerr << "  version: 0=ijk, 1=ikj, 2=Strassen, 3=MKL, 4=all" << endl;
    return 1;
  }

  srand(42);

  // 运行指定版本
  if (version >= 0 && version <= 2) {
    vector<vector<double>> A = createMatrix2D(M, K);
    vector<vector<double>> B = createMatrix2D(K, N);

    cout << "========================================" << endl;
    if (version == 0)
      cout << "矩阵乘法 - ijk 基础版本" << endl;
    else if (version == 1)
      cout << "矩阵乘法 - ikj 循环优化" << endl;
    else if (version == 2)
      cout << "矩阵乘法 - Strassen 分治算法" << endl;
    cout << "========================================" << endl;
    cout << "矩阵大小：A[" << M << "x" << K << "] * B[" << K << "x" << N
         << "] = C[" << M << "x" << N << "]" << endl;

    cout << "正在计算..." << endl;
    auto start = high_resolution_clock::now();

    vector<vector<double>> C;
    if (version == 0)
      C = matrixMultiply_ijk(A, B, M, K, N);
    else if (version == 1)
      C = matrixMultiply_ikj(A, B, M, K, N);
    else if (version == 2)
      C = matrixMultiply_strassen(A, B, M, K, N);

    auto end = high_resolution_clock::now();

    double duration_ms =
        duration_cast<microseconds>(end - start).count() / 1000.0;
    cout << "计算时间：" << fixed << setprecision(3) << duration_ms << " ms"
         << endl;
    cout << "结果验证：sum(C) = " << scientific << setprecision(6)
         << computeSum(C) << endl;
    cout << "========================================" << endl;

  } else if (version == 3) {
#ifdef USE_MKL
    cout << "========================================" << endl;
    cout << "矩阵乘法 - Intel MKL 版本" << endl;
    cout << "========================================" << endl;
    cout << "矩阵大小：A[" << M << "x" << K << "] * B[" << K << "x" << N
         << "] = C[" << M << "x" << N << "]" << endl;

    char version_buffer[256];
    mkl_get_version_string(version_buffer, sizeof(version_buffer));
    cout << "MKL 版本：" << version_buffer << endl;

    vector<double> A = createMatrix1D(M, K);
    vector<double> B = createMatrix1D(K, N);
    vector<double> C(M * N, 0.0);

    cout << "正在计算..." << endl;
    auto start = high_resolution_clock::now();
    matrixMultiply_mkl(A.data(), B.data(), C.data(), M, K, N);
    auto end = high_resolution_clock::now();

    double duration_ms =
        duration_cast<microseconds>(end - start).count() / 1000.0;
    cout << "计算时间：" << fixed << setprecision(3) << duration_ms << " ms"
         << endl;
    cout << "结果验证：sum(C) = " << scientific << setprecision(6)
         << computeSum1D(C) << endl;
    cout << "========================================" << endl;
#else
    cout << "错误：MKL 未启用。请使用 -DUSE_MKL 编译并链接 MKL 库。" << endl;
#endif

  } else {
    // 全部版本对比
    printHeader(M, K, N);

    vector<Result> results;

    // 版本 0: ijk
    {
      srand(42);
      vector<vector<double>> A = createMatrix2D(M, K);
      vector<vector<double>> B = createMatrix2D(K, N);

      auto start = high_resolution_clock::now();
      vector<vector<double>> C = matrixMultiply_ijk(A, B, M, K, N);
      auto end = high_resolution_clock::now();

      results.push_back(
          {"ijk 基础版",
           duration_cast<microseconds>(end - start).count() / 1000.0,
           computeSum(C)});
    }

    // 版本 1: ikj
    {
      srand(42);
      vector<vector<double>> A = createMatrix2D(M, K);
      vector<vector<double>> B = createMatrix2D(K, N);

      auto start = high_resolution_clock::now();
      vector<vector<double>> C = matrixMultiply_ikj(A, B, M, K, N);
      auto end = high_resolution_clock::now();

      results.push_back(
          {"ikj 循环优化",
           duration_cast<microseconds>(end - start).count() / 1000.0,
           computeSum(C)});
    }

    // 版本 2: Strassen
    {
      srand(42);
      vector<vector<double>> A = createMatrix2D(M, K);
      vector<vector<double>> B = createMatrix2D(K, N);

      auto start = high_resolution_clock::now();
      vector<vector<double>> C = matrixMultiply_strassen(A, B, M, K, N);
      auto end = high_resolution_clock::now();

      results.push_back(
          {"Strassen 算法",
           duration_cast<microseconds>(end - start).count() / 1000.0,
           computeSum(C)});
    }

    // 版本 3: MKL
#ifdef USE_MKL
    {
      srand(42);
      vector<double> A = createMatrix1D(M, K);
      vector<double> B = createMatrix1D(K, N);
      vector<double> C(M * N, 0.0);

      auto start = high_resolution_clock::now();
      matrixMultiply_mkl(A.data(), B.data(), C.data(), M, K, N);
      auto end = high_resolution_clock::now();

      results.push_back(
          {"Intel MKL",
           duration_cast<microseconds>(end - start).count() / 1000.0,
           computeSum1D(C)});
    }
#endif

    printResults(results);
  }

  return 0;
}
