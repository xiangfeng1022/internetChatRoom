#include "threadPool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MIN_THREADS 5
#define DEFAULT_MAX_THREADS 15
#define DEFAULT_TASK_CAPACITY   5

#define WAIT_TIME   5
#define DEFAULT_VARY_THREADS    3

enum STATUS_CODE
{
    ON_SUCESS,
    MALLOC_ERROR,
    INVALID_DATA,
    NULL_PTR,
    UNKNOWN_ERROR,
};

#define CHECK_MALLOC(ptr)       \
do{                             \
    if (!ptr){                  \
        return MALLOC_ERROR;    \
    }                           \
}while(0)

static int cancle_thread(threadPool * pool);
static void * threadHander(void *arg);
static void * managerHander(void * arg);


static int cancle_thread(threadPool * pool)
{
    for (int idx = 0; idx < pool->maxThreads; idx++)
    {
        if (pthread_self() == pool->threadIds[idx])
        {
            pool->threadIds[idx] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}

static void * threadHander(void *arg)
{
    threadPool * pool = (threadPool *)arg;

    while (1)
    {
        pthread_mutex_lock(&(pool->mutex_pool));
        /* 任务队列为空则需要等信号 */
        while (pool->queue->queueSize == 0 && pool->shoutdown == 0)
        {
            pthread_cond_wait(&(pool->not_empty), &(pool->mutex_pool));

            if (pool->exitNums > 0)
            {
                pool->exitNums--;
                if (pool->liveThreadNums > pool->minThreads)
                {
                    pool->liveThreadNums--;
                    pthread_mutex_unlock(&(pool->mutex_pool));
                    cancle_thread(pool);
                }
            }

        }

        if (pool->shoutdown)
        {
            pthread_mutex_unlock(&(pool->mutex_pool));
            cancle_thread(pool);
        }

        /* 取任务队列数据 */
        task_queue tmpTask = pool->queue->taskQueue[pool->queue->front];
        pool->queue->queueSize--;
        pool->queue->front = (pool->queue->front + 1) % pool->queue->queueCapacity;

        pthread_mutex_unlock(&(pool->mutex_pool));
        pthread_cond_signal(&(pool->not_full));

        pthread_mutex_lock(&(pool->mutex_busy));
        pool->busyThreadNums++;
        pthread_mutex_unlock(&(pool->mutex_busy));

        tmpTask.task_hander(tmpTask.arg);

        pthread_mutex_lock(&(pool->mutex_busy));
        pool->busyThreadNums--;
        pthread_mutex_unlock(&(pool->mutex_busy));
        
    }

    pthread_exit(NULL);

}

/* 管理者线程 */
static void * managerHander(void * arg)
{
    threadPool * pool = (threadPool *)arg;
    int flag = 0;
    while (pool->shoutdown == 0)
    {
        sleep(WAIT_TIME);

        pthread_mutex_lock(&pool->mutex_pool);
        int taskNum = pool->queue->queueSize;
        int liveNum = pool->liveThreadNums;
        pthread_mutex_unlock(&pool->mutex_pool);

        pthread_mutex_lock(&pool->mutex_busy);
        int busyNum = pool->busyThreadNums;
        pthread_mutex_unlock(&pool->mutex_busy);

        int ret = 0;
        int count = 0;
        if (busyNum == liveNum)
        {
            /* 扩容 */
            for (int idx = 0; idx < pool->maxThreads && count < DEFAULT_VARY_THREADS && liveNum <= pool->maxThreads; idx++)
            {
                if (pool->threadIds[idx] == 0)
                {
                    ret = pthread_create(&pool->threadIds[idx], NULL, threadHander, pool);
                    if (ret != 0)
                    {
                        printf("pthread create error\n");
                        break;
                    }
                    count++;
                    pthread_mutex_lock(&pool->mutex_pool);
                    pool->liveThreadNums++;
                    pthread_mutex_unlock(&pool->mutex_pool);
                }
                
            }
        }

        if (ret != 0)
        {
            flag = 1;
            break;
        }

        if ((liveNum - busyNum) >> 1 > busyNum && liveNum > pool->minThreads)
        {
            /* 缩容 */
            pthread_mutex_lock(&pool->mutex_pool);

            pool->exitNums = DEFAULT_VARY_THREADS;
            for (int idx = 0; idx < liveNum; idx++)
            {
                pthread_cond_signal(&pool->not_empty);
            }

            pthread_mutex_unlock(&pool->mutex_pool);
        }

    }

    /* 出错销毁线程池 */
    if (flag)
    {
        flag = 0;
        threadPoolDestroy(pool);
    }

    /* 线程池销毁函数 */
    if (pool->shoutdown)
    {
        pthread_exit(NULL);
    }

    return ON_SUCESS;
    
}

/* 线程池的初始化 */
int threadPoolInit(threadPool * pool, int minThreads, int maxThreads, int queueCapacity)
{
    /* 判断大小是否合法 */
    if (!pool)
    {
        return NULL_PTR;
    }

    do 
    {
        if (minThreads <= 0 || maxThreads <= 0 || minThreads > maxThreads)
        {
            minThreads = DEFAULT_MIN_THREADS;
            maxThreads = DEFAULT_MAX_THREADS;
        }

        if (queueCapacity <= 0)
        {
            queueCapacity = DEFAULT_TASK_CAPACITY;
        }

    
        pool->minThreads = minThreads;
        pool->maxThreads = maxThreads;
        pool->exitNums = 0;
        pool->shoutdown = 0;

        pool->threadIds = (pthread_t *)malloc(sizeof(pthread_t) * pool->maxThreads);
        CHECK_MALLOC(pool->threadIds);

        /* 任务队列 */
        pool->queue = (taskQueue *)malloc(sizeof(taskQueue) * 1);
        CHECK_MALLOC(pool->queue);
        memset(pool->queue, 0, sizeof(taskQueue) * 1);

        pool->queue->front = 0;
        pool->queue->rear = 0;
        pool->queue->queueCapacity = queueCapacity;
        pool->queue->queueSize = 0;

        pool->queue->taskQueue = (task_queue *)malloc(sizeof(task_queue) * pool->queue->queueCapacity);
        CHECK_MALLOC(pool->queue->taskQueue);
        memset(pool->queue->taskQueue, 0, sizeof(task_queue) * pool->queue->queueCapacity);

        /* 创建线程 */
        int ret = 0;
        for (int idx = 0; idx < pool->minThreads; idx++)
        {
            ret = pthread_create(&(pool->threadIds[idx]), NULL, threadHander, pool);
            if (ret != 0)
            {
                printf("pthread create error");
                break;
            }
        }

        if (ret != 0)
        {
            break;
        }

        /* 创建管理者线程 */
        pthread_create(&(pool->manage_thread), NULL, managerHander, pool);

        pool->busyThreadNums = 0;
        pool->liveThreadNums = pool->minThreads;

        /* 初始化锁和条件变量 */
        pthread_mutex_init(&(pool->mutex_pool), NULL);

        pthread_cond_init(&(pool->not_full), NULL);
        pthread_cond_init(&(pool->not_empty),NULL);


        return ON_SUCESS;

    }while(0);

    /* 初始化中有错误，需要销毁资源 */
    for (int idx = 0; idx < minThreads; idx++)
    {
        if (pool->threadIds[idx] != 0)
        {
            pthread_join(pool->threadIds[idx], NULL);
        }
    }

    pthread_join(pool->manage_thread, NULL);

    pthread_mutex_destroy(&pool->mutex_pool);
    pthread_mutex_destroy(&pool->mutex_busy);

    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);

    if (pool->threadIds)
    {
        free(pool->threadIds);
        pool->threadIds = NULL;
    }

    if (pool->queue->taskQueue)
    {
        free(pool->queue->taskQueue);
        pool->queue->taskQueue = NULL;
    }

    if (pool->queue)
    {
        free(pool->queue);
        pool->queue = 0;
    }


    return UNKNOWN_ERROR;
}

/* 任务队列的插入 */
int taskQueueInsert(threadPool * pool, void *(*task_hander)(void *arg), void * arg)
{
    if (!pool)
    {
        return NULL_PTR;
    }

    /* 不是线程，不需要循环等待 */
    pthread_mutex_lock(&(pool->mutex_pool));
    while (pool->queue->queueSize == pool->queue->queueCapacity)
    {
        pthread_cond_wait(&(pool->not_full), &(pool->mutex_pool));
    }

    pool->queue->taskQueue[pool->queue->rear].task_hander = task_hander; 
    pool->queue->taskQueue[pool->queue->rear].arg = arg;
    pool->queue->rear = (pool->queue->rear + 1) % pool->queue->queueCapacity;
    pool->queue->queueSize++;

    pthread_mutex_unlock(&(pool->mutex_pool));
    pthread_cond_signal(&(pool->not_empty));

    return ON_SUCESS;
}

/* 线程池的销毁 */
int threadPoolDestroy(threadPool * pool)
{
    if (!pool)
    {
        return ON_SUCESS;
    }

    pool->shoutdown = 1;

    pthread_cond_broadcast(&pool->not_empty);

    /* 回收线程池资源 */
    for (int idx = 0; idx < pool->maxThreads; idx++)
    {
        if (pool->threadIds[idx] != 0)
        {
            pthread_join(pool->threadIds[idx], NULL);
        }
    }

    pthread_join(pool->manage_thread, NULL);

    pthread_mutex_destroy(&pool->mutex_pool);
    pthread_mutex_destroy(&pool->mutex_busy);

    pthread_cond_destroy(&pool->not_empty);
    pthread_cond_destroy(&pool->not_full);

    if (pool->threadIds)
    {
        free(pool->threadIds);
        pool->threadIds = NULL;
    }

    if (pool->queue->taskQueue)
    {
        free(pool->queue->taskQueue);
        pool->queue->taskQueue = NULL;
    }

    if (pool->queue)
    {
        free(pool->queue);
        pool->queue = 0;
    }

    return ON_SUCESS;
}
