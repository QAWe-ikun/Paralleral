# Task 3: Pthreads 并行计算

## 概述

本任务使用 **POSIX pthreads** 实现两个并行计算程序，可在 **WSL (Windows Subsystem for Linux)** 中运行：

1. **并行矩阵乘法** - 使用多线程实现矩阵乘法 C = A × B
2. **并行数组求和** - 使用多线程实现数组元素求和

## 文件结构

```
task3/
├── src/
│   ├── pthread_mat_mul.c      # 并行矩阵乘法 (pthreads)
│   └── pthread_array_sum.c    # 并行数组求和 (pthreads)
├── bin/                        # 编译输出目录
├── obj/                        # 对象文件目录
├── compile.sh                  # 编译脚本 (WSL/Linux)
├── benchmark.sh                # 性能测试脚本 (WSL/Linux)
└── README.md                   # 本文档
```

## 编译

### 在 WSL/Linux 中编译

```bash
cd task3
chmod +x compile.sh
./compile.sh
```

或手动编译：

```bash
# 编译矩阵乘法
gcc -O2 -o bin/pthread_mat_mul src/pthread_mat_mul.c -lpthread -lm

# 编译数组求和
gcc -O2 -o bin/pthread_array_sum src/pthread_array_sum.c -lpthread
```

## 运行

### 1. 并行矩阵乘法

```bash
# 用法 1: 指定 m, n, k
bin/pthread_mat_mul <m> <n> <k> [num_threads] [strategy]

# 用法 2: 方阵 (m=n=k)
bin/pthread_mat_mul <size> [num_threads] [strategy]
```

**参数说明：**

- `m, n, k`: 矩阵维度，范围 [128, 2048]
- `num_threads`: 线程数量，范围 [1, 16]，默认 4
- `strategy`: 数据划分策略
  - `0` - 行划分 (默认)
  - `1` - 分块划分

**示例：**

```bash
# 512x512 矩阵，4 线程，行划分
bin/pthread_mat_mul 512 4 0

# 1024x512x256 矩阵，8 线程，分块划分
bin/pthread_mat_mul 1024 512 256 8 1
```

### 2. 并行数组求和

```bash
bin/pthread_array_sum <n> [num_threads] [method]
```

**参数说明：**

- `n`: 数组长度，范围 [1M, 128M]
- `num_threads`: 线程数量，范围 [1, 16]，默认 4
- `method`: 聚合方式
  - `0` - 直接累加 (默认)
  - `1` - 树形聚合

**示例：**

```bash
# 1000 万元素，4 线程，直接累加
bin/pthread_array_sum 10000000 4 0

# 1 亿元素，8 线程，树形聚合
bin/pthread_array_sum 100000000 8 1
```

## 性能测试

### WSL/Linux

```bash
cd task3
chmod +x benchmark.sh
./benchmark.sh
```

## 算法说明

### 1. 并行矩阵乘法

#### 行划分 (Row Division)

- 将结果矩阵 C 的行按线程数平均分配
- 每个线程计算 C 的指定行范围
- 所有线程共享完整的 A 和 B 矩阵

#### 分块划分 (Block Division)

- 将结果矩阵 C 划分为固定大小的块 (默认 64×64)
- 将块按线程数平均分配
- 每个线程计算分配到的块

### 2. 并行数组求和

#### 直接累加 (Direct Sum)

- 将数组按线程数平均分配
- 每个线程计算局部和
- 使用互斥锁 (pthread_mutex) 保护全局和

#### 树形聚合 (Tree Aggregation)

- 将数组按线程数平均分配
- 每个线程计算局部和并存储到专属位置
- 主线程合并所有局部和

## 性能分析指标

- **加速比 (Speedup)**: S = T_serial / T_parallel
- **效率 (Efficiency)**: E = S / P × 100% (P 为线程数)

## 注意事项

1. 本实现使用 POSIX pthreads，可在 WSL 或 Linux 中运行
2. 矩阵乘法使用 `ikj` 循环顺序以优化缓存性能
3. 数组求和的直接累加方式使用互斥锁保护全局和
4. 数组求和的树形聚合方式避免了锁的竞争开销
5. 大规模矩阵 (2048×2048) 需要约 32MB 内存 per 矩阵

## 在 WSL 中运行

```bash

# 1. 进入 WSL

wsl

# 2. 进入项目目录 (假设项目挂载在 /mnt/d/)

cd ./Parallel/task3

# 3. 编译

chmod +x compile.sh
./compile.sh

# 4. 运行测试

bin/pthread_mat_mul 512 4 0

# 5. 运行完整 benchmark

chmod +x benchmark.sh
./benchmark.sh