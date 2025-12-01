#include "crossmidi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__linux__)
    #include <alsa/asoundlib.h>
    #include <poll.h>
    #include <pthread.h>
    #include <time.h>
#elif defined(__APPLE__)
    #include <CoreMIDI/CoreMIDI.h>
    #include <mach/mach_time.h>
#else
    #error "Unsupported platform: only Linux and macOS are supported"
#endif

struct CM_Context {
    CM_Callback callback;
    void *user_data;

#if defined(__linux__)
    snd_seq_t *seq;
    int port_id;
    pthread_t thread;
    volatile int running;
#elif defined(__APPLE__)
    MIDIClientRef client;
    MIDIPortRef input_port;
    MIDIEndpointRef virtual_endpoint;
#endif
};

/* ———————————————————————— Common ———————————————————————— */

uint64_t CM_get_timestamp_ms(void)
{
#if defined(__linux__)
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL;
#elif defined(__APPLE__)
    static mach_timebase_info_data_t info = {0};
    if (info.denom == 0) mach_timebase_info(&info);
    uint64_t t = mach_absolute_time();
    return t * info.numer / info.denom / 1000000ULL;
#endif
}

/* ———————————————————————— Linux / ALSA ———————————————————————— */

#if defined(__linux__)

static void* alsa_thread(void* arg)
{
    CM_Context* ctx = (CM_Context*)arg;
    struct pollfd* pfds;
    int npfds = snd_seq_poll_descriptors_count(ctx->seq, POLLIN);
    pfds = alloca(npfds * sizeof(struct pollfd));
    snd_seq_poll_descriptors(ctx->seq, pfds, npfds, POLLIN);

    while (ctx->running) {
        if (poll(pfds, npfds, 500) > 0) {
            snd_seq_event_t* ev;
            while (snd_seq_event_input(ctx->seq, &ev) > 0) {
                // Skip subscription events
                if (ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED ||
                    ev->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED) {
                    snd_seq_free_event(ev);
                    continue;
                }

                uint64_t ts = CM_get_timestamp_ms();
                const uint8_t* msg = NULL;
                size_t len = 0;

                switch (ev->type) {
                    case SND_SEQ_EVENT_NOTEON:
                    case SND_SEQ_EVENT_NOTEOFF:
                    case SND_SEQ_EVENT_KEYPRESS:
                        msg = (const uint8_t[]){
                            (uint8_t)(0x80 | ev->data.note.channel |
                                     (ev->type == SND_SEQ_EVENT_NOTEON && ev->data.note.velocity ? 0x10 : 0x00)),
                            (uint8_t)ev->data.note.note,
                            (uint8_t)ev->data.note.velocity
                        };
                        len = 3;
                        break;

                    case SND_SEQ_EVENT_CONTROLLER:
                        msg = (const uint8_t[]){ (uint8_t)(0xB0 | ev->data.control.channel),
                                                 (uint8_t)ev->data.control.param,
                                                 (uint8_t)ev->data.control.value };
                        len = 3;
                        break;

                    case SND_SEQ_EVENT_PGMCHANGE:
                        msg = (const uint8_t[]){ (uint8_t)(0xC0 | ev->data.control.channel),
                                                 (uint8_t)ev->data.control.value };
                        len = 2;
                        break;

                    case SND_SEQ_EVENT_CHANPRESS:
                        msg = (const uint8_t[]){ (uint8_t)(0xD0 | ev->data.control.channel),
                                                 (uint8_t)ev->data.control.value };
                        len = 2;
                        break;

                    case SND_SEQ_EVENT_PITCHBEND:
                        {
                            int pb = ev->data.control.value + 8192;
                            msg = (const uint8_t[]){ (uint8_t)(0xE0 | ev->data.control.channel),
                                                     (uint8_t)(pb & 0x7F),
                                                     (uint8_t)(pb >> 7) };
                            len = 3;
                        }
                        break;

                    case SND_SEQ_EVENT_SYSEX:
                        if (ev->data.ext.len > 0) {
                            msg = (const uint8_t*)ev->data.ext.ptr;
                            len = ev->data.ext.len;
                        }
                        break;

                    default:
                        snd_seq_free_event(ev);
                        continue;
                }

                if (msg && len > 0 && ctx->callback)
                    ctx->callback(ts, msg, len, ctx->user_data);

                snd_seq_free_event(ev);
            }
        }
    }
    return NULL;
}

