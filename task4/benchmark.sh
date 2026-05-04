#!/bin/bash
# ============================================================================
# Task 4: Pthreads 一元二次方程求解 & 蒙特卡洛求π - 性能测试脚本 (WSL/Linux)
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$SCRIPT_DIR/bin"

echo "========================================"
echo "Task 4 - Benchmark"
echo "========================================"
echo ""

# ============================================================================
# 任务 1: 一元二次方程求解
# ============================================================================
echo "===== 任务 1: 一元二次方程求解 ====="
echo ""

# 测试不同的方程
echo "Test 1: x^2 - 5x + 6 = 0 (roots: 2, 3)"
"$BIN_DIR/pthread_quadratic" 1 -5 6
echo ""

echo "Test 2: x^2 + 2x + 1 = 0 (double root: -1)"
"$BIN_DIR/pthread_quadratic" 1 2 1
echo ""

echo "Test 3: x^2 + 1 = 0 (no real roots)"
"$BIN_DIR/pthread_quadratic" 1 0 1
echo ""

echo "Test 4: 2x^2 - 4x + 1 = 0"
"$BIN_DIR/pthread_quadratic" 2 -4 1
echo ""

echo "Test 5: -3x^2 + 12x - 9 = 0"
"$BIN_DIR/pthread_quadratic" -3 12 -9
echo ""

# ============================================================================
# 任务 2: 蒙特卡洛方法求π
# ============================================================================
echo ""
echo "===== 任务 2: 蒙特卡洛方法求π ====="
echo ""

SAMPLE_SIZES=(1024 2048 4096 8192 16384 32768 65536)
THREAD_COUNTS=(1 2 4 8 16)

for n in "${SAMPLE_SIZES[@]}"; do
    echo "Samples: $n"

    # 先运行单线程获取串行基准时间
    serial_time=$("$BIN_DIR/pthread_monte_carlo_pi" "$n" 1 2>&1 | grep "Serial Time:" | awk '{print $3}')
    serial_pi=$("$BIN_DIR/pthread_monte_carlo_pi" "$n" 1 2>&1 | grep "Serial π:" | awk '{print $3}')

    if [ -z "$serial_time" ]; then
        echo "  Warning: Could not get serial time, skipping..."
        continue
    fi
    echo "  Serial: Time=${serial_time}s, π=${serial_pi}"

    # 测试不同线程数
    for threads in "${THREAD_COUNTS[@]}"; do
        output=$("$BIN_DIR/pthread_monte_carlo_pi" "$n" "$threads" 2>&1)
        parallel_time=$(echo "$output" | grep "Parallel Time:" | awk '{print $3}')
        parallel_pi=$(echo "$output" | grep "Parallel π:" | awk '{print $3}')
        speedup=$(echo "$output" | grep "Speedup:" | awk '{print $2}')

        if [ -n "$parallel_time" ]; then
            echo "  Threads=$threads: Time=${parallel_time}s, π=${parallel_pi}, Speedup=${speedup}x"
        fi
    done
    echo ""
done

echo "========================================"
echo "Benchmark Complete!"
echo "========================================"