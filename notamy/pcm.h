#ifndef __PCM_USER_H
#define __PCM_USER_H

typedef struct {
    int32_t offset;
    uint32_t length;
    uint32_t loopstart;
    uint32_t loopend;
    uint8_t midinote;
    int32_t rate;
    int16_t *external;
} pcm_map_t;

extern pcm_map_t pcm_map[];
extern int16_t pcm[];

pcm_map_t *pcm_patch_to_map(int patch);

int pcm_rom_samples(void);
int pcm_rom_patches(void);
int pcm_usr_patches(void);

#endif

