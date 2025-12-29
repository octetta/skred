#include <stdio.h>
#include <windows.h>
#include <mmsystem.h>

// This function is called every time loopMIDI receives a message
void CALLBACK MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance,
             DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
  if (wMsg == MIM_DATA) {
    // Extract MIDI bytes from the 32-bit dwParam1
    // Format: [00][Byte2][Byte1][Status]
    unsigned char status = (dwParam1 & 0xFF);
    unsigned char data1  = ((dwParam1 >> 8) & 0xFF);
    unsigned char data2  = ((dwParam1 >> 16) & 0xFF);

    // printf("MIDI Received: Status: 0x%02X, Data1: %d, Data2: %d\n", status, data1, data2);
    printf("%02x %02x %02x\n", status, data1, data2);
    fflush(stdout);

    // --- YOUR TCP/UDP LOGIC HERE ---
    // Example: if (status == 0x90) { send_udp_packet("NoteOn", data1); }
  }
}

int main() {
  UINT nDevices = midiInGetNumDevs();
  if (nDevices == 0) {
    // printf("FAIL\n");
    // fflush(stdout);
    return 1;
  }

  // List devices to find the ID for loopMIDI
  MIDIINCAPS caps;
  int targetDevice = -1;
  for (UINT i = 0; i < nDevices; i++) {
    midiInGetDevCaps(i, &caps, sizeof(MIDIINCAPS));
    // printf("ID %d: %s\n", i, caps.szPname);
    // printf("AVAIL %s\n", caps.szPname);
    // fflush(stdout);
    // Automatically pick the loopMIDI port
    if (strstr(caps.szPname, "loopMIDI")) targetDevice = i;
  }

  if (targetDevice == -1) {
    // printf("FAIL no-device\n");
    // fflush(stdout);
    return 1;
  }

  HMIDIIN hMidiIn;
  // Open the device with our callback
  if (midiInOpen(&hMidiIn, targetDevice, (DWORD_PTR)MidiInProc, 0, CALLBACK_FUNCTION) != MMSYSERR_NOERROR) {
    // printf("FAIL cannot-open\n");
    // fflush(stdout);
    return 1;
  }

  midiInStart(hMidiIn);
  // printf("OKAY %d\n", targetDevice);
  // fflush(stdout);

  // Keep program alive indefinitely, allowing MIDI messages to be processed by the callback
  while (1) {
    Sleep(1000); // Sleep for 100 milliseconds to reduce CPU usage
  }

  midiInStop(hMidiIn);
  midiInClose(hMidiIn);
  return 0;
}
