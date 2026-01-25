#include <stdlib.h>
#include <string.h>
#include "portable_atomic.h"
#include "skqueue.h"
#include "seq.h"

// Initialize to 0 at startup
static atomic_uint64_t global_qid;
static int qid_initialized = 0;

static uint64_t get_next_qid(void) {
    if (!qid_initialized) {
        atomic_store_uint64(&global_qid, 0);
        qid_initialized = 1;
    }
    return atomic_fetch_add_uint64(&global_qid, 1);
}

static void ring_buffer_init(ring_buffer_t *rb, int capacity) {
    rb->items = (item_t *)calloc(capacity, sizeof(item_t));
    rb->capacity = capacity;
    atomic_store_int(&rb->write_idx, 0);
    atomic_store_int(&rb->read_idx, 0);
}

static void ring_buffer_free(ring_buffer_t *rb) {
    free(rb->items);
    rb->items = NULL;
}

static void pq_init(priority_queue_t *pq, int capacity) {
    pq->heap = (item_t *)malloc(sizeof(item_t) * capacity);
    pq->size = 0;
    pq->capacity = capacity;
}

static void pq_free(priority_queue_t *pq) {
    free(pq->heap);
    pq->heap = NULL;
}

void queue_init(queue_t *q, int max_size) {
    int capacity = 16;
    while (capacity < max_size && max_size > 0) {
        capacity *= 2;
    }
    if (max_size == 0) capacity = 1024;
    
    ring_buffer_init(&q->incoming, capacity);
    pq_init(&q->sorted, capacity);
    q->max_size = max_size;
}

void queue_free(queue_t *q) {
    ring_buffer_free(&q->incoming);
    pq_free(&q->sorted);
}

int queue_size(queue_t *q) {
    int write = atomic_load_int(&q->incoming.write_idx);
    int read = atomic_load_int(&q->incoming.read_idx);
    int ring_size = write - read;
    if (ring_size < 0) ring_size = 0;
    if (ring_size > q->incoming.capacity) ring_size = q->incoming.capacity;
    return ring_size + q->sorted.size;
}

// Lock-free multi-producer enqueue
bool queue_put(queue_t *q, uint64_t timestamp, int tag, void *data, int voice, char *what) {
    ring_buffer_t *rb = &q->incoming;
    int capacity = rb->capacity;
    
    while (1) {
        int current_write = atomic_load_int(&rb->write_idx);
        int current_read = atomic_load_int(&rb->read_idx);
        
        int size = current_write - current_read;
        if (size < 0) size = 0;
        
        if (size >= capacity - 1) {
            return false;
        }
        
        int next_write = current_write + 1;
        int expected = current_write;
        
        if (atomic_compare_exchange_int(&rb->write_idx, &expected, next_write)) {
            int slot = current_write % capacity;
            
            if (slot < 0 || slot >= capacity) {
                return false;
            }
            
            item_t *item = &rb->items[slot];
            
            item->timestamp = timestamp;
            item->id = get_next_qid();
            item->tag = tag;
            item->data = data;
            item->event.voice = voice;
            item->event.state = 1;
            
            if (what != NULL) {
                size_t max_len = sizeof(item->event.what) - 1;
                size_t len = 0;
                while (len < max_len && what[len] != '\0') {
                    item->event.what[len] = what[len];
                    len++;
                }
                item->event.what[len] = '\0';
            } else {
                item->event.what[0] = '\0';
            }
            
            atomic_store_int(&item->cancelled, 0);
            
            MEMORY_BARRIER();
            return true;
        }
    }
}

