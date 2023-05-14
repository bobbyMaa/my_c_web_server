#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "threadpool.h"

void *task_entry(void *tpool)
{
    thread_pool *pool = (thread_pool *)tpool;
    tpool_task task;
    while(true) {
        pthread_mutex_lock(&(pool->pool_lock));
        while(!pool->shutdown && (pool->task_size == 0)) {
            printf("thread id: %u is waiting\n", (unsigned int)pthread_self());
            pthread_cond_wait(&(pool->no_task), &(pool->pool_lock));
        }
        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->pool_lock));
            printf("thread id:0x%x is exiting\n", (unsigned int)pthread_self());
            pthread_exit(NULL);
        }
        task.task_routine = pool->task_head->task_routine;
        task.args = pool->task_head->args;
        tpool_task *temp = pool->task_head;
        pool->task_head = pool->task_head->next;
        pool->task_size--;
        printf("+++++++++++\nnow task num need to be handled : %d\n+++++++++++\n", pool->task_size);
        free(temp);
        pool->busy_thread_size++;
        pthread_mutex_unlock(&(pool->pool_lock));
        (task.task_routine)(task.args);
        pthread_mutex_lock(&(pool->pool_lock));
        printf("thread id: %utask finished\n", (unsigned int)pthread_self());
        pool->busy_thread_size++;
        task.task_routine = NULL;
        task.args = NULL;
        pthread_mutex_unlock(&(pool->pool_lock));
    }
    pthread_exit(NULL);
}

thread_pool *create_threadpool(int num)
{
    thread_pool *pool = (thread_pool *)malloc(sizeof(thread_pool) * num);
    if (pool == NULL) {
        perror("create thread pool failed in mallocing");
        return NULL;
    }

    pool->shutdown = false;
    pool->pool_size = num;
    pool->busy_thread_size = 0;
    pool->task_size = 0;
    pool->task_head = (tpool_task *)malloc(sizeof(tpool_task));
    pool->task_head->task_routine = NULL;
    pool->task_head->args = NULL;
    pool->task_head->next = NULL;

    pool->thread = (pthread_t *)malloc(sizeof(pthread_t) * num);
    if (pool->thread == NULL) {
        perror("create thread pool failed in thread mallocing");
        free(pool);
        return NULL;
    }
    if(pthread_mutex_init(&(pool->pool_lock), NULL) != 0) {
        perror("init pool lock failed");
        free(pool->thread);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&(pool->pool_ready), NULL) != 0) {
        perror("init pool ready condition failed");
        free(pool->thread);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&(pool->no_task), NULL) != 0) {
        perror("init pool no task condition failed");
        free(pool->thread);
        free(pool);
        return NULL;
    }
    for (int i = 0; i < num; i++) {
        pthread_create(&(pool->thread[i]), NULL, task_entry, pool);
    }
    return pool;
}

/* 返回0表示添加成功，添加完任务后去唤醒线程 */
int add_task_in_threadpool(thread_pool *pool, tpool_task *task)
{
    if (pool == NULL) {
        return -1;
    }
    pthread_mutex_lock(&(pool->pool_lock));
    tpool_task *cur_task = pool->task_head;
    while(cur_task->next != NULL) {
        cur_task = cur_task->next;
    }
    cur_task->next = (tpool_task *)malloc(sizeof(tpool_task));
    if (cur_task->next == NULL) {
        perror("add task failed in malloc next task\n");
        return -1;
    }
    ((tpool_task *)(cur_task->next))->task_routine = NULL;
    ((tpool_task *)(cur_task->next))->args = NULL;
    ((tpool_task *)(cur_task->next))->next = NULL;
    cur_task->task_routine = task->task_routine;
    cur_task->args = task->args;
    pool->task_size++;
    printf("add task success!!!\n");
    pthread_mutex_unlock(&(pool->pool_lock));
    pthread_cond_signal(&(pool->no_task));
    return 0;
}