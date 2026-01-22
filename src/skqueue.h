#ifndef SKQUEUE_H
#define SKQUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "nsync_mu.h"

typedef struct {
    uint64_t timestamp;
    int tag;
    void *data;
} item_t;

typedef struct {
    nsync_mu mu;
    item_t *items;
    int size;
    int capacity;
    int max_size; // 0 for unbounded
} queue_t;

void queue_init(queue_t *q, int max_size);
void queue_free(queue_t *q);
void queue_put(queue_t *q, uint64_t timestamp, int tag, void *data);
bool queue_get_filtered(queue_t *q, uint64_t limit_ts, item_t *out);

// Returns the current number of items in the queue
int queue_size(queue_t *q);

#endif