// Simpler approach: Always check ring buffer before popping from heap
bool queue_get_filtered(queue_t *q, uint64_t limit_ts, item_t *out) {
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    
    // Always transfer any available items from ring to heap
    int read = atomic_load_int(&rb->read_idx);
    int write = atomic_load_int(&rb->write_idx);
    int available = write - read;
    if (available < 0) available = 0;
    if (available > rb->capacity) available = rb->capacity;
    
    if (available > 0) {
        int old_size = pq->size;
        
        // Copy all non-cancelled items to heap
        for (int i = 0; i < available && pq->size < pq->capacity; i++) {
            int slot = (read + i) % rb->capacity;
            if (slot < 0 || slot >= rb->capacity) continue;
            
            item_t *item = &rb->items[slot];
            MEMORY_BARRIER();
            
            if (atomic_load_int(&item->cancelled) == 0) {
                pq->heap[pq->size++] = *item;
            }
        }
        
        // Update read pointer
        atomic_store_int(&rb->read_idx, read + available);
        
        // Re-heapify if we added new items
        if (pq->size > old_size) {
            for (int i = (pq->size / 2) - 1; i >= 0; i--) {
                int idx = i;
                item_t temp = pq->heap[idx];
                
                while (2 * idx + 1 < pq->size) {
                    int child = 2 * idx + 1;
                    
                    if (child + 1 < pq->size && pq->heap[child + 1].timestamp < pq->heap[child].timestamp) {
                        child++;
                    }
                    
                    if (temp.timestamp <= pq->heap[child].timestamp) break;
                    
                    pq->heap[idx] = pq->heap[child];
                    idx = child;
                }
                
                pq->heap[idx] = temp;
            }
        }
    }
    
    // Pop from heap
    while (pq->size > 0) {
        // Check cancelled
        if (atomic_load_int(&pq->heap[0].cancelled) != 0) {
            // Remove and re-heapify
            pq->heap[0] = pq->heap[--pq->size];
            
            // Sift down
            int idx = 0;
            item_t temp = pq->heap[0];
            
            while (2 * idx + 1 < pq->size) {
                int child = 2 * idx + 1;
                
                if (child + 1 < pq->size && pq->heap[child + 1].timestamp < pq->heap[child].timestamp) {
                    child++;
                }
                
                if (temp.timestamp <= pq->heap[child].timestamp) break;
                
                pq->heap[idx] = pq->heap[child];
                idx = child;
            }
            
            pq->heap[idx] = temp;
            continue;
        }
        
        // Check timestamp
        if (pq->heap[0].timestamp > limit_ts) {
            return false;
        }
        
        // Return this item
        *out = pq->heap[0];
        
        // Remove and sift down
        pq->heap[0] = pq->heap[--pq->size];
        
        if (pq->size > 0) {
            int idx = 0;
            item_t temp = pq->heap[0];
            
            while (2 * idx + 1 < pq->size) {
                int child = 2 * idx + 1;
                
                if (child + 1 < pq->size && pq->heap[child + 1].timestamp < pq->heap[child].timestamp) {
                    child++;
                }
                
                if (temp.timestamp <= pq->heap[child].timestamp) break;
                
                pq->heap[idx] = pq->heap[child];
                idx = child;
            }
            
            pq->heap[idx] = temp;
        }
        
        return true;
    }
    
    return false;
}

// Lock-free iteration
void queue_foreach(queue_t *q, queue_foreach_cb callback, void *userdata) {
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    
    int heap_size = pq->size;
    if (heap_size > pq->capacity) heap_size = pq->capacity;
    
    for (int i = 0; i < heap_size; i++) {
        if (atomic_load_int(&pq->heap[i].cancelled) == 0) {
            if (callback(&pq->heap[i], userdata) != 0) {
                return;
            }
        }
    }
    
    int read = atomic_load_int(&rb->read_idx);
    int write = atomic_load_int(&rb->write_idx);
    
    int count = write - read;
    if (count < 0) count = 0;
    if (count > rb->capacity) count = rb->capacity;
    
    for (int i = 0; i < count; i++) {
        int idx = (read + i) % rb->capacity;
        if (idx < 0 || idx >= rb->capacity) continue;
        if (atomic_load_int(&rb->items[idx].cancelled) == 0) {
            if (callback(&rb->items[idx], userdata) != 0) {
                return;
            }
        }
    }
}

// Lock-free cancel
int queue_cancel(queue_t *q, queue_cancel_cb should_cancel, void *userdata) {
    int cancelled = 0;
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    
    int heap_size = pq->size;
    if (heap_size > pq->capacity) heap_size = pq->capacity;
    
    for (int i = 0; i < heap_size; i++) {
        if (atomic_load_int(&pq->heap[i].cancelled) == 0) {
            if (should_cancel(&pq->heap[i], userdata)) {
                atomic_store_int(&pq->heap[i].cancelled, 1);
                cancelled++;
            }
        }
    }
    
    int read = atomic_load_int(&rb->read_idx);
    int write = atomic_load_int(&rb->write_idx);
    
    int count = write - read;
    if (count < 0) count = 0;
    if (count > rb->capacity) count = rb->capacity;
    
    for (int i = 0; i < count; i++) {
        int idx = (read + i) % rb->capacity;
        if (idx < 0 || idx >= rb->capacity) continue;
        if (atomic_load_int(&rb->items[idx].cancelled) == 0) {
            if (should_cancel(&rb->items[idx], userdata)) {
                atomic_store_int(&rb->items[idx].cancelled, 1);
                cancelled++;
            }
        }
    }
    
    return cancelled;
}

void queue_clear(queue_t *q) {
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    
    pq->size = 0;
    
    int write = atomic_load_int(&rb->write_idx);
    atomic_store_int(&rb->read_idx, write);
    
    MEMORY_BARRIER();
}
