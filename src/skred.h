#ifndef _SKRED_H_
#define _SKRED_H_

#include <stdint.h>

#define HISTORY_FILE ".skred_history"

extern int scope_enable;
extern int scope_pattern_pointer;

#define REC_IN_SEC (5 * 60)
#define ONE_FRAME_MAX (256 * 1024)

extern int rec_state;
extern long rec_max;
extern float rec_sec;
extern long rec_ptr;
extern float *recording;

#endif
