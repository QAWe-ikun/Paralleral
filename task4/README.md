# Task 4: Pthreads 一元二次方程求解 & 蒙特卡洛求π

## 概述

本任务使用 **POSIX pthreads** 实现两个并行计算程序，可在 **WSL (Windows Subsystem for Linux)** 中运行：
1. **一元二次方程求解** - 使用多线程和条件变量并行求解一元二次方程
2. **蒙特卡洛方法求π** - 使用多线程蒙特卡洛方法估算圆周率π

## 文件结构

```
task4/
├── src/
│   ├── pthread_quadratic.c        # 一元二次方程求解 (pthreads + 条件变量)
│   └── pthread_monte_carlo_pi.c   # 蒙特卡洛求π (pthreads)
├── bin/                            # 编译输出目录
├── obj/                            # 对象文件目录
├── compile.sh                      # 编译脚本 (WSL/Linux)
├── benchmark.sh                    # 性能测试脚本 (WSL/Linux)
└── README.md                       # 本文档
```

## 编译

### 在 WSL/Linux 中编译

```bash
cd task4
chmod +x compile.sh
./compile.sh
```

或手动编译：

```bash
# 编译一元二次方程求解
gcc -O2 -o bin/pthread_quadratic src/pthread_quadratic.c -lpthread -lm

# 编译蒙特卡洛求π
gcc -O2 -o bin/pthread_monte_carlo_pi src/pthread_monte_carlo_pi.c -lpthread
```

## 运行

### 1. 一元二次方程求解

```bash
bin/pthread_quadratic <a> <b> <c>
```

**参数说明：**
- `a, b, c`: 方程系数，范围 [-100, 100]，a ≠ 0
- 求解方程: ax² + bx + c = 0

**示例：**
```bash
# x^2 - 5x + 6 = 0 (根: 2, 3)
bin/pthread_quadratic 1 -5 6

# x^2 + 2x + 1 = 0 (重根: -1)
bin/pthread_quadratic 1 2 1

# x^2 + 1 = 0 (无实根)
bin/pthread_quadratic 1 0 1
```

### 2. 蒙特卡洛方法求π

```bash
bin/pthread_monte_carlo_pi <n> [num_threads]
```

**参数说明：**
- `n`: 采样点数，范围 [1024, 65536]
- `num_threads`: 线程数量，范围 [1, 16]，默认 4

**示例：**
```bash
# 1024 个采样点，4 线程
bin/pthread_monte_carlo_pi 1024 4

# 65536 个采样点，8 线程
bin/pthread_monte_carlo_pi 65536 8
```

## 性能测试

```bash
cd task4
chmod +x benchmark.sh
./benchmark.sh
```

## 算法说明

### 1. 一元二次方程求解

#### 求根公式
```
x = (-b ± √(b²-4ac)) / 2a
```

#### 线程分工
| 线程 | 任务 | 计算内容 |
|------|------|---------|
| 线程1 | 计算判别式 | Δ = b² - 4ac |
| 线程2 | 计算 -b | neg_b = -B |
| 线程3 | 计算 2a | two_a = 2A |
| 主线程 | 等待 + 计算最终解 | 使用条件变量等待所有中间结果 |

#### 条件变量使用
- 每个工作线程完成后通过 `pthread_cond_signal` 通知主线程
- 主线程通过 `pthread_cond_wait` 等待所有 3 个中间结果完成
- 使用 `completed_tasks` 计数器跟踪完成状态

### 2. 蒙特卡洛方法求π

#### 基本原理
- 在正方形 [-1,1]×[-1,1] 内随机撒点
- 统计落在内切圆 x²+y²≤1 内的点数比例
- π ≈ 4 × (圆内点数 / 总点数)

#### 线程分工
- 将采样点按线程数平均分配
- 每个线程使用 `rand_r` 生成独立的随机序列
- 使用互斥锁保护全局计数器

## 性能分析指标

- **加速比 (Speedup)**: S = T_serial / T_parallel
- **效率 (Efficiency)**: E = S / P × 100% (P 为线程数)

## 注意事项

1. 一元二次方程求解中，由于计算量很小，多线程的线程创建开销可能超过并行收益
2. 蒙特卡洛方法中，采样点数越多，π 的估计越精确
3. 每个线程使用不同的随机种子 (`42 + thread_id`) 确保随机性
4. 条件变量是解决线程间依赖关系的有效方式