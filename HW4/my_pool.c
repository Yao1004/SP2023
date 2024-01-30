#include "my_pool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void* work(void *arg)
{
    tpool* pool = (tpool*)arg;
    while(1)
    {
        pthread_mutex_lock(&pool->mlock);
        while(pool->queue_head == NULL && !pool->finish)
            pthread_cond_wait(&pool->available, &pool->mlock);
        //printf("this is %ld\n", pthread_self());
        if(pool->queue_head != NULL)
        {
            task* job = pool->queue_head;
            pool->queue_head = pool->queue_head->next;
            pthread_mutex_unlock(&pool->mlock);
            job->func(job->arg);
            free(job);
            pthread_cond_broadcast(&pool->available);
        }
        else if (pool->finish)
        {
            pthread_mutex_unlock(&pool->mlock);
            pthread_cond_broadcast(&pool->available);
            pthread_exit(NULL);
        }
    } 
}

void tpool_add(tpool *pool, void *(*func)(void *), void *arg) 
{   
    pthread_mutex_lock(&pool->mlock);
    task* job = (task*)malloc(sizeof(task));
    job->next = NULL;
    job->arg = arg;
    job->func = func;
    if(pool->queue_head != NULL && pool->queue_head->next != NULL)
    {
        pool->queue_tail->next = job;
        pool->queue_tail = job;
    }
    else if(pool->queue_head != NULL && pool->queue_head->next == NULL)
    {
        pool->queue_head->next = job;
        pool->queue_tail = job;
    }
    else
        pool->queue_head = pool->queue_tail = job;
    pthread_cond_broadcast(&pool->available);
    pthread_mutex_unlock(&pool->mlock);
}

void tpool_wait(tpool *pool)
{
    pool->finish = true;
    pthread_cond_broadcast(&pool->available);
    for(int i=0; i<pool->n_threads; i++)
        pthread_join(pool->thread_id[i], NULL);
}

void tpool_destroy(tpool *pool)
{
    pthread_cond_destroy(&pool->available);
    pthread_mutex_destroy(&pool->mlock);
    free(pool);
}

tpool *tpool_init(int n_threads)
{
    tpool *pool = (tpool*)malloc(sizeof(tpool));
    pthread_cond_init(&pool->available, NULL);
    pthread_mutex_init(&pool->mlock, NULL);
    pool->n_threads = n_threads;
    pool->finish = false;
    pool->queue_head = pool->queue_tail = NULL;
    pool->thread_id = (pthread_t*)malloc(sizeof(pthread_t)*n_threads);
    for(int i=0; i<n_threads; i++)
        pthread_create(pool->thread_id + i, NULL, work, pool);
    return pool;
}