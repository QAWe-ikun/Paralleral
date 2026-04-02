import numpy as np
import time
import tqdm
import sys

def createMatrix(M: int, N: int) -> np.ndarray:
    return np.random.random((M, N))

def matrixMultiply(Mat1: np.ndarray, Mat2: np.ndarray) -> np.ndarray | None:
    if Mat1.shape[1] != Mat2.shape[0]:
        return None

    Mat3 = np.empty((Mat1.shape[0], Mat2.shape[1]))

    for i in tqdm.tqdm(range(0, Mat1.shape[0])):
        for j in range(0, Mat2.shape[1]):
            for k in range(0, Mat1.shape[1]):
                Mat3[i][j] += Mat1[i][k] * Mat2[k][j]

    return Mat3

def main(M: int = 1024, K: int = 1024, N: int = 1024):
    # 验证参数范围
    if not (512 <= M <= 2048 and 512 <= K <= 2048 and 512 <= N <= 2048):
        print("错误：M, K, N 的取值范围必须在 512 到 2048 之间")
        sys.exit(1)

    print("========================================")
    print("矩阵乘法 - Python 基础版本")
    print("========================================")
    print(f"矩阵大小：A[{M}x{K}] * B[{K}x{N}] = C[{M}x{N}]")

    Mat1, Mat2 = createMatrix(M, K), createMatrix(K, N)

    starttime = time.time()
    Mat3 = matrixMultiply(Mat1, Mat2)
    duration = time.time() - starttime

    print("----------------------------------------")
    print(f"计算时间：{duration:.3f} s")
    print(f"结果验证：sum(C) = {np.sum(Mat3):.6e}") # type: ignore
    print("========================================")

if __name__ == '__main__':
    if len(sys.argv) == 4:
        M, K, N = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
    elif len(sys.argv) == 2:
        size = int(sys.argv[1])
        M, K, N = size, size, size
    else:
        M, K, N = 1024, 1024, 1024
    main(M, K, N)
