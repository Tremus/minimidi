/* MINIMIDI
 * STB style header library.
 * Simply #define MINIMIDI_IMPL once in your project to get the OS specific implementation
 *
 * DOCS:
 * Set #define MINIMIDI_USE_GLOBAL to add a static global MiniMIDI in the implementation.
 * You can access this object by calling minimidi_get_global();
 */

#ifdef __cplusplus
extern "C" {
#endif
#ifndef MINIMIDI_H
#define MINIMIDI_H

#include <stddef.h>

typedef struct MiniMIDI MiniMIDI;

void      minimidi_init(MiniMIDI* mm);
MiniMIDI* minimidi_create();
void      minimidi_free(MiniMIDI* mm);
#ifdef MINIMIDI_USE_GLOBAL
MiniMIDI* minimidi_get_global(void);
#endif

unsigned long minimidi_get_num_ports(MiniMIDI* mm);

/* Fills 'nameBuffer' with null terminated string
   Returns 0 on success */
int minimidi_get_port_name(MiniMIDI* mm, unsigned int portNumber, char* nameBuffer, size_t bufferSize);

/* Creates a port with given name.
   Returns 0 on success */
int  minimidi_connect_port(MiniMIDI* mm, unsigned int portNumber, const char* portName);
void minimidi_disconnect_port(MiniMIDI* mm);

/*
    TODO:
    - get midi message from tueue
    - push midi message to queue
*/

#endif /* MINIMIDI_H */

#ifdef MINIMIDI_IMPL
#undef MINIMIDI_IMPL

#define ARRSIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define MINIMIDI_MIDI_BUFFER_COUNT 4
#define MINIMIDI_MIDI_BUFFER_SIZE 1024

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>

struct MiniMIDI
{
    CFStringRef   clientName;
    MIDIClientRef clientRef;

    CFStringRef connectedPortName;
    MIDIPortRef portRef;
};

int minimidi_init(MiniMIDI* mm)
{
    OSStatus error;

    memset(mm, 0, sizeof(*mm));
    /* TODO: try and create string here without allocating */
    mm->clientName = CFStringCreateWithCString(NULL, "MiniMIDI Input Client", kCFStringEncodingASCII);
    error          = MIDIClientCreate(mm->clientName, NULL, NULL, &mm->clientRef);

    return ptr;
}

MiniMIDI* minimidi_create()
{
    MiniMIDI* mm = (MiniMIDI*)calloc(1, sizeof(MiniMIDI));
    minimidi_init(mm);
    return mm;
}

void minimidi_free(MiniMIDI* mm)
{
    assert(mm != NULL);
    minimidi_disconnect_port(mm);
    if (mm->clientName != NULL)
        CFRelease(mm->clientName);
#ifndef MINIMIDI_USE_GLOBAL
    free(mm);
#endif
}

unsigned long minimidi_get_num_ports(MiniMIDI* mm)
{
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, false);
    return MIDIGetNumberOfSources();
}

/*
This is the simplist way to get a port name.
More complicated ways invlove querying for connections, iterating through them and
appending their names to a comma seperated string list. Such examples can be found here:
https://developer.apple.com/library/archive/qa/qa1374/_index.html
*/
int minimidi_get_port_name(MiniMIDI* mm, unsigned int portNum, char* nameBuf, size_t bufSize)
{
    OSStatus        err     = 1;
    MIDIEndpointRef portRef = 0;
    CFStringRef     nameRef = NULL;

    portRef = MIDIGetSource(portNum);
    err     = MIDIObjectGetStringProperty(portRef, kMIDIPropertyDisplayName, &nameRef);
    if (err == noErr)
        CFStringGetCString(nameRef, nameBuf, bufSize, kCFStringEncodingUTF8);

    return err;
}

