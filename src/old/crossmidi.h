#ifndef CROSSMIDI_H
#define CROSSMIDI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*CM_Callback)(uint64_t timestamp_ms,
                            const uint8_t *message,
                            size_t size,
                            void *user_data);

typedef struct CM_Context CM_Context;

CM_Context* CM_initialize(const char *port_name,
                          CM_Callback callback,
                          void *user_data);

void CM_cleanup(CM_Context *ctx);

uint64_t CM_get_timestamp_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* CROSSMIDI_H */