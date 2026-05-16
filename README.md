# Parallel - 并行计算实验

并行计算课程实验项目，探索并行计算与性能优化的多种技术。

## 📁 实验列表

| 实验 | 描述 | 技术栈 | 状态 |
|------|------|--------|------|
| [task0](./task0/) | 矩阵乘法性能优化 | C++, MKL, Strassen | ✅ 完成 |
| [task1](./task1/) | MPI 点对点通信实现并行矩阵乘法 | MPI (Send/Recv) | ✅ 完成 |
| [task2](./task2/) | MPI 集合通信实现并行矩阵乘法 | MPI (Bcast, Scatterv, Gatherv) | ✅ 完成 |
| [task3](./task3/) | Pthreads 并行矩阵乘法与数组求和 | POSIX pthreads | ✅ 完成 |
| [task4](./task4/) | Pthreads 一元二次方程求解与蒙特卡洛求π | pthreads + 条件变量 | ✅ 完成 |
| [task5](./task5/) | OpenMP 与 Pthreads parallel_for 矩阵乘法 | OpenMP, Pthreads, 动态链接库 | ✅ 完成 |
| task6 | *待开发* | - | ⏳ 进行中 |

---

## 🎯 实验主题

### 并行编程模型

| 模型 | 实验 | 特点 |
|------|------|------|
| **MPI** | task1, task2 | 分布式内存，进程间通信 |
| **OpenMP** | task5 | 共享内存，编译器指令 |
| **Pthreads** | task3, task4, task5 | 共享内存，底层线程 API |

### 优化技术

- **循环顺序优化** (ikj vs ijk) - task0
- **Strassen 分治算法** - task0
- **Intel MKL 库** - task0
- **B 矩阵转置优化缓存** - task5
- **SIMD 向量化** - task0
- **多线程并行** - task3, task4, task5

---

## 🚀 快速开始

每个实验都有独立的目录和说明文档：

```bash
# 进入具体实验目录
cd task[x]

# 查看该实验的详细说明
cat README.md

# Windows (task0, task1, task2)
compile.bat

# Linux/WSL (task3, task4, task5)
chmod +x compile.sh
./compile.sh
```

---

## 📊 性能亮点

| 实验 | 最佳加速比 | 说明 |
|------|-----------|------|
| task0 (MKL) | ~30000x | Python vs Intel MKL |
| task0 (Strassen) | ~189x | 分治算法 |
| task1 (MPI) | ~12x | 16 进程，中小规模矩阵 |
| task2 (MPI) | ~12x | 集合通信优化 |
| task5 (OpenMP) | ~8x | 8 线程，2048×2048 矩阵 |
| task5 (Pthreads) | ~7.5x | 8 线程，2048×2048 矩阵 |

---

## 📝 实验报告

每个实验的详细报告位于对应目录的 `report/` 子目录中：

```
task0/report/实验报告.md
task1/report/实验报告.md
task2/report/实验报告.md
task3/report/实验报告.md
task4/report/实验报告.md
task5/report/实验报告.md
```

---

## 📚 实验内容详解

### task0: 矩阵乘法性能优化

实现多种矩阵乘法算法并对比性能：
- ijk 基础版本
- ikj 循环顺序优化（缓存友好）
- Strassen 分治算法（O(n^2.81)）
- Intel MKL 库调用

**关键发现**: 从 Python 到优化后的 MKL，性能提升近 30000 倍！

### task1: MPI 点对点通信矩阵乘法

使用 MPI_Send/MPI_Recv 实现 Master-Worker 模型：
- Rank 0 负责生成矩阵和分发任务
- Rank 1~P-1 负责计算并返回结果
- 分析不同矩阵规模下的最佳进程数

### task2: MPI 集合通信矩阵乘法

使用 MPI_Bcast、MPI_Scatterv、MPI_Gatherv 实现高效并行：
- **行划分版本**: 广播 B 矩阵，Scatterv 分发 A 的行
- **列划分版本**: 广播 A 矩阵，Scatterv 分发 B 的列（转置后）
- **2D 块划分版本**: 二维网格分布，负载均衡更优
- C++ 类封装 MPI 自定义数据类型

### task3: Pthreads 并行基础

两个经典并行算法实现：
- **并行矩阵乘法**: 行划分 vs 分块划分
- **并行数组求和**: 互斥锁累加 vs 树形聚合

### task4: Pthreads 进阶应用

- **一元二次方程求解**: 使用条件变量处理线程依赖
- **蒙特卡洛求π**: 大规模随机采样并行化
- 分析计算密度对并行效率的影响

### task5: OpenMP 与 parallel_for

- **OpenMP 版本**: 对比默认、静态 (static,1)、动态 (dynamic,1) 调度
- **Pthreads parallel_for**: 实现支持三种调度的动态链接库
- **B 矩阵转置优化**: 提升缓存命中率
- **墙上时间计时**: 正确使用 clock_gettime 而非 clock()

---

## 🔧 环境要求

| 实验 | 编译环境 | 运行环境 |
|------|---------|---------|
| task0 | GCC/MSVC | Windows/Linux |
| task1, task2 | MS-MPI | Windows |
| task3, task4, task5 | GCC + pthreads | Linux/WSL |

---

## 👥 作者

Q <13610252512@139.com>

中山大学计算机学院 - 并行程序设计与算法课程实验