#include <stdio.h>
#include <stdlib.h>
#include "skqueue.h"

// Direct heap test - bypass the ring buffer entirely
int main() {
    queue_t q;
    queue_init(&q, 1024);
    
    priority_queue_t *pq = &q.sorted;
    
    printf("Direct heap test: Adding items 100 down to 1\n");
    printf("============================================\n");
    
    // Manually add items directly to heap
    for (int i = 100; i > 0; i--) {
        if (pq->size >= pq->capacity) {
            printf("ERROR: Heap full\n");
            break;
        }
        
        // Add to end
        pq->heap[pq->size].timestamp = i;
        pq->heap[pq->size].id = i;
        atomic_store_int(&pq->heap[pq->size].cancelled, 0);
        pq->size++;
        
        // Sift up
        int idx = pq->size - 1;
        while (idx > 0) {
            int parent = (idx - 1) / 2;
            if (pq->heap[parent].timestamp <= pq->heap[idx].timestamp) break;
            
            item_t temp = pq->heap[idx];
            pq->heap[idx] = pq->heap[parent];
            pq->heap[parent] = temp;
            idx = parent;
        }
    }
    
    printf("Added 100 items to heap\n");
    printf("Heap root timestamp: %llu (should be 1)\n\n", pq->heap[0].timestamp);
    
    // Verify heap property
    printf("Verifying heap property...\n");
    int violations = 0;
    for (int i = 0; i < pq->size; i++) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        
        if (left < pq->size && pq->heap[i].timestamp > pq->heap[left].timestamp) {
            printf("VIOLATION at %d: parent=%llu > left=%llu\n", 
                   i, pq->heap[i].timestamp, pq->heap[left].timestamp);
            violations++;
        }
        if (right < pq->size && pq->heap[i].timestamp > pq->heap[right].timestamp) {
            printf("VIOLATION at %d: parent=%llu > right=%llu\n",
                   i, pq->heap[i].timestamp, pq->heap[right].timestamp);
            violations++;
        }
    }
    
    if (violations == 0) {
        printf("✓ Heap property verified\n\n");
    } else {
        printf("✗ Found %d violations\n\n", violations);
        queue_free(&q);
        return 1;
    }
    
    // Now extract all items
    printf("Extracting items (should come out 1, 2, 3, ...):\n");
    printf("=================================================\n");
    
    uint64_t last_ts = 0;
    int count = 0;
    violations = 0;
    
    while (pq->size > 0) {
        count++;
        uint64_t ts = pq->heap[0].timestamp;
        
        printf("Item %3d: timestamp=%3llu", count, ts);
        
        if (ts < last_ts) {
            printf(" *** OUT OF ORDER! (last was %llu) ***", last_ts);
            violations++;
        }
        printf("\n");
        
        last_ts = ts;
        
        // Remove root
        pq->size--;
        if (pq->size > 0) {
            pq->heap[0] = pq->heap[pq->size];
            
            // Sift down
            int idx = 0;
            item_t temp = pq->heap[0];
            while (1) {
                int left = 2 * idx + 1;
                int right = 2 * idx + 2;
                int smallest = idx;
                
                if (left < pq->size && pq->heap[left].timestamp < temp.timestamp) {
                    smallest = left;
                }
                if (right < pq->size) {
                    uint64_t smallest_ts = (smallest == idx) ? temp.timestamp : pq->heap[left].timestamp;
                    if (pq->heap[right].timestamp < smallest_ts) {
                        smallest = right;
                    }
                }
                
                if (smallest == idx) break;
                
                pq->heap[idx] = pq->heap[smallest];
                idx = smallest;
            }
            pq->heap[idx] = temp;
        }
    }
    
    printf("\nResults:\n");
    printf("========\n");
    printf("Items extracted: %d\n", count);
    printf("Order violations: %d\n", violations);
    
    if (violations == 0 && count == 100) {
        printf("✓ TEST PASSED - Heap works correctly!\n");
    } else {
        printf("✗ TEST FAILED - Heap is broken!\n");
    }
    
    queue_free(&q);
    return violations > 0 ? 1 : 0;
}
