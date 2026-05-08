#!/bin/bash
# Task5 编译脚本
# 编译 OpenMP 矩阵乘法和 Pthreads parallel_for 动态链接库

set -e

echo "========================================"
echo "  Task5 编译脚本"
echo "========================================"

# 创建必要的目录
mkdir -p bin obj lib

# 1. 编译 OpenMP 矩阵乘法
echo ""
echo "[1] 编译 OpenMP 矩阵乘法..."
gcc -fopenmp -O2 -o bin/gemm_omp src/gemm_omp.c -lm
echo "    完成: bin/gemm_omp"

# 2. 编译 Pthreads parallel_for 动态链接库
echo ""
echo "[2] 编译 Pthreads parallel_for 动态链接库..."
gcc -shared -fPIC -O2 -o lib/libparallel_for.so src/parallel_for.c -lpthread
echo "    完成: lib/libparallel_for.so"

# 3. 编译基于 parallel_for 的矩阵乘法
echo ""
echo "[3] 编译基于 parallel_for 的矩阵乘法..."
gcc -O2 -o bin/gemm_pthreads src/gemm_pthreads.c -Llib -lparallel_for -lpthread -lm -Isrc
echo "    完成: bin/gemm_pthreads"

echo ""
echo "========================================"
echo "  编译完成!"
echo "========================================"
echo ""
echo "可执行文件:"
echo "  - bin/gemm_omp       (OpenMP 矩阵乘法)"
echo "  - bin/gemm_pthreads  (Pthreads parallel_for 矩阵乘法)"
echo ""
echo "动态链接库:"
echo "  - lib/libparallel_for.so"
echo ""
echo "使用方法:"
echo "  ./bin/gemm_omp <M> <N> <K> [num_threads]"
echo "  export LD_LIBRARY_PATH=lib:\$LD_LIBRARY_PATH"
echo "  ./bin/gemm_pthreads <M> <N> <K> [num_threads]"