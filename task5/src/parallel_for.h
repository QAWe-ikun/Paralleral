/**
 * @file parallel_for.h
 * @brief 基于 Pthreads 的并行 for 循环动态链接库
 *
 * 提供 parallel_for 函数，模仿 OpenMP 的 omp parallel for 构造
 * 基于 Pthreads 实现并行循环分解、分配及执行机制
 *
 * 编译为动态链接库：
 *   gcc -shared -fPIC -o lib/libparallel_for.so src/parallel_for.c -lpthread
 *
 * 使用示例：
 *   gcc -o main main.c -Llib -lparallel_for -lpthread
 */

#ifndef PARALLEL_FOR_H
#define PARALLEL_FOR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 循环索引信息结构体
 */
typedef struct {
  int start;     ///< 循环开始索引
  int end;       ///< 循环结束索引（不包含）
  int increment; ///< 每次循环增加索引数
} for_index_t;

/**
 * @brief 线程工作参数结构体
 */
typedef struct {
  int start;                     ///< 当前线程负责的起始索引
  int end;                       ///< 当前线程负责的结束索引
  int increment;                 ///< 索引自增量
  void *(*functor)(int, void *); ///< 函数指针，定义每次循环执行的内容
  void *arg;                     ///< functor 的参数指针
  int thread_id;                 ///< 线程 ID
} thread_work_t;

/**
 * @brief 并行 for 循环调度策略
 */
typedef enum {
  SCHEDULE_STATIC,  ///< 静态调度：预先平均分配任务
  SCHEDULE_DYNAMIC, ///< 动态调度：运行时动态分配任务块
  SCHEDULE_GUIDED   ///< 引导调度：块大小逐渐减小
} schedule_type_t;

/**
 * @brief 并行 for 循环配置
 */
typedef struct {
  int num_threads;          ///< 线程数量
  schedule_type_t schedule; ///< 调度策略
  int chunk_size;           ///< 块大小（用于动态/引导调度）
} parallel_config_t;

/**
 * @brief 基础并行 for 循环函数
 *
 * 创建多个 Pthreads 线程，并行执行指定的循环内容
 *
 * @param start 循环开始索引
 * @param end 循环结束索引（不包含）
 * @param increment 每次循环增加索引数
 * @param functor 函数指针，定义每次循环执行的内容
 *                函数签名: void *(*functor)(int idx, void *arg)
 * @param arg functor 的参数指针
 * @param num_threads 期望产生的线程数量
 * @return 0 表示成功，-1 表示失败
 *
 * 示例：
 *   struct functor_args args = {A, B, C};
 *   parallel_for(0, 10, 1, functor, (void*)&args, 2);
 */
int parallel_for(int start, int end, int increment,
                 void *(*functor)(int, void *), void *arg, int num_threads);

/**
 * @brief 高级并行 for 循环函数（支持调度策略）
 *
 * @param start 循环开始索引
 * @param end 循环结束索引（不包含）
 * @param increment 每次循环增加索引数
 * @param functor 函数指针，定义每次循环执行的内容
 * @param arg functor 的参数指针
 * @param config 并行配置（线程数、调度策略、块大小）
 * @return 0 表示成功，-1 表示失败
 */
int parallel_for_advanced(int start, int end, int increment,
                          void *(*functor)(int, void *), void *arg,
                          parallel_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* PARALLEL_FOR_H */