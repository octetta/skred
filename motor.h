#ifndef _MOTOR_H_
#define _MOTOR_H_

void motor_opts(int method, int priority);
int motor_init(void (*work)(void));
int motor_go(void);
void motor_update(double ms);
void motor_fini(void);
void motor_wake_tick();
int64_t nsec_now(void);

int futex_wake(_Atomic int *addr, int n);
int futex_wait(_Atomic int *addr, int val);

extern _Atomic int futex_var;
#endif
