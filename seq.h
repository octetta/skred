#ifndef _SEQ_H_
#define _SEQ_H_

#include <stdint.h>

//int timer_init(int64_t interval_ns);
int timer_init(int64_t interval_ns, void (*work)(void *));
int timer_start(void);
void timer_stop(void);
void timer_set_interval(int64_t interval_ns);
void timer_get_stats(uint64_t *tick_count, int64_t *max_drift_ns, int64_t *avg_drift_ns);
#endif
