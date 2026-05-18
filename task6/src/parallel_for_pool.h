/**
 * @file parallel_for_pool.h
 * @brief 基于 Pthreads 线程池的并行 for 循环动态链接库
 *
 * 与 task5 的 parallel_for 不同，此版本使用线程池模式：
 * - 首次调用时创建线程池，后续调用复用线程
 * - 使用条件变量通知工作，避免反复 pthread_create/destroy
 * - API 与 task5 parallel_for.h 完全兼容
 *
 * 编译为动态链接库：
 *   gcc -shared -fPIC -O2 -o libparallel_for_pool.so src/parallel_for_pool.c -lpthread
 *
 * 使用示例：
 *   gcc -o main main.c -Llib -lparallel_for_pool -lpthread
 */

#ifndef PARALLEL_FOR_POOL_H
#define PARALLEL_FOR_POOL_H

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
    void *(*functor)(int, void *); ///< 函数指针
    void *arg;                     ///< functor 的参数
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
 * @brief 基础并行 for 循环函数（静态调度）
 */
int parallel_for(int start, int end, int increment,
                 void *(*functor)(int, void *), void *arg, int num_threads);

/**
 * @brief 高级并行 for 循环函数（支持调度策略）
 */
int parallel_for_advanced(int start, int end, int increment,
                          void *(*functor)(int, void *), void *arg,
                          parallel_config_t *config);

/**
 * @brief 销毁线程池，释放所有资源
 *
 * 程序结束前应调用此函数，否则线程池会驻留到进程退出
 */
void parallel_for_pool_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* PARALLEL_FOR_POOL_H */
