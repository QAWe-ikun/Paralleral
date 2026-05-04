#!/bin/bash
# ============================================================================
# Pthreads 并行计算 - 性能测试脚本 (WSL/Linux)
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN_DIR="$SCRIPT_DIR/bin"

echo "========================================"
echo "Pthreads Parallel Computing - Benchmark"
echo "========================================"
echo ""

# ============================================================================
# 任务 1: 并行矩阵乘法
# ============================================================================
echo "===== 任务 1: 并行矩阵乘法 ====="
echo ""

MAT_SIZES=(128 256 512 1024 2048)
THREAD_COUNTS=(1 2 4 8 16)

# 存储结果
declare -a ROW_SPEEDUPS
declare -a BLOCK_SPEEDUPS

for size in "${MAT_SIZES[@]}"; do
    echo "Matrix size: ${size} x ${size} x ${size}"
    
    # 先运行单线程获取串行基准时间
    serial_time=$("$BIN_DIR/pthread_mat_mul" "$size" 1 0 2>&1 | grep "Serial Time:" | awk '{print $3}')
    
    if [ -z "$serial_time" ]; then
        echo "  Warning: Could not get serial time, skipping..."
        continue
    fi
    echo "  Serial Time: $serial_time s"
    
    # 测试不同线程数
    for threads in "${THREAD_COUNTS[@]}"; do
        # 行划分
        output=$("$BIN_DIR/pthread_mat_mul" "$size" "$threads" 0 2>&1)
        parallel_time=$(echo "$output" | grep "Parallel Time:" | awk '{print $3}')
        speedup=$(echo "$output" | grep "Speedup:" | awk '{print $2}')
        efficiency=$(echo "$output" | grep "Efficiency:" | awk '{print $2}')
        
        if [ -n "$parallel_time" ]; then
            echo "  Threads=$threads (Row): Time=${parallel_time}s, Speedup=${speedup}x, Efficiency=${efficiency}%"
        fi
        
        # 分块划分 (只测试 4, 8, 16 线程以节省时间)
        if [ "$threads" -ge 4 ]; then
            output=$("$BIN_DIR/pthread_mat_mul" "$size" "$threads" 1 2>&1)
            parallel_time=$(echo "$output" | grep "Parallel Time:" | awk '{print $3}')
            speedup=$(echo "$output" | grep "Speedup:" | awk '{print $2}')
            efficiency=$(echo "$output" | grep "Efficiency:" | awk '{print $2}')
            
            if [ -n "$parallel_time" ]; then
                echo "  Threads=$threads (Block): Time=${parallel_time}s, Speedup=${speedup}x, Efficiency=${efficiency}%"
            fi
        fi
    done
    echo ""
done

# ============================================================================
# 任务 2: 并行数组求和
# ============================================================================
echo ""
echo "===== 任务 2: 并行数组求和 ====="
echo ""

ARRAY_SIZES=(1000000 10000000 50000000 100000000 128000000)  # 1M, 10M, 50M, 100M, 128M

for n in "${ARRAY_SIZES[@]}"; do
    echo "Array size: $n"
    
    # 先运行单线程获取串行基准时间
    serial_time=$("$BIN_DIR/pthread_array_sum" "$n" 1 0 2>&1 | grep "Serial Time:" | awk '{print $3}')
    
    if [ -z "$serial_time" ]; then
        echo "  Warning: Could not get serial time, skipping..."
        continue
    fi
    echo "  Serial Time: $serial_time s"
    
    # 测试不同线程数
    for threads in "${THREAD_COUNTS[@]}"; do
        # 直接累加
        output=$("$BIN_DIR/pthread_array_sum" "$n" "$threads" 0 2>&1)
        parallel_time=$(echo "$output" | grep "Parallel Time:" | awk '{print $3}')
        speedup=$(echo "$output" | grep "Speedup:" | awk '{print $2}')
        
        if [ -n "$parallel_time" ]; then
            echo "  Threads=$threads (Direct): Time=${parallel_time}s, Speedup=${speedup}x"
        fi
        
        # 树形聚合 (只测试 4, 8, 16 线程以节省时间)
        if [ "$threads" -ge 4 ]; then
            output=$("$BIN_DIR/pthread_array_sum" "$n" "$threads" 1 2>&1)
            parallel_time=$(echo "$output" | grep "Parallel Time:" | awk '{print $3}')
            speedup=$(echo "$output" | grep "Speedup:" | awk '{print $2}')
            
            if [ -n "$parallel_time" ]; then
                echo "  Threads=$threads (Tree):   Time=${parallel_time}s, Speedup=${speedup}x"
            fi
        fi
    done
    echo ""
done

echo "========================================"
echo "Benchmark Complete!"
echo "========================================"