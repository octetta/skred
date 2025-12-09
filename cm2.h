/**
 * CrossMIDI - Cross-platform Virtual MIDI Input Library
 * 
 * Supports:
 * - Linux (ALSA Sequencer API)
 * - macOS (CoreMIDI API)
 * 
 * Compilation:
 * Linux:   gcc -o example example.c crossmidi.c -lasound -lpthread
 * macOS:   gcc -o example example.c crossmidi.c -framework CoreMIDI -framework CoreFoundation
 */

#ifndef CROSSMIDI_H
#define CROSSMIDI_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Return codes */
typedef enum {
    CM_SUCCESS = 0,
    CM_ERROR_INIT = -1,
    CM_ERROR_MEMORY = -2,
    CM_ERROR_PLATFORM = -3,
    CM_ERROR_INVALID_PARAM = -4
} CM_Result;

/* Opaque handle for MIDI context */
typedef struct CM_Context CM_Context;

/**
 * MIDI message callback function
 * 
 * @param timestamp_ms Timestamp in milliseconds since initialization
 * @param message Pointer to raw MIDI message bytes
 * @param size Number of bytes in the message
 * @param user_data User-provided context pointer
 */
typedef void (*CM_Callback)(uint64_t timestamp_ms, const uint8_t *message, 
                            size_t size, void *user_data);

/**
 * Initialize the MIDI library and create a virtual input port
 * 
 * @param port_name Name for the virtual MIDI port (visible to other applications)
 * @param callback Function to call when MIDI messages are received
 * @param user_data Pointer passed to callback function (can be NULL)
 * @param out_context Pointer to receive the context handle
 * @return CM_SUCCESS on success, error code otherwise
 */
CM_Result CM_initialize(const char *port_name, CM_Callback callback, 
                        void *user_data, CM_Context **out_context);

/**
 * Shutdown the MIDI library and free resources
 * 
 * @param context Context handle from CM_initialize
 */
void CM_shutdown(CM_Context *context);

/**
 * Get a human-readable error message for the last error
 * 
 * @param context Context handle (can be NULL for initialization errors)
 * @return Error message string
 */
const char* CM_get_error(CM_Context *context);

#ifdef __cplusplus
}
#endif

#endif /* CROSSMIDI_H */
