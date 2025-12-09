/**
 * CrossMIDI Usage Example
 * 
 * Compilation:
 * Linux:   gcc -o example example.c crossmidi.c -lasound -lpthread
 * macOS:   gcc -o example example.c crossmidi.c -framework CoreMIDI -framework CoreFoundation
 */

#include "cm2.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static volatile int running = 1;

/* Signal handler for clean shutdown */
void signal_handler(int sig) {
    running = 0;
}

/* MIDI message callback */
void midi_callback(uint64_t timestamp_ms, const uint8_t *message, 
                   size_t size, void *user_data) {
    printf("[%6llu ms] MIDI Message (%zu bytes): ", 
           (unsigned long long)timestamp_ms, size);
    
    for (size_t i = 0; i < size; i++) {
        printf("%02X ", message[i]);
    }
    
    /* Decode common MIDI messages */
    if (size >= 1) {
        uint8_t status = message[0] & 0xF0;
        uint8_t channel = message[0] & 0x0F;
        
        switch (status) {
            case 0x80:
                if (size >= 3)
                    printf("(Note Off - Ch:%d Note:%d Vel:%d)", 
                           channel + 1, message[1], message[2]);
                break;
            case 0x90:
                if (size >= 3) {
                    if (message[2] == 0)
                        printf("(Note Off - Ch:%d Note:%d)", channel + 1, message[1]);
                    else
                        printf("(Note On - Ch:%d Note:%d Vel:%d)", 
                               channel + 1, message[1], message[2]);
                }
                break;
            case 0xA0:
                if (size >= 3)
                    printf("(Poly Aftertouch - Ch:%d Note:%d Pressure:%d)", 
                           channel + 1, message[1], message[2]);
                break;
            case 0xB0:
                if (size >= 3)
                    printf("(Control Change - Ch:%d CC:%d Val:%d)", 
                           channel + 1, message[1], message[2]);
                break;
            case 0xC0:
                if (size >= 2)
                    printf("(Program Change - Ch:%d Program:%d)", 
                           channel + 1, message[1]);
                break;
            case 0xD0:
                if (size >= 2)
                    printf("(Channel Aftertouch - Ch:%d Pressure:%d)", 
                           channel + 1, message[1]);
                break;
            case 0xE0:
                if (size >= 3) {
                    int bend = (message[2] << 7) | message[1];
                    printf("(Pitch Bend - Ch:%d Value:%d)", channel + 1, bend - 8192);
                }
                break;
        }
    }
    
    printf("\n");
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    const char *port_name = "CrossMIDI Virtual Input";
    
    if (argc > 1) {
        port_name = argv[1];
    }
    
    printf("CrossMIDI Example\n");
    printf("=================\n");
    printf("Creating virtual MIDI input port: %s\n", port_name);
    printf("Press Ctrl+C to exit\n\n");
    
    /* Set up signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Initialize CrossMIDI */
    CM_Context *ctx = NULL;
    CM_Result result = CM_initialize(port_name, midi_callback, NULL, &ctx);
    
    if (result != CM_SUCCESS) {
        fprintf(stderr, "Error initializing CrossMIDI: %s\n", CM_get_error(ctx));
        return 1;
    }
    
    printf("Virtual MIDI port created successfully!\n");
    printf("Connect your MIDI controller or software to '%s'\n\n", port_name);
    
    /* Wait for MIDI messages */
    while (running) {
        sleep(1);
    }
    
    printf("\nShutting down...\n");
    CM_shutdown(ctx);
    printf("Goodbye!\n");
    
    return 0;
}
