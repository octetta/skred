/**
 * CrossMIDI - Platform-specific implementation
 */

#include "cm2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Platform detection */
#if defined(__linux__)
    #define CM_PLATFORM_LINUX
#elif defined(__APPLE__) && defined(__MACH__)
    #define CM_PLATFORM_MACOS
#else
    #error "Unsupported platform"
#endif

/* Platform-specific includes */
#ifdef CM_PLATFORM_LINUX
    #include <alsa/asoundlib.h>
    #include <pthread.h>
#endif

#ifdef CM_PLATFORM_MACOS
    #include <CoreMIDI/CoreMIDI.h>
    #include <CoreFoundation/CoreFoundation.h>
    #include <mach/mach_time.h>
#endif

/* Context structure */
struct CM_Context {
    char port_name[256];
    CM_Callback callback;
    void *user_data;
    uint64_t start_time_ms;
    char error_msg[512];
    
#ifdef CM_PLATFORM_LINUX
    snd_seq_t *seq_handle;
    int port_id;
    pthread_t thread;
    int running;
#endif

#ifdef CM_PLATFORM_MACOS
    MIDIClientRef client;
    MIDIEndpointRef endpoint;
    uint64_t timebase_numer;
    uint64_t timebase_denom;
#endif
};

/* Helper: Get current timestamp in milliseconds */
static uint64_t CM_get_time_ms(CM_Context *ctx) {
#ifdef CM_PLATFORM_LINUX
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL) - ctx->start_time_ms;
#endif

#ifdef CM_PLATFORM_MACOS
    uint64_t abs_time = mach_absolute_time();
    uint64_t nanos = abs_time * ctx->timebase_numer / ctx->timebase_denom;
    return (nanos / 1000000ULL) - ctx->start_time_ms;
#endif
}

/* Helper: Set error message */
static void CM_set_error(CM_Context *ctx, const char *msg) {
    if (ctx) {
        strncpy(ctx->error_msg, msg, sizeof(ctx->error_msg) - 1);
        ctx->error_msg[sizeof(ctx->error_msg) - 1] = '\0';
    }
}

/* ==================== LINUX IMPLEMENTATION (ALSA) ==================== */
#ifdef CM_PLATFORM_LINUX

static void* CM_alsa_thread(void *arg) {
    CM_Context *ctx = (CM_Context*)arg;
    snd_seq_event_t *ev = NULL;
    
    while (ctx->running) {
        if (snd_seq_event_input(ctx->seq_handle, &ev) >= 0) {
            if (ev->type == SND_SEQ_EVENT_NOTEON || 
                ev->type == SND_SEQ_EVENT_NOTEOFF ||
                ev->type == SND_SEQ_EVENT_KEYPRESS ||
                ev->type == SND_SEQ_EVENT_CONTROLLER ||
                ev->type == SND_SEQ_EVENT_PGMCHANGE ||
                ev->type == SND_SEQ_EVENT_CHANPRESS ||
                ev->type == SND_SEQ_EVENT_PITCHBEND) {
                
                uint8_t midi_msg[3];
                size_t size = 0;
                
                switch (ev->type) {
                    case SND_SEQ_EVENT_NOTEON:
                        midi_msg[0] = 0x90 | (ev->data.note.channel & 0x0F);
                        midi_msg[1] = ev->data.note.note & 0x7F;
                        midi_msg[2] = ev->data.note.velocity & 0x7F;
                        size = 3;
                        break;
                    case SND_SEQ_EVENT_NOTEOFF:
                        midi_msg[0] = 0x80 | (ev->data.note.channel & 0x0F);
                        midi_msg[1] = ev->data.note.note & 0x7F;
                        midi_msg[2] = ev->data.note.velocity & 0x7F;
                        size = 3;
                        break;
                    case SND_SEQ_EVENT_KEYPRESS:
                        midi_msg[0] = 0xA0 | (ev->data.note.channel & 0x0F);
                        midi_msg[1] = ev->data.note.note & 0x7F;
                        midi_msg[2] = ev->data.note.velocity & 0x7F;
                        size = 3;
                        break;
                    case SND_SEQ_EVENT_CONTROLLER:
                        midi_msg[0] = 0xB0 | (ev->data.control.channel & 0x0F);
                        midi_msg[1] = ev->data.control.param & 0x7F;
                        midi_msg[2] = ev->data.control.value & 0x7F;
                        size = 3;
                        break;
                    case SND_SEQ_EVENT_PGMCHANGE:
                        midi_msg[0] = 0xC0 | (ev->data.control.channel & 0x0F);
                        midi_msg[1] = ev->data.control.value & 0x7F;
                        size = 2;
                        break;
                    case SND_SEQ_EVENT_CHANPRESS:
                        midi_msg[0] = 0xD0 | (ev->data.control.channel & 0x0F);
                        midi_msg[1] = ev->data.control.value & 0x7F;
                        size = 2;
                        break;
                    case SND_SEQ_EVENT_PITCHBEND:
                        midi_msg[0] = 0xE0 | (ev->data.control.channel & 0x0F);
                        int bend = ev->data.control.value + 8192;
                        midi_msg[1] = bend & 0x7F;
                        midi_msg[2] = (bend >> 7) & 0x7F;
                        size = 3;
                        break;
                }
                
                if (size > 0 && ctx->callback) {
                    uint64_t timestamp = CM_get_time_ms(ctx);
                    ctx->callback(timestamp, midi_msg, size, ctx->user_data);
                }
            }
            snd_seq_free_event(ev);
        }
    }
    
    return NULL;
}

