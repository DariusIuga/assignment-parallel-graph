// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "log/log.h"
#include "os_graph.h"
#include "os_threadpool.h"
#include "utils.h"

#define NUM_THREADS 4

static int sum;
static os_graph_t *graph;
static os_threadpool_t *tp;
static pthread_mutex_t graph_mutex = PTHREAD_MUTEX_INITIALIZER;

/* TODO: Define graph synchronization mechanisms. */

/* TODO: Define graph task argument. */

static void process_node(void *arg)
{
    unsigned int idx = (unsigned int)arg;
    if (idx >= graph->num_nodes) {
        return;
    }
    // printf("%u\n", idx);

    pthread_mutex_lock(&graph_mutex);
    os_node_t *node = graph->nodes[idx];
    if (graph->visited[idx] == DONE) {
        pthread_mutex_unlock(&graph_mutex);
        return;
    }
    graph->visited[idx] = DONE;
    sum += node->info;
    pthread_mutex_unlock(&graph_mutex);

    for (unsigned int i = 0; i < node->num_neighbours; i++) {
        if (graph->visited[node->neighbours[i]] == NOT_VISITED) {
            graph->visited[node->neighbours[i]] = PROCESSING;
            os_task_t *task = create_task(
                process_node, (void *)(node->neighbours[i]), destroy_task);
            enqueue_task(tp, task);
        }
    }

    graph->visited[idx] = DONE;
}

int main(int argc, char *argv[])
{
    FILE *input_file;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s input_file\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    input_file = fopen(argv[1], "r");
    DIE(input_file == NULL, "fopen");

    graph = create_graph_from_file(input_file);

    /* TODO: Initialize graph synchronization mechanisms. */
    tp = create_threadpool(NUM_THREADS);
    process_node(0);
    wait_for_completion(tp);
    destroy_threadpool(tp);

    printf("%d", sum);

    return 0;
}