static void minimidi_readProc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon)
{
    MiniMIDI* mm = (MiniMIDI*)readProcRefCon;

    const MIDIPacket* packet = &pktlist->packet[0];
    for (unsigned int i = 0; i < pktlist->numPackets; ++i)
    {
        printf("Packet - ts: %llu, num bytes: %hu\n", packet->timeStamp, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

int minimidi_connect_port(MiniMIDI* mm, unsigned int portNumber, const char* portName)
{
    OSStatus        err = 0;
    MIDIEndpointRef sourceRef;
    assert(mm->connectedPortName == NULL);
    assert(mm->portRef == 0);

    /* TODO: try and create string here without allocating */
    mm->connectedPortName = CFStringCreateWithCString(NULL, portName, kCFStringEncodingASCII);
    err = MIDIInputPortCreate(mm->clientRef, mm->connectedPortName, minimidi_readProc, mm, &mm->portRef);

    if (err != noErr)
        goto failed;

    sourceRef = MIDIGetSource(portNumber);

    if (sourceRef == 0)
        goto failed;

    err = MIDIPortConnectSource(mm->portRef, sourceRef, NULL);
    if (err != noErr)
        goto failed;

    return err;

failed:
    minimidi_disconnect_port(mm);
    if (err == 0)
        err = 1;
    return err;
}

void minimidi_disconnect_port(MiniMIDI* mm)
{
    if (mm->portRef != 0)
    {
        MIDIPortDispose(mm->portRef);
        mm->portRef = 0;
    }
    if (mm->connectedPortName != NULL)
    {
        CFRelease(mm->connectedPortName);
        mm->connectedPortName = NULL;
    }
}

#endif /* __APPLE__ */

#ifdef _WIN32
#pragma comment(lib, "winmm.lib")
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mmeapi.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MiniMIDIBuffer
{
    MIDIHDR header;
    char    buffer[MINIMIDI_MIDI_BUFFER_SIZE];
} MiniMIDIBuffer;

struct MiniMIDI
{
    HMIDIIN midiInHandle;
    int     connected;
    /* Both LibreMidi and RtMidi use 4 headers.
       Can't hurt to copy them right? */
    MiniMIDIBuffer buffers[MINIMIDI_MIDI_BUFFER_COUNT];
};

void minimidi_init(MiniMIDI* mm)
{
    int i;
    memset(mm, 0, sizeof(*mm));

    for (i = 0; i < ARRSIZE(mm->buffers); i++)
    {
        MIDIHDR* head        = &mm->buffers[i].header;
        head->lpData         = &mm->buffers[i].buffer[0];
        head->dwBufferLength = ARRSIZE(mm->buffers[i].buffer);
        head->dwUser         = i;
    }
}

MiniMIDI* minimidi_create() { return (MiniMIDI*)calloc(1, sizeof(MiniMIDI)); }

void minimidi_free(MiniMIDI* mm)
{
    assert(mm != NULL);
    minimidi_disconnect_port(mm);
#ifndef MINIMIDI_USE_GLOBAL
    free(mm);
#endif
}

unsigned long minimidi_get_num_ports(MiniMIDI* mm) { return midiInGetNumDevs(); }

int minimidi_get_port_name(MiniMIDI* mm, unsigned int portNumber, char* nameBuffer, size_t bufferSize)
{
    MMRESULT   result;
    MIDIINCAPS caps;
    memset(&caps, 0, sizeof(caps));

    result = midiInGetDevCapsA(portNumber, &caps, sizeof(MIDIINCAPS));
    if (result == MMSYSERR_NOERROR)
        strcpy_s(nameBuffer, bufferSize, caps.szPname);
    return result;
}

void CALLBACK
minimidi_MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    MiniMIDI* mm = (MiniMIDI*)dwInstance;
    printf("Midi: %u\n", wMsg);

    /* https://learn.microsoft.com/en-gb/windows/win32/multimedia/mim-data?redirectedfrom=MSDN */
    if (wMsg == MIM_DATA)
    {
        UCHAR status = wMsg & 0xff;
    }
    else if (wMsg == MIM_LONGDATA)
    {
        /* handle sysex*/
    }

    return;
}

int minimidi_connect_port(MiniMIDI* mm, unsigned int portNumber, const char* portName)
{
    MMRESULT result;
    int      i;
    result =
        midiInOpen(&mm->midiInHandle, portNumber, (DWORD_PTR)&minimidi_MidiInProc, (DWORD_PTR)mm, CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR)
        goto failed;

    for (i = 0; i < ARRSIZE(mm->buffers); i++)
    {
        MIDIHDR* head = &mm->buffers[i].header;
        result        = midiInPrepareHeader(mm->midiInHandle, head, sizeof(*head));
        if (result != MMSYSERR_NOERROR)
            goto failed;
        result = midiInAddBuffer(mm->midiInHandle, head, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
            goto failed;
    }

    result = midiInStart(mm->midiInHandle);
    if (result != MMSYSERR_NOERROR)
        goto failed;

    mm->connected = 1;

    return result;

failed:
    if (mm->midiInHandle)
    {
        midiInClose(mm->midiInHandle);
        mm->midiInHandle = 0;
    }
    return result;
}
void minimidi_disconnect_port(MiniMIDI* mm)
{
    if (mm->connected)
    {
        MMRESULT result;
        int      i;
        midiInReset(mm->midiInHandle);
        midiInStop(mm->midiInHandle);

        for (i = 0; i < ARRSIZE(mm->buffers); i++)
        {
            MIDIHDR* head = &mm->buffers[i].header;
            result        = midiInUnprepareHeader(mm->midiInHandle, head, sizeof(*head));

            if (result != MMSYSERR_NOERROR)
                break;
        }
        midiInClose(mm->midiInHandle);
        mm->midiInHandle = 0;
        mm->connected    = 0;
    }
}

#endif /* _WIN32 */

#ifdef MINIMIDI_USE_GLOBAL
static MiniMIDI g_minimidi;
MiniMIDI*       minimidi_get_global(void) { return &g_minimidi; }
#endif /* MINIMIDI_USE_GLOBAL */

#endif /* MINIMIDI_IMPL */

#ifdef __cplusplus
}
#endif