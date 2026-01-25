#ifndef SKQUEUE_H
#define SKQUEUE_H

#include <stdint.h>
#include <stdbool.h>
#include "portable_atomic.h"
#include "seq.h"

typedef struct {
    uint64_t timestamp;
    uint64_t id;
    int tag;
    void *data;
    event_t event;
    atomic_int_t cancelled;  // 0 = active, 1 = cancelled
} item_t;

// Lock-free ring buffer for incoming items
typedef struct {
    item_t *items;
    atomic_int_t write_idx;
    atomic_int_t read_idx;
    int capacity;
} ring_buffer_t;

// Priority queue (min-heap) for sorted output
typedef struct {
    item_t *heap;
    int size;
    int capacity;
} priority_queue_t;

typedef struct {
    ring_buffer_t incoming;  // Lock-free MPSC queue
    priority_queue_t sorted; // Min-heap for sorted access
    int max_size;
} queue_t;

void queue_init(queue_t *q, int max_size);
void queue_free(queue_t *q);

// Lock-free: Multiple producers can call this (returns false if full)
bool queue_put(queue_t *q, uint64_t timestamp, int tag, void *data, int voice, char *what);

// Lock-free for audio callback: Gets next item by timestamp
// Internally transfers from ring buffer to heap as needed
bool queue_get_filtered(queue_t *q, uint64_t limit_ts, item_t *out);

int queue_size(queue_t *q);

// Admin operations
// Callback receives each item, return 0 to continue, non-zero to stop
// Note: may see partial/inconsistent state during iteration
typedef int (*queue_foreach_cb)(const item_t *item, void *userdata);
void queue_foreach(queue_t *q, queue_foreach_cb callback, void *userdata);

// Cancel items matching a predicate (lock-free using tombstones)
typedef bool (*queue_cancel_cb)(const item_t *item, void *userdata);
int queue_cancel(queue_t *q, queue_cancel_cb should_cancel, void *userdata);

void queue_clear(queue_t *q);

#endif
