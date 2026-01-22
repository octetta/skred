#include <stdlib.h>
#include <string.h>
#include "skqueue.h"

void queue_init(queue_t *q, int max_size) {
    nsync_mu_init(&q->mu);
    q->items = NULL;
    q->size = 0;
    q->capacity = 0;
    q->max_size = max_size;
}

void queue_reserve(queue_t *q, int count) {
    nsync_mu_lock(&q->mu);
    if (count > q->capacity) {
        q->items = (item_t *)realloc(q->items, sizeof(item_t) * count);
        q->capacity = count;
    }
    nsync_mu_unlock(&q->mu);
}

void queue_free(queue_t *q) {
    nsync_mu_lock(&q->mu);
    free(q->items);
    q->items = NULL;
    q->size = 0;
    q->capacity = 0;
    nsync_mu_unlock(&q->mu);
}

int queue_size(queue_t *q) {
    int s;
    nsync_mu_lock(&q->mu);
    s = q->size;
    nsync_mu_unlock(&q->mu);
    return s;
}

void queue_put(queue_t *q, uint64_t timestamp, int tag, void *data) {
    nsync_mu_lock(&q->mu);

    // Check Hard Limit
    if (q->max_size > 0 && q->size >= q->max_size) {
        // Queue is full. We unlock and return.
        // In audio, we usually "drop" the newest event if the queue is at capacity.
        nsync_mu_unlock(&q->mu);
        return; 
    }

    // Resize logic (Dynamic growth up to max_size)
    if (q->size == q->capacity) {
        int new_cap = (q->capacity == 0) ? 16 : q->capacity * 2;
        
        // Ensure we don't realloc more than the hard limit
        if (q->max_size > 0 && new_cap > q->max_size) {
            new_cap = q->max_size;
        }

        q->items = (item_t *)realloc(q->items, sizeof(item_t) * new_cap);
        q->capacity = new_cap;
    }

    // Standard Sift-Up logic...
    int i = q->size++;
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (q->items[parent].timestamp <= timestamp) break;
        q->items[i] = q->items[parent];
        i = parent;
    }
    q->items[i].timestamp = timestamp;
    q->items[i].tag = tag;
    q->items[i].data = data;

    nsync_mu_unlock(&q->mu);
}

bool queue_get_filtered(queue_t *q, uint64_t limit_ts, item_t *out) {
    bool found = false;

    // Use trylock for the audio thread to avoid blocking
    if (nsync_mu_trylock(&q->mu)) {
        if (q->size > 0 && q->items[0].timestamp <= limit_ts) {
            // Success: extract the root
            *out = q->items[0];
            found = true;

            q->size--;
            if (q->size > 0) {
                // Move the last item in the array to a temporary 'moving' variable
                item_t moving = q->items[q->size];
                int i = 0;

                // Sift Down
                while (1) {
                    int left = 2 * i + 1;
                    int right = 2 * i + 2;
                    int smallest = -1; 

                    // Check if left child exists and is smaller than our moving item
                    if (left < q->size) {
                        if (q->items[left].timestamp < moving.timestamp) {
                            smallest = left;
                        }
                    }

                    // Check if right child exists and is smaller than current best candidate
                    if (right < q->size) {
                        uint64_t current_best_ts = (smallest == -1) ? moving.timestamp : q->items[left].timestamp;
                        if (q->items[right].timestamp < current_best_ts) {
                            smallest = right;
                        }
                    }

                    // If no child is smaller than 'moving', i is the correct spot
                    if (smallest == -1) {
                        break;
                    }

                    // Move the smallest child up into the hole
                    q->items[i] = q->items[smallest];
                    i = smallest;
                }
                
                // Final placement of the item that was at the end
                q->items[i] = moving;
            }
        }
        nsync_mu_unlock(&q->mu);
    }

    return found;
}
