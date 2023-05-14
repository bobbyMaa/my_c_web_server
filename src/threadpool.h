#ifndef THREAD_POOL
#define THREAD_POOL

#include <pthread.h>
#include <stdbool.h>

typedef struct tpool_work{
   void *(*task_routine)(void *args);
   void *args;
   struct tool_work *next;
}tpool_task;

typedef enum {
    THREAD_IDLE,         // 当前线程是空闲的，无事可干
    THREAD_BUSY,         // 当前线程正在执行任务
} thread_state;

typedef struct pool_thread_t{
    pthread_t thread;
    thread_state state;
}pool_thread;
 
typedef struct tpool{
    bool                 shutdown;         // is tpool shutdown or not, 1 ---> yes; 0 ---> no
    size_t               pool_size;        // count of threads
    size_t               busy_thread_size; // count of busy threads
    size_t               task_size;        // 需要运行的任务
    pthread_t            *thread;          // a array of threads
    tpool_task           *task_head;       // tpool_work queue
    pthread_cond_t       pool_ready;       // 线程池操作的条件变量
    pthread_cond_t       no_task;          // 没有任务 来阻塞线程池
    pthread_mutex_t      pool_lock;        // 操作线程池的互斥量
}thread_pool;

thread_pool *create_threadpool(int num);
int add_task_in_threadpool(thread_pool *pool, tpool_task *task);
void *task_entry(void *tpool);


#endif