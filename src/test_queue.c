#include <stdio.h>
#include <stdlib.h>
#include "skqueue.h"

int main() {
    queue_t q;
    queue_init(&q, 1024);
    
    printf("Test 1: Adding items in reverse order (100 down to 1)\n");
    printf("================================================\n");
    
    // Add items with timestamps 100, 99, 98, ..., 1
    for (int i = 100; i > 0; i--) {
        bool success = queue_put(&q, i, 0, NULL, 0, "test");
        if (!success) {
            printf("ERROR: Failed to add item with timestamp %d\n", i);
        }
    }
    
    printf("Added 100 items\n");
    printf("Queue size: %d\n", queue_size(&q));
    
    // Check what's in the ring buffer
    printf("\nRing buffer state:\n");
    int read = atomic_load_int(&q.incoming.read_idx);
    int write = atomic_load_int(&q.incoming.write_idx);
    printf("read_idx=%d, write_idx=%d, items in ring=%d\n", read, write, write - read);
    printf("First 10 items in ring:\n");
    for (int i = 0; i < 10 && i < (write - read); i++) {
        int slot = (read + i) % q.incoming.capacity;
        printf("  [%d] slot=%d ts=%llu\n", i, slot, q.incoming.items[slot].timestamp);
    }
    
    printf("\nHeap state before any gets:\n");
    printf("heap size=%d\n", q.sorted.size);
    
    printf("\n");
    printf("Test 2: First call to queue_get_filtered\n");
    printf("==========================================\n");
    
    item_t item;
    if (queue_get_filtered(&q, 1000, &item)) {
        printf("Got item: timestamp=%llu\n", item.timestamp);
    }
    
    printf("\nAfter first get:\n");
    read = atomic_load_int(&q.incoming.read_idx);
    write = atomic_load_int(&q.incoming.write_idx);
    printf("Ring: read_idx=%d, write_idx=%d, remaining=%d\n", read, write, write - read);
    printf("Heap: size=%d\n", q.sorted.size);
    
    if (q.sorted.size > 0) {
        printf("First 10 items in heap:\n");
        for (int i = 0; i < 10 && i < q.sorted.size; i++) {
            printf("  [%d] ts=%llu\n", i, q.sorted.heap[i].timestamp);
        }
        
        // Verify heap property
        printf("\nVerifying heap property:\n");
        int violations = 0;
        for (int i = 0; i < q.sorted.size; i++) {
            int left = 2 * i + 1;
            int right = 2 * i + 2;
            
            if (left < q.sorted.size && q.sorted.heap[i].timestamp > q.sorted.heap[left].timestamp) {
                printf("VIOLATION at %d: parent=%llu > left[%d]=%llu\n", 
                       i, q.sorted.heap[i].timestamp, left, q.sorted.heap[left].timestamp);
                violations++;
                if (violations >= 5) break;
            }
            if (right < q.sorted.size && q.sorted.heap[i].timestamp > q.sorted.heap[right].timestamp) {
                printf("VIOLATION at %d: parent=%llu > right[%d]=%llu\n",
                       i, q.sorted.heap[i].timestamp, right, q.sorted.heap[right].timestamp);
                violations++;
                if (violations >= 5) break;
            }
        }
        if (violations == 0) {
            printf("✓ Heap property is valid\n");
        } else {
            printf("✗ Found %d violations\n", violations);
        }
        
        // Check what timestamps are actually in the heap
        printf("\nAll timestamps in heap (sorted for verification):\n");
        uint64_t timestamps[100];
        int count = 0;
        for (int i = 0; i < q.sorted.size && i < 100; i++) {
            timestamps[count++] = q.sorted.heap[i].timestamp;
        }
        // Simple bubble sort
        for (int i = 0; i < count - 1; i++) {
            for (int j = 0; j < count - i - 1; j++) {
                if (timestamps[j] > timestamps[j + 1]) {
                    uint64_t tmp = timestamps[j];
                    timestamps[j] = timestamps[j + 1];
                    timestamps[j + 1] = tmp;
                }
            }
        }
        for (int i = 0; i < 20 && i < count; i++) {
            printf("%llu ", timestamps[i]);
        }
        printf("\n");
    }
    
    printf("\n");
    printf("Test 3: Retrieving remaining items\n");
    printf("====================================\n");
    
    uint64_t last_ts = item.timestamp;
    int count = 1;
    int violations = 0;
    
    while (queue_get_filtered(&q, 1000, &item)) {
        count++;
        
        if (count <= 20) {  // Print first 20
            printf("Item %3d: timestamp=%3llu", count, item.timestamp);
            
            if (item.timestamp < last_ts) {
                printf(" *** OUT OF ORDER! (last was %llu) ***", last_ts);
                violations++;
            }
            printf("\n");
        } else if (item.timestamp < last_ts) {
            violations++;
        }
        
        last_ts = item.timestamp;
    }
    
    printf("\n");
    printf("Results:\n");
    printf("========\n");
    printf("Total items retrieved: %d\n", count);
    printf("Order violations: %d\n", violations);
    
    if (violations == 0 && count == 100) {
        printf("✓ TEST PASSED\n");
    } else {
        printf("✗ TEST FAILED\n");
    }
    
    queue_free(&q);
    return violations > 0 ? 1 : 0;
}
