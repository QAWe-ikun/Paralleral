# Task5: 通用矩阵乘法 (GEMM) 并行实现

## 实验内容

本实验实现两种并行通用矩阵乘法：

1. **基于 OpenMP 的矩阵乘法** - 使用 OpenMP 指令实现并行
2. **基于 Pthreads parallel_for 的矩阵乘法** - 实现 parallel_for 动态链接库并用其实现矩阵乘法

## 目录结构

```
task5/
├── src/
│   ├── gemm_omp.c          # OpenMP 矩阵乘法
│   ├── parallel_for.h      # parallel_for 头文件
│   ├── parallel_for.c      # parallel_for 实现
│   └── gemm_pthreads.c     # 基于 parallel_for 的矩阵乘法
├── bin/                    # 可执行文件
├── lib/                    # 动态链接库
├── obj/                    # 目标文件
├── compile.sh              # 编译脚本
├── benchmark.sh            # 性能测试脚本
└── README.md               # 说明文档
```

## 编译

```bash
# Linux 环境
bash compile.sh

# 或手动编译
# 1. 编译 OpenMP 矩阵乘法
gcc -fopenmp -O2 -o bin/gemm_omp src/gemm_omp.c -lm

# 2. 编译 parallel_for 动态链接库
gcc -shared -fPIC -O2 -o lib/libparallel_for.so src/parallel_for.c -lpthread

# 3. 编译基于 parallel_for 的矩阵乘法
gcc -O2 -o bin/gemm_pthreads src/gemm_pthreads.c -Llib -lparallel_for -lpthread -lm -Isrc
```

## 运行

### OpenMP 矩阵乘法

```bash
./bin/gemm_omp <M> <N> <K> [num_threads]

# 示例
./bin/gemm_omp 512 512 512 4
./bin/gemm_omp 1024 1024 1024 8
```

### Pthreads parallel_for 矩阵乘法

```bash
# 设置库路径
export LD_LIBRARY_PATH=lib:$LD_LIBRARY_PATH

# 运行
./bin/gemm_pthreads <M> <N> <K> [num_threads]

# 示例
./bin/gemm_pthreads 512 512 512 4
./bin/gemm_pthreads 1024 1024 1024 8
```

## 性能测试

```bash
bash benchmark.sh
```

## 实验参数

- **矩阵规模**: 512, 1024, 2048
- **线程数**: 1, 2, 4, 8
- **调度策略**:
  - OpenMP: 默认调度、静态调度 (static, 1)、动态调度 (dynamic, 1)
  - Pthreads: 静态调度、动态调度、引导调度

## parallel_for 动态链接库

### API 说明

```c
// 基础 API
int parallel_for(int start, int end, int increment,
                 void *(*functor)(int, void *),
                 void *arg, int num_threads);

// 高级 API（支持调度策略）
int parallel_for_advanced(int start, int end, int increment,
                          void *(*functor)(int, void *),
                          void *arg, parallel_config_t *config);
```

### 使用示例

```c
#include "parallel_for.h"

// 定义 functor 参数
struct functor_args {
    float *A, *B, *C;
};

// 定义 functor 函数
void *functor(int idx, void *args) {
    struct functor_args *data = (struct functor_args *)args;
    data->C[idx] = data->A[idx] + data->B[idx];
    return NULL;
}

// 调用 parallel_for
struct functor_args args = {A, B, C};
parallel_for(0, 10, 1, functor, (void *)&args, 2);
```

## 矩阵乘法算法

### 公式

$$C = A \times B$$

$$C_{m,k} = \sum_{n=1}^{N} A_{m,n} \times B_{n,k}$$

### 并行策略

- **外层并行**: 将矩阵 C 的每一行分配给不同线程计算
- **内层串行**: 每个线程独立完成一行所有列的计算
- **无数据竞争**: 每个线程写入 C 的不同行，无需同步

## 性能分析

### OpenMP 调度策略比较

| 调度策略 | 特点 | 适用场景 |
|---------|------|---------|
| 默认 | 编译器决定 | 一般情况 |
| static | 预先平均分配 | 负载均匀 |
| dynamic | 运行时动态分配 | 负载不均 |

### Pthreads 调度策略比较

| 调度策略 | 特点 | 适用场景 |
|---------|------|---------|
| static | 预先平均分配 | 负载均匀 |
| dynamic | 固定块大小动态分配 | 负载不均 |
| guided | 块大小逐渐减小 | 负载未知 |

## 作者

Q <13610252512@139.com>