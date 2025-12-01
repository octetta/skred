/*
 * crossmidi example — Virtual MIDI Input Port
 * Works on Linux (ALSA) and macOS (CoreMIDI)
 *
 * Compile:
 *   Linux:   gcc example.c crossmidi.c -lasound -lpthread -o midi_monitor
 *   macOS:   clang example.c crossmidi.c -framework CoreMIDI -framework CoreFoundation -o midi_monitor
 */

#include "crossmidi.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "udpmini.h"

udp_t *skred;

// This callback runs every time a MIDI message arrives
void midi_callback(uint64_t timestamp_ms,
           const uint8_t *message,
           size_t size,
           void *user_data)
{
  (void)user_data;  // unused in this example

  // Convert timestamp to seconds with millisecond precision
  double time_sec = timestamp_ms / 1000.0;

  printf("[%.3f] ", time_sec);

  // Print raw bytes in hex
  for (size_t i = 0; i < size; ++i) {
    printf("%02X ", message[i]);
  }

  // Optional: human-readable interpretation for common messages
  if (size >= 1) {
    uint8_t status = message[0];
    uint8_t channel = status & 0x0F;
    uint8_t type = status & 0xF0;

    char s[1024];

    switch (type) {
      case 0x90:
        if (size == 3 && message[2] > 0) {
          printf("  Note On  ch:%2d note:%3d vel:%3d", channel + 1, message[1], message[2]);
          sprintf(s, "v%d n%d l1", channel, message[1]);
          printf(" %s -> skred", s);
          udp_send(skred, s, strlen(s));
        } else {
          printf("  Note Off ch:%2d note:%3d", channel + 1, message[1]);
          sprintf(s, "v%d l0", channel);
          printf(" %s -> skred", s);
          udp_send(skred, s, strlen(s));
        }
        break;
      case 0x80:
        printf("  Note Off ch:%2d note:%3d vel:%3d", channel + 1, message[1], message[2]);
        sprintf(s, "v%d l0", channel);
        printf(" %s -> skred", s);
        udp_send(skred, s, strlen(s));
        break;
      case 0xB0: printf("  CC     ch:%2d num:%3d val:%3d", channel + 1, message[1], message[2]); break;
      case 0xC0: printf("  Program  ch:%2d pgm:%3d", channel + 1, message[1]); break;
      case 0xE0: {
        int pitch = (message[2] << 7) | message[1];
        pitch -= 8192;
        float f = (float)pitch / (float)8192.0f;
        printf("  PitchBend ch:%2d val:%+d -> %g", channel + 1, pitch, f);
        break;
      }
      default:
        if (status == 0xF0 || status == 0xF7) {
          printf("  SysEx (%zu bytes)", size);
        } else {
          printf("  Other");
        }
    }
  }

  printf("\n");
  fflush(stdout);
}

int main(void) {
  skred = udp_open("127.0.0.1", 60440);
  if (skred) {
    printf("opened!\n");
  } else {
    printf("NOT opened!\n");
    return 1;
  }
  char *skred = "skred-midi-bridge";
  printf("crossmidi virtual MIDI input example\n");
  printf("Creating virtual port named '%s'...\n", skred);

  CM_Context *ctx = CM_initialize(skred, midi_callback, NULL);

  if (!ctx) {
    fprintf(stderr, "Failed to create virtual MIDI input port!\n");
    fprintf(stderr, "Make sure you are on Linux or macOS.\n");
    return 1;
  }

  printf("SUCCESS! Virtual MIDI input port is now active.\n\n");
  printf("You can now send MIDI to it from any app:\n");
#if defined(__linux__)
  printf("   • In QjackCtl / Patchage / a2jmidid → connect something to '%s'\n", skred);
  printf("   • Or use: aconnect <sender> <%s client>:0\n", skred);
#elif defined(__APPLE__)
  printf("   • Open 'Audio MIDI Setup' → MIDI Studio → double-click IAC Driver → enable '%s'\n", skred);
  printf("   • Or use any DAW (Ableton, Logic, Reaper, etc.)\n");
#endif
  printf("\nPress Enter to stop and exit...\n\n");

  getchar();  // Wait for user to press Enter

  printf("Shutting down...\n");
  CM_cleanup(ctx);
  printf("Done.\n");

  return 0;
}