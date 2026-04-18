# Task2 - MPI 集合通信实现并行矩阵乘法

使用 MPI 集合通信（MPI_Bcast, MPI_Scatterv, MPI_Gatherv）实现并行通用矩阵乘法 C = A × B，并尝试不同的数据/任务划分方式。

## 目录结构

```
task2/
├── src/
│   ├── mpi_collective_mat_mul.cpp      # MPI 集合通信矩阵乘法源码（行划分，使用 MPI_Type_create_struct，C++类封装）
│   ├── mpi_col_distrib_mat_mul.cpp     # MPI 列划分矩阵乘法源码（对比不同任务划分方式，C++类封装）
│   └── mpi_2d_block_mat_mul.cpp        # MPI 2D 块划分矩阵乘法源码（C++类封装）
├── bin/
│   ├── mpi_collective_mat_mul.exe      # 行划分版编译产物
│   ├── mpi_col_distrib_mat_mul.exe     # 列划分版编译产物
│   └── mpi_2d_block_mat_mul.exe        # 2D 块划分版编译产物
├── compile.bat                         # 编译脚本
└── benchmark.ps1                       # 性能测试脚本
```

## 编译方法

```cmd
cd task2
compile.bat
```

使用 MSVC 编译器 + MS-MPI SDK 编译，所有程序均为 C++ 实现。

## 运行方法

### 命令行参数

```cmd
mpiexec -n <进程数> bin\mpi_collective_mat_mul.exe <m> <n> <k>
mpiexec -n <进程数> bin\mpi_collective_mat_mul.exe <size>  # m=n=k=size
mpiexec -n <进程数> bin\mpi_col_distrib_mat_mul.exe <m> <n> <k>  # 列划分版本
mpiexec -n <进程数> bin\mpi_col_distrib_mat_mul.exe <size>       # 列划分版本
mpiexec -n <进程数> bin\mpi_2d_block_mat_mul.exe <m> <n> <k>     # 2D 块划分版本
mpiexec -n <进程数> bin\mpi_2d_block_mat_mul.exe <size>          # 2D 块划分版本
```

### 示例

```cmd
# 2 个进程，128×128×128 矩阵，使用行划分
mpiexec.exe -n 2 bin\mpi_collective_mat_mul.exe 128

# 2 个进程，128×128×128 矩阵，使用列划分（对比实验）
mpiexec.exe -n 2 bin\mpi_col_distrib_mat_mul.exe 128

# 2 个进程，128×128×128 矩阵，使用 2D 块划分（对比实验）
mpiexec.exe -n 2 bin\mpi_2d_block_mat_mul.exe 128

# 16 个进程，2048×2048×2048 矩阵
mpiexec.exe -n 16 bin\mpi_collective_mat_mul.exe 2048
```

### 自动性能测试

运行 `benchmark.ps1` 会自动测试所有进程数（2,4,8,16）和矩阵规模（128,256,512,1024,2048）的组合：

```powershell
& .\benchmark.ps1
```

## 架构设计

### C++ 类封装的 MPI 自定义数据类型

所有 MPI 程序都使用 C++ 类封装了 MPI 自定义数据类型：

```cpp
class MatrixParams
{
public:
    int m, n, k;
    int rows;      // 该进程需要处理的行数
    int cols;      // 该进程需要处理的列数
    double compute_time;  // 计算时间

    static MPI_Datatype MPI_type;

    static void buildMPIType();  // 用正确的方式创建 MPI 数据类型
};
```

**简化后的 MPI 类型定义（2 个块）：**

```cpp
void MatrixParams::buildMPIType()
{
    MatrixParams temp;

    // 使用两个块：5 个连续的 int (m,n,k,rows,cols) + 1 个 double (compute_time)
    MPI_Datatype types[2] = {MPI_INT, MPI_DOUBLE};
    int block_lengths[2] = {5, 1};
    MPI_Aint displacements[2];

    MPI_Aint base_address;
    MPI_Get_address(&temp, &base_address);
    MPI_Get_address(&temp.m, &displacements[0]);            // 第一个 int 的位置
    MPI_Get_address(&temp.compute_time, &displacements[1]); // double 的位置

    displacements[0] -= base_address;
    displacements[1] -= base_address;

    MPI_Type_create_struct(2, block_lengths, displacements, types, &MPI_type);
    MPI_Type_commit(&MPI_type);
}
```

### 集合通信模型

使用 MPI 集合通信函数实现高效的并行计算：

- **Master (Rank 0)**: 生成随机矩阵 A 和 B，使用 `MPI_Bcast` 广播给所有进程，使用 `MPI_Scatterv` 分发矩阵块，收集各进程的结果并拼接为完整结果
- **Workers (Rank 1~P-1)**: 接收矩阵块，计算部分结果，将结果发送回 Master

### 不同数据划分方式

