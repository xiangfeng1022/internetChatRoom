#ifndef __THREAD_POOL_H_
#define __THREAD_POOL_H_

#include <pthread.h>

typedef struct task_queue
{
    void *(*task_hander)(void *arg);
    void * arg;
} task_queue;

typedef struct taskQueue
{
    task_queue * taskQueue;
    int rear;
    int front;
    int queueCapacity;
    int queueSize;
} taskQueue;


typedef struct threadPool
{
    pthread_t * threadIds;
    int maxThreads;
    int minThreads;

    /* 管理者线程 */
    pthread_t manage_thread;
    /* 忙碌和存活的线程数 */
    int busyThreadNums;
    int liveThreadNums;

    /* 任务队列 */
    taskQueue * queue;

    /* 锁 */
    pthread_mutex_t mutex_pool;
    pthread_mutex_t mutex_busy;

    /* 每次变化的线程数 */
    int exitNums;

    /* 销毁线程池标记 */
    int shoutdown;

    /* 条件变量 */
    pthread_cond_t not_full;
    pthread_cond_t not_empty;

} threadPool;

/* 线程池的初始化 */
int threadPoolInit(threadPool * pool, int minThreads, int maxThreads, int queueCapacity);

/* 任务队列的插入 */
int taskQueueInsert(threadPool * pool, void *(*task_hander)(void *arg), void * arg);

/* 线程池的销毁 */
int threadPoolDestroy(threadPool * pool);


#endif //__THREAD_POOL_H_