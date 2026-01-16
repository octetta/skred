#ifndef _KORG_H_
#define _KORG_H_

#define KWAVEMAX (33)

extern int16_t *kwave[KWAVEMAX];
extern int kwave_size[KWAVEMAX];
extern double kwave_freq[KWAVEMAX];
extern char *kwave_name[KWAVEMAX];

void korg_init(void);
void resize(void);
int reconstruct_high_res_table(int16_t *source, int16_t *target, int size);

#endif
