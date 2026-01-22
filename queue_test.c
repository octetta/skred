#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "skqueue.h"

int main() {
    queue_t q;
    queue_init(&q, 0);

    printf("Starting Single-Threaded Heap Integrity Test...\n");

    // 1. PUSH 1000 items with random timestamps
    for (int i = 0; i < 1000; i++) {
        uint64_t ts = rand() % 10000;
        queue_put(&q, ts, 0, NULL);
    }

    // 2. PULL and verify order
    item_t item;
    uint64_t last_ts = 0;
    int violations = 0;
    int collected = 0;

    // We use a high limit_ts to drain everything
    while (queue_get_filtered(&q, 20000, &item)) {
        if (collected > 0 && item.timestamp < last_ts) {
            printf("HEAP ERROR: Got %" PRIu64 " after %" PRIu64 "\n", item.timestamp, last_ts);
            violations++;
        }
        last_ts = item.timestamp;
        collected++;
    }

    printf("Collected: %d items. Violations: %d\n", collected, violations);

    if (violations == 0 && collected == 1000) {
        printf("RESULT: SUCCESS! The Binary Heap logic is 100%% correct.\n");
    } else {
        printf("RESULT: FAILURE. Logic needs adjustment.\n");
    }

    queue_free(&q);
    return 0;
}
