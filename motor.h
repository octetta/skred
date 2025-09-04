#ifndef _MOTOR_H_
#define _MOTOR_H_
//
void motor_opts(int method, int priority);
void motor_init(int ms, void (*work)(void *));
void motor_update(int ms);
void motor_fini(void);
void motor_trigger(void);

#include <linux/futex.h>
#include <sys/syscall.h>
#include <stdatomic.h>

void futex_wait(int *addr, int val);
void futex_wake(int *addr);
#endif
