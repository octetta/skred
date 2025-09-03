#ifndef _MOTOR_H_
#define _MOTOR_H_
//
void motor_init(int ms, void (*work)(void *));
void motor_update(int ms);
void motor_fini(void);
#endif
