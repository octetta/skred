#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#include <io.h>      // for _isatty()
#elif defined(__APPLE__)
#include <CoreMIDI/CoreMIDI.h>
#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>  // for isatty()
#elif defined(__linux__)
#include <alsa/asoundlib.h>
#include <poll.h>
#include <unistd.h>  // for isatty()
#else
#error "Unsupported platform"
#endif

void process_midi(unsigned char status, unsigned char data1, unsigned char data2) {
    printf("%02x %02x %02x\n", status, data1, data2);
    fflush(stdout);
}

#if defined(_WIN32)

void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    if (wMsg == MIM_DATA) {
        unsigned char status = (dwParam1 & 0xFF);
        unsigned char data1 = ((dwParam1 >> 8) & 0xFF);
        unsigned char data2 = ((dwParam1 >> 16) & 0xFF);
        process_midi(status, data1, data2);
    }
}

int main() {
    UINT nDevices = midiInGetNumDevs();
    if (nDevices == 0) {
        return 1;
    }

    MIDIINCAPS caps;
    int targetDevice = -1;
    for (UINT i = 0; i < nDevices; i++) {
        midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));
        if (strstr(caps.szPname, "loopMIDI")) {
            targetDevice = i;
            break;
        }
    }
    if (targetDevice == -1) {
        fprintf(stderr, "No loopMIDI port found. Please create one using loopMIDI.\n");
        return 1;
    }

    HMIDIIN hMidiIn;
    if (midiInOpen(&hMidiIn, targetDevice, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
        return 1;
    }
    midiInStart(hMidiIn);

    const int is_pipe = !_isatty(_fileno(stdout));

    while (1) {
        // In pipe mode, poll faster; in interactive mode, sleep longer
        Sleep(is_pipe ? 10 : 1000);

        // On Windows, closing the pipe from Tcl will eventually cause the process to terminate
        // when we try to write to stdout, but we keep the loop responsive.
    }

    midiInStop(hMidiIn);
    midiInClose(hMidiIn);
    return 0;
}

#elif defined(__APPLE__)

static void midiReadProc(const MIDIPacketList *pktlist, void *refCon, void *connRefCon) {
    MIDIPacket *packet = (MIDIPacket *)pktlist->packet;
    for (UInt32 j = 0; j < pktlist->numPackets; ++j) {
        for (int i = 0; i < packet->length; ) {
            unsigned char status = packet->data[i];
            if (status >= 0x80 && status < 0xF0) {
                int len = (status >= 0xC0 && status <= 0xDF) ? 2 : 3;
                if (i + len <= packet->length) {
                    unsigned char data1 = (len > 1) ? packet->data[i + 1] : 0;
                    unsigned char data2 = (len > 2) ? packet->data[i + 2] : 0;
                    process_midi(status, data1, data2);
                    i += len;
                } else {
                    break;
                }
            } else if (status >= 0xF8) {
                // Realtime single-byte messages
                process_midi(status, 0, 0);
                i += 1;
            } else {
                i += 1;
            }
        }
        packet = MIDIPacketNext(packet);
    }
}

static void checkPipe(CFRunLoopTimerRef timer, void *info) {
    if (!isatty(STDOUT_FILENO)) {
        *(Boolean *)info = true;
    }
}

int main() {
    OSStatus result;
    MIDIClientRef client;
    result = MIDIClientCreate(CFSTR("MIDI Bridge"), NULL, NULL, &client);
    if (result != noErr) {
        fprintf(stderr, "Error creating MIDI client\n");
        return 1;
    }

    MIDIEndpointRef endpoint;
    result = MIDIDestinationCreate(client, CFSTR("loopMIDI"), midiReadProc, NULL, &endpoint);
    if (result != noErr) {
        fprintf(stderr, "Error creating virtual MIDI destination\n");
        MIDIClientDispose(client);
        return 1;
    }

    Boolean shouldQuit = false;
    CFRunLoopTimerRef timer = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(), 0.05, 0, 0,
                                                   checkPipe, &shouldQuit);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);

    while (!shouldQuit) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, true);
    }

    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopCommonModes);
    CFRelease(timer);

    MIDIEndpointDispose(endpoint);
    MIDIClientDispose(client);
    return 0;
}