1. **行划分 (Row-wise Distribution)** - mpi_collective_mat_mul.exe:
   - 将矩阵 A 按行划分给不同进程
   - 每个进程获得完整的 B 矩阵
   - 计算 A_sub × B 得到 C 的对应行

2. **列划分 (Column-wise Distribution)** - mpi_col_distrib_mat_mul.exe:
   - 将矩阵 B 按列划分给不同进程
   - 每个进程获得完整的 A 矩阵
   - 计算 A × B_sub 得到 C 的对应列
   - **优化**: 使用 B 转置 + MPI_Scatterv 高效分发列数据

3. **2D 块划分 (2D Block Distribution)** - mpi_2d_block_mat_mul.exe:
   - 将矩阵按二维网格方式分布在处理器网格上
   - 提供更均匀的负载均衡
   - 每个进程处理矩阵的一个块
   - **优化**: 使用 MPI_Scatterv 分发 A 的行块

### 通信流程

#### 行划分版本：
```
Master (Rank 0)                          Workers (Rank 1~P-1)
════════════════════                     ════════════════════
生成 A[m×n], B[n×k]                      同步等待
  │ Bcast(B) ───────────────────────────→ │ Bcast(B)
  │ Scatterv(A_sub) ────────────────────→ │ Scatterv(A_sub)
  │                                      │ 计算 C_sub = A_sub × B
  │ ←───────────────────────────────────── │ Send(C_sub)
  │ Recv(C_sub) 收集结果
拼接完整 C[m×k]
输出结果、验证正确性、分析性能
```

#### 列划分版本：
```
Master (Rank 0)                          Workers (Rank 1~P-1)
════════════════════                     ════════════════════
生成 A[m×n], B[n×k]                      同步等待
  │ Bcast(A) ───────────────────────────→ │ Bcast(A)
  │ B -> B_T (转置)
  │ Scatterv(B_T_sub) ──────────────────→ │ Scatterv(B_T_sub)
  │                                      │ B_T_sub -> B_local (转置回)
  │                                      │ 计算 C_sub = A × B_local
  │ ←───────────────────────────────────── │ Send(C_sub)
  │ Recv(C_sub) 收集结果
拼接完整 C[m×k]
输出结果、验证正确性、分析性能
```

#### 2D 块划分版本：
```
Master (Rank 0)                          Workers (Rank 1~P-1)
════════════════════                     ════════════════════
生成 A[m×n], B[n×k]                      同步等待
  │ Bcast(B) ───────────────────────────→ │ Bcast(B)
  │ Bcast(params_arr) ──────────────────→ │ Bcast(params_arr)
  │ Bcast(grid_dims) ───────────────────→ │ Bcast(grid_dims)
  │ Scatterv(A_sub) ────────────────────→ │ Scatterv(A_sub)
  │                                      │ 计算 C_sub = A_local × B
  │ ←───────────────────────────────────── │ Send(C_sub)
  │ Recv(C_sub) 收集结果
拼接完整 C[m×k]
输出结果、验证正确性、分析性能
```

### 使用 C++ 类封装 MPI_Type_create_struct

- 使用 `MatrixParams::buildMPIType()` 方法将多个相关参数聚合为单个结构体进行通信
- **简化设计**: 将 5 个 int 字段合并为一个连续块，double 字段单独一个块
- 减少了通信次数，提高了通信效率
- 代码更简洁，易于维护

### 性能特点

| 特性 | 行划分版本 | 列划分版本 | 2D 块划分版本 |
|------|-----------|-----------|--------------|
| 通信模式 | Broadcast B + Scatterv A | Broadcast A + Scatterv B (转置) | Broadcast B + Scatterv A |
| 内存使用 | 每进程存储完整 B 矩阵 | 每进程存储完整 A 矩阵 | 每进程存储局部 A、B、C 块 |
| 扩展性 | 适合 A >> B 场景 | 适合 B >> A 场景 | 最优的负载均衡 |
| 通信效率 | 高效广播 B 矩阵 | 高效分发 B 列块 (转置后连续) | 高效分发 A 行块 |

### 关键优化

1. **列划分版本 - B 转置 + Scatterv**:
   - 将 B 矩阵转置为 B_T，使列数据变成连续行
   - 使用 MPI_Scatterv 分发连续的 B_T 行块
   - 避免了广播整个 B 矩阵的开销

2. **2D 块划分版本 - A 直接 Scatterv**:
   - 直接使用 MPI_Scatterv 分发 A 的行块
   - 每个进程只接收自己需要的数据
   - 大大提高了通信效率

3. **结果收集 - Send/Recv**:
   - 由于 C 矩阵是 row-major 存储，列数据不连续
   - 使用 MPI_Send/MPI_Recv 替代 Gatherv 来收集非连续列数据
   - 确保数据正确放置到全局 C 矩阵的对应位置

详细分析见 [report/实验报告.md](./report/实验报告.md)