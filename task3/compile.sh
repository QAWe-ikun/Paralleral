#!/bin/bash
# ============================================================================
# Pthreads 并行计算 - 编译脚本 (WSL/Linux)
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BIN_DIR="$SCRIPT_DIR/bin"
OBJ_DIR="$SCRIPT_DIR/obj"

echo "========================================"
echo "Pthreads Parallel Computing - Build Script"
echo "========================================"
echo "Source: $SRC_DIR"
echo "Output: $BIN_DIR"
echo "Obj:    $OBJ_DIR"
echo "========================================"

# Check gcc compiler
if ! command -v gcc &> /dev/null; then
    echo "Error: gcc compiler not found"
    echo "Please install gcc: sudo apt-get install gcc"
    exit 1
fi

# Create directories
mkdir -p "$BIN_DIR"
mkdir -p "$OBJ_DIR"

echo ""
echo "[1/2] Building pthread_mat_mul (Parallel Matrix Multiplication)..."
gcc -O2 -o "$BIN_DIR/pthread_mat_mul" "$SRC_DIR/pthread_mat_mul.c" -lpthread -lm
if [ $? -ne 0 ]; then
    echo "Build FAILED!"
else
    echo "Build SUCCESS: bin/pthread_mat_mul"
fi

echo ""
echo "[2/2] Building pthread_array_sum (Parallel Array Sum)..."
gcc -O2 -o "$BIN_DIR/pthread_array_sum" "$SRC_DIR/pthread_array_sum.c" -lpthread
if [ $? -ne 0 ]; then
    echo "Build FAILED!"
else
    echo "Build SUCCESS: bin/pthread_array_sum"
fi

echo ""
echo "========================================"
echo "Build Complete!"
echo "========================================"
echo ""
echo "Executables:"
ls -la "$BIN_DIR/" 2>/dev/null
echo ""
echo "Usage:"
echo "  Matrix Multiplication:"
echo "    bin/pthread_mat_mul <size> [num_threads] [strategy]"
echo "    strategy: 0=row division (default), 1=block division"
echo ""
echo "  Array Sum:"
echo "    bin/pthread_array_sum <n> [num_threads] [method]"
echo "    method: 0=direct sum (default), 1=tree aggregation"
echo ""