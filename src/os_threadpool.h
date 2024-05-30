/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef __OS_THREADPOOL_H__
#define __OS_THREADPOOL_H__ 1

#include <pthread.h>
#include <stdbool.h>

#include "os_list.h"

typedef struct {
    void *argument;
    void (*action)(void *arg);
    void (*destroy_arg)(void *arg);
    os_list_node_t list;
} os_task_t;

typedef struct os_threadpool {
    unsigned int num_threads;
    pthread_t *threads;

    /*
     * Head of queue used to store tasks.
     * First item is head.next, if head.next != head (i.e. if queue
     * is not empty).
     * Last item is head.prev, if head.prev != head (i.e. if queue
     * is not empty).
     */
    os_list_node_t head;

    /* TODO: Define threapool / queue synchronization data. */

    // The main mutex, used for locking
    pthread_mutex_t mutex;

    // Signals threads that there is work that needs to be processed
    pthread_cond_t work_cond;

    // Signals threads that there is work that there are no active threads
    pthread_cond_t working_cond;

    // Tracks the number of living threads
    size_t thread_counter;

    // Thracks the number of working threads
    size_t working_counter;

    // Used for stopping the threads
    bool stop;

    bool work_started;

} os_threadpool_t;

os_task_t *create_task(void (*f)(void *), void *arg,
                       void (*destroy_arg)(void *));
void destroy_task(void *arg);

os_threadpool_t *create_threadpool(unsigned int num_threads);
void destroy_threadpool(os_threadpool_t *tp);

void enqueue_task(os_threadpool_t *q, os_task_t *t);
os_task_t *dequeue_task(os_threadpool_t *tp);
void wait_for_completion(os_threadpool_t *tp);

#endif