CM_Result CM_initialize(const char *port_name, CM_Callback callback, 
                        void *user_data, CM_Context **out_context) {
    if (!port_name || !callback || !out_context) {
        return CM_ERROR_INVALID_PARAM;
    }
    
    CM_Context *ctx = (CM_Context*)calloc(1, sizeof(CM_Context));
    if (!ctx) {
        return CM_ERROR_MEMORY;
    }
    
    strncpy(ctx->port_name, port_name, sizeof(ctx->port_name) - 1);
    ctx->callback = callback;
    ctx->user_data = user_data;
    ctx->running = 1;
    
    /* Initialize timestamp */
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ctx->start_time_ms = ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
    
    /* Open ALSA sequencer */
    int err = snd_seq_open(&ctx->seq_handle, "default", SND_SEQ_OPEN_INPUT, 0);
    if (err < 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), 
                 "Failed to open ALSA sequencer: %s", snd_strerror(err));
        free(ctx);
        return CM_ERROR_PLATFORM;
    }
    
    /* Set client name */
    snd_seq_set_client_name(ctx->seq_handle, port_name);
    
    /* Create input port */
    ctx->port_id = snd_seq_create_simple_port(ctx->seq_handle, "input",
                        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                        SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);
    
    if (ctx->port_id < 0) {
        CM_set_error(ctx, "Failed to create ALSA port");
        snd_seq_close(ctx->seq_handle);
        free(ctx);
        return CM_ERROR_PLATFORM;
    }
    
    /* Start receiving thread */
    if (pthread_create(&ctx->thread, NULL, CM_alsa_thread, ctx) != 0) {
        CM_set_error(ctx, "Failed to create receiver thread");
        snd_seq_delete_simple_port(ctx->seq_handle, ctx->port_id);
        snd_seq_close(ctx->seq_handle);
        free(ctx);
        return CM_ERROR_PLATFORM;
    }
    
    *out_context = ctx;
    return CM_SUCCESS;
}

void CM_shutdown(CM_Context *context) {
    if (!context) return;
    
    context->running = 0;
    pthread_join(context->thread, NULL);
    snd_seq_delete_simple_port(context->seq_handle, context->port_id);
    snd_seq_close(context->seq_handle);
    free(context);
}

#endif /* CM_PLATFORM_LINUX */

/* ==================== MACOS IMPLEMENTATION (CoreMIDI) ==================== */
#ifdef CM_PLATFORM_MACOS

static void CM_coremidi_callback(const MIDIPacketList *pktlist, 
                                  void *readProcRefCon, void *srcConnRefCon) {
    CM_Context *ctx = (CM_Context*)readProcRefCon;
    const MIDIPacket *packet = &pktlist->packet[0];
    
    for (UInt32 i = 0; i < pktlist->numPackets; i++) {
        if (packet->length > 0 && ctx->callback) {
            uint64_t timestamp = CM_get_time_ms(ctx);
            ctx->callback(timestamp, packet->data, packet->length, ctx->user_data);
        }
        packet = MIDIPacketNext(packet);
    }
}

CM_Result CM_initialize(const char *port_name, CM_Callback callback, 
                        void *user_data, CM_Context **out_context) {
    if (!port_name || !callback || !out_context) {
        return CM_ERROR_INVALID_PARAM;
    }
    
    CM_Context *ctx = (CM_Context*)calloc(1, sizeof(CM_Context));
    if (!ctx) {
        return CM_ERROR_MEMORY;
    }
    
    strncpy(ctx->port_name, port_name, sizeof(ctx->port_name) - 1);
    ctx->callback = callback;
    ctx->user_data = user_data;
    
    /* Initialize Mach timebase */
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    ctx->timebase_numer = timebase.numer;
    ctx->timebase_denom = timebase.denom;
    
    uint64_t abs_time = mach_absolute_time();
    ctx->start_time_ms = (abs_time * ctx->timebase_numer / ctx->timebase_denom) / 1000000ULL;
    
    /* Create MIDI client */
    CFStringRef name = CFStringCreateWithCString(NULL, port_name, kCFStringEncodingUTF8);
    OSStatus status = MIDIClientCreate(name, NULL, NULL, &ctx->client);
    CFRelease(name);
    
    if (status != noErr) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), 
                 "Failed to create MIDI client: %d", (int)status);
        free(ctx);
        return CM_ERROR_PLATFORM;
    }
    
    /* Create virtual destination (input) endpoint */
    CFStringRef endpointName = CFStringCreateWithCString(NULL, port_name, kCFStringEncodingUTF8);
    status = MIDIDestinationCreate(ctx->client, endpointName, 
                                    CM_coremidi_callback, ctx, &ctx->endpoint);
    CFRelease(endpointName);
    
    if (status != noErr) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg), 
                 "Failed to create MIDI endpoint: %d", (int)status);
        MIDIClientDispose(ctx->client);
        free(ctx);
        return CM_ERROR_PLATFORM;
    }
    
    *out_context = ctx;
    return CM_SUCCESS;
}

void CM_shutdown(CM_Context *context) {
    if (!context) return;
    
    MIDIEndpointDispose(context->endpoint);
    MIDIClientDispose(context->client);
    free(context);
}

#endif /* CM_PLATFORM_MACOS */

/* ==================== COMMON FUNCTIONS ==================== */

const char* CM_get_error(CM_Context *context) {
    if (context && context->error_msg[0]) {
        return context->error_msg;
    }
    return "Unknown error";
}