static CM_Context* cm_init_linux(const char* name, CM_Callback cb, void* ud)
{
    CM_Context* ctx = calloc(1, sizeof(CM_Context));
    if (!ctx) return NULL;

    ctx->callback = cb;
    ctx->user_data = ud;
    ctx->running = 1;

    if (snd_seq_open(&ctx->seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "crossmidi: cannot open ALSA sequencer\n");
        free(ctx);
        return NULL;
    }

    snd_seq_set_client_name(ctx->seq, name);

    ctx->port_id = snd_seq_create_simple_port(ctx->seq, name,
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_APPLICATION | SND_SEQ_PORT_TYPE_MIDI_GENERIC);

    if (ctx->port_id < 0) {
        fprintf(stderr, "crossmidi: cannot create port\n");
        snd_seq_close(ctx->seq);
        free(ctx);
        return NULL;
    }

    if (pthread_create(&ctx->thread, NULL, alsa_thread, ctx) != 0) {
        snd_seq_delete_simple_port(ctx->seq, ctx->port_id);
        snd_seq_close(ctx->seq);
        free(ctx);
        return NULL;
    }

    return ctx;
}

static void cm_cleanup_linux(CM_Context* ctx)
{
    if (!ctx) return;
    ctx->running = 0;
    pthread_join(ctx->thread, NULL);
    if (ctx->seq) {
        snd_seq_delete_simple_port(ctx->seq, ctx->port_id);
        snd_seq_close(ctx->seq);
    }
    free(ctx);
}

#endif // __linux__

/* ———————————————————————— macOS / CoreMIDI ———————————————————————— */

#if defined(__APPLE__)

static void midi_read_proc(const MIDIPacketList* pktlist,
                           void* refCon,
                           void* connRefCon)
{
    (void)connRefCon;
    CM_Context* ctx = (CM_Context*)refCon;
    const MIDIPacket* packet = &pktlist->packet[0];

    for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
        uint64_t ts = CM_get_timestamp_ms();
        if (ctx->callback)
            ctx->callback(ts, packet->data, packet->length, ctx->user_data);
        packet = MIDIPacketNext(packet);
    }
}

static CM_Context* cm_init_macos(const char* name, CM_Callback cb, void* ud)
{
    CM_Context* ctx = calloc(1, sizeof(CM_Context));
    if (!ctx) return NULL;

    ctx->callback = cb;
    ctx->user_data = ud;

    OSStatus err;

    err = MIDIClientCreate(CFSTR(name), NULL, NULL, &ctx->client);
    if (err) { free(ctx); return NULL; }

    CFStringRef portNameCF = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);

    err = MIDIDestinationCreateWithBlock(ctx->client, portNameCF,
                                         &ctx->virtual_endpoint, NULL);
    if (err) {
        CFRelease(portNameCF);
        MIDIClientDispose(ctx->client);
        free(ctx);
        return NULL;
    }

    err = MIDIInputPortCreateWithBlock(ctx->client, CFSTR("In"),
                                       midi_read_proc, ctx, &ctx->input_port);
    if (err) {
        MIDIEndpointDispose(ctx->virtual_endpoint);
        CFRelease(portNameCF);
        MIDIClientDispose(ctx->client);
        free(ctx);
        return NULL;
    }

    // Connect all existing sources
    ItemCount n = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < n; ++i)
        MIDIPortConnectSource(ctx->input_port, MIDIGetSource(i), NULL);

    CFRelease(portNameCF);
    return ctx;
}

static void cm_cleanup_macos(CM_Context* ctx)
{
    if (!ctx) return;
    if (ctx->input_port) MIDIPortDispose(ctx->input_port);
    if (ctx->virtual_endpoint) MIDIEndpointDispose(ctx->virtual_endpoint);
    if (ctx->client) MIDIClientDispose(ctx->client);
    free(ctx);
}

#endif // __APPLE__

/* ———————————————————————— Public API ———————————————————————— */

CM_Context* CM_initialize(const char* port_name,
                          CM_Callback callback,
                          void* user_data)
{
    if (!port_name || !callback) return NULL;

#if defined(__linux__)
    return cm_init_linux(port_name, callback, user_data);
#elif defined(__APPLE__)
    return cm_init_macos(port_name, callback, user_data);
#else
    return NULL;
#endif
}

void CM_cleanup(CM_Context* ctx)
{
#if defined(__linux__)
    cm_cleanup_linux(ctx);
#elif defined(__APPLE__)
    cm_cleanup_macos(ctx);
#endif
}