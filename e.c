#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

struct data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    // Your sequencer state variables
    float sequencer_position;
    float frames_per_beat;
    // ...
};

// The core audio processing callback
static void on_process(void *userdata) {
    struct data *d = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;

    // Get the buffer to fill
    b = pw_stream_dequeue_buffer(d->stream);
    if (!b) return;

    buf = b->buffer;
    float *samples = (float *)buf->datas[0].data;
    int n_frames = buf->datas[0].maxsize / sizeof(float);

    // This is where the magic happens:
    for (int i = 0; i < n_frames; i++) {
        // Step the sequencer based on the audio frame
        d->sequencer_position++;

        if (d->sequencer_position >= d->frames_per_beat) {
            // It's time for a new beat!
            // Trigger the next step of your sequence here.
            // ...
            d->sequencer_position = 0; // Reset for the next beat
        }

        // Generate audio based on the current sequencer state
        // For a simple example, just generate a sine wave or a click
        samples[i] = 0.0f; // Placeholder
    }

    // Queue the filled buffer for playback
    pw_stream_queue_buffer(d->stream, b);
}

// Main function to set up PipeWire
int main(int argc, char *argv[]) {
    struct data d = {0};

    pw_init(&argc, &argv);

    d.loop = pw_main_loop_new(NULL);
    d.stream = pw_stream_new_simple(
        pw_main_loop_get_loop(d.loop),
        "my-sequencer",
        pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio",
            PW_KEY_MEDIA_CATEGORY, "Playback",
            PW_KEY_MEDIA_ROLE, "Music",
            NULL
        ),
        &stream_events,
        &d
    );

    // Set up the stream's format and connect
    // ...

    // Start the event loop. This is the main thread's run-loop.
    pw_main_loop_run(d.loop);

    // Cleanup
    pw_deinit();
    return 0;
}
