#include <pthread.h>
#include <stdbool.h>
#ifndef __MY_THREAD_POOL_H
#define __MY_THREAD_POOL_H

typedef struct task 
{   
    void *(*func)(void *);
    void *arg;
    struct task* next;
} task;

typedef struct tpool 
{ 
    pthread_t* thread_id;
    pthread_cond_t available;
    pthread_mutex_t mlock;
    task* queue_head;
    task* queue_tail;
    int n_threads;
    bool finish;
} tpool;

tpool *tpool_init(int n_threads);
void tpool_add(tpool *, void *(*func)(void *), void *);
void tpool_wait(tpool *);
void tpool_destroy(tpool *);
void *work(void *arg);

#endif