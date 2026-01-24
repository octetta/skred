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
    // Capacity must be power of 2
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

static void pq_sift_up(priority_queue_t *pq, int idx) {
    if (idx >= pq->capacity || pq->heap == NULL) return;
    
    item_t temp = pq->heap[idx];
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (pq->heap[parent].timestamp <= temp.timestamp) break;
        pq->heap[idx] = pq->heap[parent];
        idx = parent;
    }
    pq->heap[idx] = temp;
}

static void pq_sift_down(priority_queue_t *pq, int idx) {
    if (idx >= pq->size || pq->heap == NULL) return;
    
    item_t temp = pq->heap[idx];
    int size = pq->size;
    
    while (1) {
        int left = 2 * idx + 1;
        int right = 2 * idx + 2;
        int smallest = idx;
        
        if (left < size && pq->heap[left].timestamp < pq->heap[smallest].timestamp) {
            smallest = left;
        }
        if (right < size && pq->heap[right].timestamp < pq->heap[smallest].timestamp) {
            smallest = right;
        }
        
        if (smallest == idx) break;
        
        pq->heap[idx] = pq->heap[smallest];
        idx = smallest;
    }
    pq->heap[idx] = temp;
}

static bool pq_push(priority_queue_t *pq, item_t *item) {
    if (pq->size >= pq->capacity) return false;
    if (pq->heap == NULL) return false;
    
    pq->heap[pq->size] = *item;
    pq_sift_up(pq, pq->size);
    pq->size++;
    return true;
}

static bool pq_peek(priority_queue_t *pq, item_t *out) {
    if (pq->size == 0) return false;
    *out = pq->heap[0];
    return true;
}

static bool pq_pop(priority_queue_t *pq, item_t *out) {
    if (pq->size == 0) return false;
    
    *out = pq->heap[0];
    pq->size--;
    
    if (pq->size > 0) {
        pq->heap[0] = pq->heap[pq->size];
        pq_sift_down(pq, 0);
    }
    return true;
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
        if (size < 0) size += capacity * 2;
        
        if (size >= capacity - 1) {
            return false; // Full
        }
        
        int next_write = current_write + 1;
        int expected = current_write;
        
        if (atomic_compare_exchange_int(&rb->write_idx, &expected, next_write)) {
            int slot = current_write % capacity;
            item_t *item = &rb->items[slot];
            
            item->timestamp = timestamp;
            item->id = get_next_qid();
            item->tag = tag;
            item->data = data;
            item->event.voice = voice;
            
            // Safe string copy with explicit bounds
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
            
            // Mark as not cancelled
            atomic_store_int(&item->cancelled, 0);
            
            MEMORY_BARRIER();
            return true;
        }
    }
}

// Lock-free consumer: transfer items from ring to heap, then pop by timestamp
bool queue_get_filtered(queue_t *q, uint64_t limit_ts, item_t *out) {
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    int capacity = rb->capacity;
    
    // Transfer items from ring buffer to priority queue
    // This is safe because only the consumer reads from the ring
    while (1) {
        int read = atomic_load_int(&rb->read_idx);
        int write = atomic_load_int(&rb->write_idx);
        
        int size = write - read;
        if (size <= 0) break; // No more items in ring
        if (size > capacity) size = capacity; // Safety cap
        if (pq->size >= pq->capacity) break; // Heap full
        
        int slot = read % capacity;
        if (slot < 0 || slot >= capacity) break; // Safety
        
        item_t *item = &rb->items[slot];
        
        MEMORY_BARRIER();
        
        // Skip cancelled items
        if (atomic_load_int(&item->cancelled) == 0) {
            // Push to heap
            if (!pq_push(pq, item)) break;
        }
        
        // Advance read pointer
        atomic_store_int(&rb->read_idx, read + 1);
    }
    
    // Now check the heap for the earliest timestamp (skipping cancelled)
    while (pq->size > 0) {
        item_t temp;
        if (!pq_peek(pq, &temp)) {
            return false; // Nothing available
        }
        
        // Check if cancelled
        if (atomic_load_int(&temp.cancelled) != 0) {
            // Skip this cancelled item
            pq_pop(pq, &temp);
            continue;
        }
        
        if (temp.timestamp > limit_ts) {
            return false; // Earliest item is too new
        }
        
        // Pop from heap
        return pq_pop(pq, out);
    }
    
    return false;
}

// Lock-free iteration (may see partial state, items being added/removed)
void queue_foreach(queue_t *q, queue_foreach_cb callback, void *userdata) {
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    
    // Iterate heap (may see inconsistent state, that's ok for debugging)
    int heap_size = pq->size;
    if (heap_size > pq->capacity) heap_size = pq->capacity; // Safety
    
    for (int i = 0; i < heap_size; i++) {
        if (atomic_load_int(&pq->heap[i].cancelled) == 0) {
            if (callback(&pq->heap[i], userdata) != 0) {
                return;
            }
        }
    }
    
    // Iterate ring buffer
    int read = atomic_load_int(&rb->read_idx);
    int write = atomic_load_int(&rb->write_idx);
    
    int count = write - read;
    if (count < 0) count = 0;
    if (count > rb->capacity) count = rb->capacity; // Safety cap
    
    for (int i = 0; i < count; i++) {
        int idx = (read + i) % rb->capacity;
        if (idx < 0 || idx >= rb->capacity) continue; // Safety
        if (atomic_load_int(&rb->items[idx].cancelled) == 0) {
            if (callback(&rb->items[idx], userdata) != 0) {
                return;
            }
        }
    }
}

// Lock-free cancel using tombstones
int queue_cancel(queue_t *q, queue_cancel_cb should_cancel, void *userdata) {
    int cancelled = 0;
    ring_buffer_t *rb = &q->incoming;
    priority_queue_t *pq = &q->sorted;
    
    // Cancel in heap
    int heap_size = pq->size;
    if (heap_size > pq->capacity) heap_size = pq->capacity; // Safety
    
    for (int i = 0; i < heap_size; i++) {
        if (atomic_load_int(&pq->heap[i].cancelled) == 0) {
            if (should_cancel(&pq->heap[i], userdata)) {
                atomic_store_int(&pq->heap[i].cancelled, 1);
                cancelled++;
            }
        }
    }
    
    // Cancel in ring buffer
    int read = atomic_load_int(&rb->read_idx);
    int write = atomic_load_int(&rb->write_idx);
    
    int count = write - read;
    if (count < 0) count = 0;
    if (count > rb->capacity) count = rb->capacity; // Safety cap
    
    for (int i = 0; i < count; i++) {
        int idx = (read + i) % rb->capacity;
        if (idx < 0 || idx >= rb->capacity) continue; // Safety
        if (atomic_load_int(&rb->items[idx].cancelled) == 0) {
            if (should_cancel(&rb->items[idx], userdata)) {
                atomic_store_int(&rb->items[idx].cancelled, 1);
                cancelled++;
            }
        }
    }
    
    return cancelled;
}
