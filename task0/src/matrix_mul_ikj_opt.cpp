/**
 * 矩阵乘法 - 循环顺序优化版本 (ikj)
 * 
 * 仅通过调整循环顺序优化，无循环展开，无 MKL
 * 编译优化：-O3 -march=native
 * 
 * 用法：matrix_ikj [M] [K] [N]
 * M, K, N 取值范围：512-2048
 * 默认：1024 x 1024 x 1024
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstdlib>

using namespace std;
using namespace chrono;

// 创建随机矩阵
vector<vector<double>> createMatrix(int M, int N) {
    vector<vector<double>> mat(M, vector<double>(N));
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            mat[i][j] = static_cast<double>(rand()) / RAND_MAX;
        }
    }
    return mat;
}

// 矩阵乘法 - ikj 顺序 (缓存优化版本)
vector<vector<double>> matrixMultiply_ikj(const vector<vector<double>>& A,
                                           const vector<vector<double>>& B,
                                           int M, int K, int N) {
    vector<vector<double>> C(M, vector<double>(N, 0.0));

    // ikj 循环顺序 - 优化的缓存访问模式
    for (int i = 0; i < M; i++) {
        for (int k = 0; k < K; k++) {
            double a_ik = A[i][k];  // 缓存 A[i][k] 避免重复访问
            for (int j = 0; j < N; j++) {
                C[i][j] += a_ik * B[k][j];
            }
        }
    }

    return C;
}

// 验证参数范围
bool validateParams(int M, int K, int N) {
    if (M < 512 || M > 2048 || K < 512 || K > 2048 || N < 512 || N > 2048) {
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    int M = 1024, K = 1024, N = 1024;

    if (argc >= 4) {
        M = atoi(argv[1]);
        K = atoi(argv[2]);
        N = atoi(argv[3]);
    } else if (argc == 2) {
        int size = atoi(argv[1]);
        M = K = N = size;
    }

    if (!validateParams(M, K, N)) {
        cerr << "错误：M, K, N 的取值范围必须在 512 到 2048 之间!" << endl;
        cerr << "用法：" << argv[0] << " [M] [K] [N]" << endl;
        return 1;
    }

    cout << "========================================" << endl;
    cout << "矩阵乘法 - 循环顺序优化 (ikj)" << endl;
    cout << "========================================" << endl;
    cout << "矩阵大小：A[" << M << "x" << K << "] * B[" << K << "x" << N << "] = C[" << M << "x" << N << "]" << endl;
    cout << "优化策略：" << endl;
    cout << "  - 循环顺序：i -> k -> j" << endl;
    cout << "  - 缓存 A[i][k] 避免重复访问" << endl;
    cout << "  - 连续访问 C[i][j] 和 B[k][j]" << endl;
    cout << "  - 编译优化：-O3 -march=native" << endl;

    srand(42);

    cout << "正在创建矩阵..." << endl;
    auto start_total = high_resolution_clock::now();

    vector<vector<double>> A = createMatrix(M, K);
    vector<vector<double>> B = createMatrix(K, N);

    cout << "正在计算矩阵乘法..." << endl;
    auto start = high_resolution_clock::now();

    vector<vector<double>> C = matrixMultiply_ikj(A, B, M, K, N);

    auto end = high_resolution_clock::now();
    auto end_total = high_resolution_clock::now();

    double duration_ms = duration_cast<microseconds>(end - start).count() / 1000.0;
    double total_duration_ms = duration_cast<microseconds>(end_total - start_total).count() / 1000.0;

    cout << "----------------------------------------" << endl;
    cout << "计算时间：" << fixed << setprecision(3) << duration_ms << " ms" << endl;
    cout << "总时间 (含创建): " << fixed << setprecision(3) << total_duration_ms << " ms" << endl;

    double sum = 0.0;
    for (const auto& row : C) {
        for (double val : row) {
            sum += val;
        }
    }
    cout << "结果验证：sum(C) = " << scientific << setprecision(6) << sum << endl;
    cout << "========================================" << endl;

    return 0;
}
