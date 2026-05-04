#!/bin/bash
# ============================================================================
# Task 4: Pthreads 一元二次方程求解 & 蒙特卡洛求π - 编译脚本 (WSL/Linux)
# ============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$SCRIPT_DIR/src"
BIN_DIR="$SCRIPT_DIR/bin"
OBJ_DIR="$SCRIPT_DIR/obj"

echo "========================================"
echo "Task 4 - Build Script"
echo "========================================"
echo "Source: $SRC_DIR"
echo "Output: $BIN_DIR"
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
echo "[1/2] Building pthread_quadratic (Quadratic Equation Solver)..."
gcc -O2 -o "$BIN_DIR/pthread_quadratic" "$SRC_DIR/pthread_quadratic.c" -lpthread -lm
if [ $? -ne 0 ]; then
    echo "Build FAILED!"
else
    echo "Build SUCCESS: bin/pthread_quadratic"
fi

echo ""
echo "[2/2] Building pthread_monte_carlo_pi (Monte Carlo Pi Estimator)..."
gcc -O2 -o "$BIN_DIR/pthread_monte_carlo_pi" "$SRC_DIR/pthread_monte_carlo_pi.c" -lpthread
if [ $? -ne 0 ]; then
    echo "Build FAILED!"
else
    echo "Build SUCCESS: bin/pthread_monte_carlo_pi"
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
echo "  Quadratic Equation:"
echo "    bin/pthread_quadratic <a> <b> <c>"
echo ""
echo "  Monte Carlo Pi:"
echo "    bin/pthread_monte_carlo_pi <n> [num_threads]"
echo ""