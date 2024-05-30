// SPDX-License-Identifier: BSD-3-Clause

#include "os_threadpool.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "log/log.h"
#include "utils.h"

void (*worker)(void *arg) = 0;

/* Create a task that would be executed by a thread. */
os_task_t *create_task(void (*action)(void *), void *arg,
                       void (*destroy_arg)(void *))
{
    os_task_t *t;

    t = malloc(sizeof(*t));
    DIE(t == NULL, "malloc");

    t->action = action;
    if (worker == 0) {
        worker = action;
    }                              // the function
    t->argument = arg;             // arguments for the function
    t->destroy_arg = destroy_arg;  // destroy argument function

    return t;
}

/* Destroy task. */
void destroy_task(void *arg)
{
    os_task_t *t = (os_task_t *)arg;

    if (t == NULL) {
        return;
    }
    if (t->destroy_arg != NULL)
        t->destroy_arg(t->argument);
    free(t);
}

/* Put a new task to threadpool task queue. */
void enqueue_task(os_threadpool_t *tp, os_task_t *t)
{
    /* TODO: Enqueue task to the shared task queue. Use synchronization. */
    pthread_mutex_lock(&(tp->mutex));

    assert(tp != NULL);
    assert(t != NULL);

    list_add(&(tp->head), &(t->list));
    pthread_cond_signal(&(tp->work_cond));
    tp->work_started = true;

    pthread_mutex_unlock(&(tp->mutex));
}

/*
 * Check if queue is empty.
 * This function should be called in a synchronized manner.
 */
static int queue_is_empty(os_threadpool_t *tp) { return list_empty(&tp->head); }

/*
 * Get a task from threadpool task queue.
 * Block if no task is available.
 * Return NULL if work is complete, i.e. no task will become available,
 * i.e. all threads are going to block.
 */

os_task_t *dequeue_task(os_threadpool_t *tp)
{
    pthread_mutex_lock(&(tp->mutex));

    if (queue_is_empty(tp)) {
        pthread_mutex_unlock(&(tp->mutex));
        pthread_cond_wait(&(tp->work_cond), &(tp->mutex));
    }

    os_task_t *t;

    /* TODO: Dequeue task from the shared task queue. Use synchronization. */
    // t should store the task that was dequeued
    t = list_entry(tp->head.next, os_task_t, list);
    if (t == NULL) {
        pthread_mutex_unlock(&(tp->mutex));
        return NULL;
    }

    list_del(&tp->head);
    tp->working_counter++;

    pthread_mutex_unlock(&(tp->mutex));
    return t;
}

/* Loop function for threads */
static void *thread_loop_function(void *arg)
{
    os_threadpool_t *tp = (os_threadpool_t *)arg;

    while (true) {
        if (!tp->work_started) {
            continue;
        }
        os_task_t *t;

        t = dequeue_task(tp);

        if (tp->stop) {
            break;
        }

        if (t != NULL) {
            if (worker > 0) {
                worker(t->argument);
            }
        }

        pthread_mutex_lock(&(tp->mutex));
        tp->working_counter--;
        if (!tp->stop && tp->working_counter == 0 && queue_is_empty(tp)) {
            pthread_cond_signal(&(tp->working_cond));
        }
        pthread_mutex_unlock(&(tp->mutex));
    }

    tp->thread_counter--;
    pthread_cond_signal(&(tp->working_cond));
    pthread_mutex_unlock(&(tp->mutex));
    return NULL;
}

/* Wait completion of all threads. This is to be called by the main thread. */
void wait_for_completion(os_threadpool_t *tp)
{
    /* TODO: Wait for all worker threads. Use synchronization. */
    pthread_mutex_lock(&(tp->mutex));

    while (true) {
        if ((!tp->stop && tp->working_counter != 0) ||
            (tp->stop && tp->thread_counter != 0)) {
            pthread_cond_wait(&(tp->working_cond), &(tp->mutex));
        } else {
            for (size_t i = 0; i < tp->num_threads; i++) {
                pthread_join(tp, NULL);
            }
            break;
        }
    }

    pthread_mutex_unlock(&(tp->mutex));
}

/* Create a new threadpool. */
os_threadpool_t *create_threadpool(unsigned int num_threads)
{
    os_threadpool_t *tp = NULL;
    int rc;

    tp = malloc(sizeof(*tp));
    DIE(tp == NULL, "malloc");

    list_init(&tp->head);

    /* TODO: Initialize synchronization data. */
    pthread_mutex_init(&(tp->mutex), NULL);
    pthread_cond_init(&(tp->work_cond), NULL);
    pthread_cond_init(&(tp->working_cond), NULL);
    tp->thread_counter = num_threads;
    tp->num_threads = num_threads;

    tp->threads = malloc(num_threads * sizeof(*tp->threads));
    DIE(tp->threads == NULL, "malloc");
    for (unsigned int i = 0; i < num_threads; ++i) {
        rc = pthread_create(&tp->threads[i], NULL, &thread_loop_function,
                            (void *)tp);
        DIE(rc < 0, "pthread_create");

        /* The resources of the threads will be freed immediately on
         * termination, instead of waiting for another thread to perform
         * PTHREAD_JOIN. */
    }

    return tp;
}

/* Destroy a threadpool. Assume all threads have been joined. */
void destroy_threadpool(os_threadpool_t *tp)
{
    os_list_node_t *n, *p;

    if (tp == NULL) {
        return;
    }

    /* TODO: Cleanup synchronization data. */
    pthread_mutex_lock(&(tp->mutex));

    list_for_each_safe(n, p, &tp->head)
    {
        list_del(n);
        // destroy_task(list_entry(n, os_task_t, list));
    }
    tp->stop = true;
    pthread_cond_broadcast(&(tp->work_cond));

    pthread_mutex_unlock(&(tp->mutex));

    wait_for_completion(tp);

    pthread_mutex_destroy(&(tp->mutex));
    // pthread_cond_destroy(&(tp->work_cond));
    pthread_cond_destroy(&(tp->working_cond));

    free(tp->threads);
    free(tp);
}
