#!/bin/bash
# Task5 性能测试脚本
# 测试 OpenMP 和 Pthreads parallel_for 矩阵乘法的性能

set -e

echo "========================================"
echo "  Task5 性能测试"
echo "========================================"

# 确保已编译
if [ ! -f bin/gemm_omp ] || [ ! -f bin/gemm_pthreads ]; then
    echo "正在编译..."
    bash compile.sh
fi

# 设置库路径
export LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH

# 测试配置
MATRIX_SIZES=(512 1024 2048)
THREAD_COUNTS=(1 2 4 8)

echo ""
echo "========================================"
echo "  OpenMP 矩阵乘法性能测试"
echo "========================================"

for M in "${MATRIX_SIZES[@]}"; do
    for N in "${MATRIX_SIZES[@]}"; do
        for K in "${MATRIX_SIZES[@]}"; do
            echo ""
            echo "--- 矩阵规模: ${M}×${N} × ${N}×${K} ---"
            for threads in "${THREAD_COUNTS[@]}"; do
                echo -n "  线程数 ${threads}: "
                ./bin/gemm_omp $M $N $K $threads 2>&1 | grep "加速比" | head -1
            done
        done
    done
done

echo ""
echo "========================================"
echo "  Pthreads parallel_for 矩阵乘法性能测试"
echo "========================================"

for M in "${MATRIX_SIZES[@]}"; do
    for N in "${MATRIX_SIZES[@]}"; do
        for K in "${MATRIX_SIZES[@]}"; do
            echo ""
            echo "--- 矩阵规模: ${M}×${N} × ${N}×${K} ---"
            for threads in "${THREAD_COUNTS[@]}"; do
                echo -n "  线程数 ${threads}: "
                ./bin/gemm_pthreads $M $N $K $threads 2>&1 | grep "加速比" | head -1
            done
        done
    done
done

echo ""
echo "========================================"
echo "  性能测试完成!"
echo "========================================"