#elif defined(__linux__)

int main() {
    snd_seq_t *seq;
    int port;

    if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0) < 0) {
        fprintf(stderr, "Error opening ALSA sequencer\n");
        return 1;
    }

    snd_seq_set_client_name(seq, "MIDI Bridge");

    port = snd_seq_create_simple_port(seq, "loopMIDI",
                                      SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
                                      SND_SEQ_PORT_TYPE_MIDI_GENERIC | SND_SEQ_PORT_TYPE_APPLICATION);

    if (port < 0) {
        fprintf(stderr, "Error creating port\n");
        snd_seq_close(seq);
        return 1;
    }

    const int is_pipe = !isatty(STDOUT_FILENO);

    int npfds = snd_seq_poll_descriptors_count(seq, POLLIN);
    struct pollfd *pfds = alloca(npfds * sizeof(struct pollfd));

    while (1) {
        // Check if pipe was closed (Tcl exited)
        struct pollfd stdin_pfd = { .fd = STDIN_FILENO, .events = POLLHUP | POLLERR };
        if (poll(&stdin_pfd, 1, 0) > 0 && (stdin_pfd.revents & (POLLHUP | POLLERR))) {
            break;
        }

        snd_seq_poll_descriptors(seq, pfds, npfds, POLLIN);
        int timeout = is_pipe ? 10 : 1000;
        if (poll(pfds, npfds, timeout) > 0) {
            snd_seq_event_t *ev;
            while (snd_seq_event_input(seq, &ev) >= 0) {
                unsigned char status = 0, data1 = 0, data2 = 0;

                switch (ev->type) {
                    case SND_SEQ_EVENT_NOTEON:
                        status = 0x90 | ev->data.note.channel;
                        data1 = ev->data.note.note;
                        data2 = ev->data.note.velocity;
                        if (data2 == 0) status -= 0x10;  // velocity 0 → Note Off
                        break;
                    case SND_SEQ_EVENT_NOTEOFF:
                        status = 0x80 | ev->data.note.channel;
                        data1 = ev->data.note.note;
                        data2 = ev->data.note.velocity;
                        break;
                    case SND_SEQ_EVENT_KEYPRESS:
                        status = 0xA0 | ev->data.note.channel;
                        data1 = ev->data.note.note;
                        data2 = ev->data.note.velocity;
                        break;
                    case SND_SEQ_EVENT_CONTROLLER:
                        status = 0xB0 | ev->data.control.channel;
                        data1 = ev->data.control.param;
                        data2 = ev->data.control.value;
                        break;
                    case SND_SEQ_EVENT_PGMCHANGE:
                        status = 0xC0 | ev->data.control.channel;
                        data1 = ev->data.control.value;
                        break;
                    case SND_SEQ_EVENT_CHANPRESS:
                        status = 0xD0 | ev->data.control.channel;
                        data1 = ev->data.control.value;
                        break;
                    case SND_SEQ_EVENT_PITCHBEND:
                        status = 0xE0 | ev->data.control.channel;
                        int bend = ev->data.control.value + 8192;
                        data1 = bend & 0x7F;
                        data2 = (bend >> 7) & 0x7F;
                        break;
                    case SND_SEQ_EVENT_CLOCK:
                    case SND_SEQ_EVENT_START:
                    case SND_SEQ_EVENT_CONTINUE:
                    case SND_SEQ_EVENT_STOP:
                    case SND_SEQ_EVENT_TICK:
                    case SND_SEQ_EVENT_SENSING:
                    case SND_SEQ_EVENT_RESET:
                        status = 0xF8 + (ev->type - SND_SEQ_EVENT_CLOCK);
                        break;
                    default:
                        snd_seq_free_event(ev);
                        continue;
                }

                process_midi(status, data1, data2);
                snd_seq_free_event(ev);
            }
        }
    }

    snd_seq_close(seq);
    return 0;
}

#endif