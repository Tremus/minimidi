/* MINIMIDI by Tré Dudman
 * STB style header library.
 * Only handles MIDI input on Windows & MacOS, skipping SYSEX messages.
 *
 * DOCS:
 * #define MINIMIDI_IMPL once in your project to get the OS specific implementation
 *
 * #define MINIMIDI_USE_GLOBAL to add a static global MiniMIDI in the implementation.
 * You can access this object by calling minimidi_get_global();
 *
 * #define MINIMIDI_MALLOC & MINIMIDI_FREE to use your own allocator
 * #define MINIMIDI_ASSERT to use your own assert
 */

#ifdef __cplusplus
extern "C" {
#endif
#ifndef MINIMIDI_H
#define MINIMIDI_H

#ifndef MINIMIDI_RINGBUFFER_SIZE
#define MINIMIDI_RINGBUFFER_SIZE 128
#endif

#include <stddef.h>

typedef struct MiniMIDI MiniMIDI;

int       minimidi_init(MiniMIDI* mm);
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

#ifdef _WIN32
/* Windows aren't very helpful in telling you when your device is disconnected
   They can tell you when a device disconnected and give you its name, but the name is not guaranteed to be unique.
   What we've chosen to do to is set an internal flag when ANY device is disconnected.
   This function returns the result of the flag */
int minimidi_should_reconnect(MiniMIDI* mm);
/* Scans through available ports looking the port number of the last connected device.
   If found, it will connect to that port and return 1. Returns 0 in all other cases.
   This isn't a catch all solution, it just suits most cases.
   If you know a more relaiable solution, please contact me @ https://github.com/Tremus/minimidi */
int minimidi_try_reconnect(MiniMIDI* mm, const char* portName);
#endif

typedef struct MiniMIDIMessage
{
    union
    {
        struct
        {
            unsigned char status;
            unsigned char data1;
            unsigned char data2;
        };
        unsigned char bytes[4];
        unsigned int  bytesAsInt;
    };
    /* Milliseconds since first connected to MIDI port */
    unsigned int timestampMs;
} MiniMIDIMessage;

/* If there are no new messages, the returned message will be all blank (zeros) */
MiniMIDIMessage minimidi_read_message(MiniMIDI* mm);

unsigned minimidi_calc_num_bytes_from_status(unsigned char status_byte);

#endif /* MINIMIDI_H */

#define MINIMIDI_IMPL
#ifdef MINIMIDI_IMPL
#undef MINIMIDI_IMPL

#define ARRSIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#define MINIMIDI_MIDI_BUFFER_COUNT 4
#define MINIMIDI_MIDI_BUFFER_SIZE 1024

#ifndef MINIMIDI_MALLOC
#include <stdlib.h>
#define MINIMIDI_MALLOC(ctx, size) malloc(size)
#define MINIMIDI_FREE(ctx, ptr) free(ptr)
#endif

#ifndef MINIMIDI_ASSERT
#include <assert.h>
#define MINIMIDI_ASSERT assert
#endif

/* Naive ring buffer. The writer will not update the tail. The reader is expected to read in time */
typedef struct MIDIMidiRingBuffer
{
    volatile int writePos;
    volatile int readPos;

    MiniMIDIMessage buffer[MINIMIDI_RINGBUFFER_SIZE];
} MIDIMidiRingBuffer;

unsigned minimidi_calc_num_bytes_from_status(unsigned char status_byte)
{
    /* https://www.midi.org/specifications-old/item/table-2-expanded-messages-list-status-bytes  */
    /* https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2 */
    /* https://www.recordingblogs.com/wiki/midi-quarter-frame-message */
    switch (status_byte)
    {
    case 0x80 ... 0xbf:
    case 0xe0 ... 0xef:
    case 0xf2:
        return 3;
    case 0xc0 ... 0xdf:
    case 0xf1:
        return 2;
    default:
        return 1;
    }
}

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <CoreMIDI/CoreMIDI.h>

struct MiniMIDI
{
    CFStringRef   clientName;
    MIDIClientRef clientRef;

    MIDIPortRef portRef;
    UInt64      connectionStartNanos;
    CFStringRef connectedPortName;

    MIDIMidiRingBuffer ringBuffer;
};

int  minimidi_atomic_load_i32(const volatile int* ptr) { return __atomic_load_n(ptr, __ATOMIC_SEQ_CST); }
void minimidi_atomic_store_i32(volatile int* ptr, int v) { __atomic_store_n(ptr, v, __ATOMIC_SEQ_CST); }

int minimidi_init(MiniMIDI* mm)
{
    OSStatus error;

    memset(mm, 0, sizeof(*mm));
    /* TODO: try and create string here without allocating */
    mm->clientName = CFStringCreateWithCString(NULL, "MiniMIDI Input Client", kCFStringEncodingASCII);
    error          = MIDIClientCreate(mm->clientName, NULL, NULL, &mm->clientRef);

    return error;
}

MiniMIDI* minimidi_create()
{
    MiniMIDI* mm = (MiniMIDI*)MINIMIDI_MALLOC(NULL, sizeof(MiniMIDI));
    minimidi_init(mm);
    return mm;
}

void minimidi_free(MiniMIDI* mm)
{
    MINIMIDI_ASSERT(mm != NULL);
    minimidi_disconnect_port(mm);
    if (mm->clientName != NULL)
        CFRelease(mm->clientName);
#ifndef MINIMIDI_USE_GLOBAL
    MINIMIDI_FREE(NULL, mm);
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
    MiniMIDI*         mm       = (MiniMIDI*)readProcRefCon;
    const MIDIPacket* packet   = &pktlist->packet[0];
    int               writePos = minimidi_atomic_load_i32(&mm->ringBuffer.writePos);
    unsigned int      i;

    for (i = 0; i < pktlist->numPackets; ++i)
    {
        /* Either MacOS, or the cheap hardware I used while testing this, appears to send junk data if the device is
           unplugged then plugged back in. Behind the scenes, MacOS will simply reconnect you, then sends you the data.
           If this assumption is correct, then some sneaky data will lead with a valid status byte and get through...
           Here we cautiously exit the proc */
        if (packet->length == 0)
            return;
        if (*packet->data < 0x80)
            return;

        MiniMIDIMessage message;
        /* MacOS timestamps come in their own ill defined format.
           Here we convert it to num milliseconds since the beginning of the connection.
           This matches the timestamp format Windows Multimedia sends in their MIDI read callbacks */
        message.timestampMs = (AudioConvertHostTimeToNanos(packet->timeStamp) - mm->connectionStartNanos) / 1e6;

        /* MacOS can send several MIDI messages within the same packet.
           Here we push each MIDI message to our ring buffer, ignoring the SYSEX packets */

        const Byte* bytes          = &packet->data[0];
        unsigned    remainingBytes = packet->length;

        while (remainingBytes != 0)
        {
            message.status = *bytes;

            /* Skip SYSEX */
            if (message.status == 0xf0)
                return;

            unsigned numMsgBytes = minimidi_calc_num_bytes_from_status(message.status);

            if (numMsgBytes != 1)
                message.data1 = bytes[1];
            if (numMsgBytes == 3)
                message.data2 = bytes[2];

            mm->ringBuffer.buffer[writePos] = message;
            writePos++;
            writePos = writePos % ARRSIZE(mm->ringBuffer.buffer);
            minimidi_atomic_store_i32(&mm->ringBuffer.writePos, writePos);

            bytes          += numMsgBytes;
            remainingBytes -= numMsgBytes;
        }

        packet = MIDIPacketNext(packet);
    }
}

int minimidi_connect_port(MiniMIDI* mm, unsigned int portNumber, const char* portName)
{
    OSStatus        err = 0;
    MIDIEndpointRef sourceRef;
    MINIMIDI_ASSERT(mm->connectedPortName == NULL);
    MINIMIDI_ASSERT(mm->portRef == 0);

    /* TODO: try and create string here without allocating */
    mm->connectedPortName = CFStringCreateWithCString(NULL, portName, kCFStringEncodingASCII);
    err = MIDIInputPortCreate(mm->clientRef, mm->connectedPortName, minimidi_readProc, mm, &mm->portRef);

    if (err != noErr)
        goto failed;

    sourceRef = MIDIGetSource(portNumber);

    if (sourceRef == 0)
        goto failed;

    err = MIDIPortConnectSource(mm->portRef, sourceRef, NULL);

    /* mm->connectionStartNanos = AudioConvertHostTimeToNanos(mach_absolute_time()) */
    mm->connectionStartNanos = AudioConvertHostTimeToNanos(AudioGetCurrentHostTime());
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
#pragma comment(lib, "cfgmgr32.lib")
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cfgmgr32.h>
#include <mmeapi.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef DRV_QUERYDEVICEINTERFACE
#define DRV_QUERYDEVICEINTERFACE 0x080c
#define DRV_QUERYDEVICEINTERFACESIZE 0x080d
#endif

typedef struct MiniMIDIBuffer
{
    MIDIHDR header;
    char    buffer[MINIMIDI_MIDI_BUFFER_SIZE];
} MiniMIDIBuffer;

struct MiniMIDI
{
    HMIDIIN         midiInHandle;
    HCMNOTIFICATION notifyContext;

    int lastConnectedPortNum;
    /* set to 1 whenever a device is removed */
    volatile LONG shouldReconnect;

    int connected;

    MIDIMidiRingBuffer ringBuffer;
    /* Both LibreMidi and RtMidi use 4 headers.
       Can't hurt to copy them right? */
    MiniMIDIBuffer buffers[MINIMIDI_MIDI_BUFFER_COUNT];
};

int  minimidi_atomic_load_i32(volatile int* ptr) { return _InterlockedCompareExchange((volatile LONG*)ptr, 0, 0); }
void minimidi_atomic_store_i32(volatile int* ptr, int v) { _InterlockedExchange((volatile LONG*)ptr, v); }

int minimidi_init(MiniMIDI* mm)
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
    return 0;
}

MiniMIDI* minimidi_create()
{
    MiniMIDI* mm = MINIMIDI_MALLOC(NULL, sizeof(MiniMIDI));
    minimidi_init(mm);
    return mm;
}

void minimidi_free(MiniMIDI* mm)
{
    assert(mm != NULL);
    minimidi_disconnect_port(mm);
#ifndef MINIMIDI_USE_GLOBAL
    MINIMIDI_FREE(NULL, mm);
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

/*
 * wMsg: message type
 * dwParam1: midi status byte followed by up to 2 data bytes. The remaining bytes are junk.
 * dwParam2: represents the time in milliseconds since the port connected
 */
void CALLBACK
minimidi_MidiInProc(HMIDIIN hMidiIn, UINT wMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    MiniMIDI* mm = (MiniMIDI*)dwInstance;

    /* https://learn.microsoft.com/en-gb/windows/win32/multimedia/mim-data?redirectedfrom=MSDN */
    if (wMsg == MM_MIM_DATA)
    {
        MiniMIDIMessage msg;
        int             writePos;

        /* take first 3 bytes. remember, the rest are junk, including possibly the ones we're taking */
        msg.bytesAsInt  = dwParam1 & 0xffffff;
        msg.timestampMs = dwParam2;

        writePos = minimidi_atomic_load_i32(&mm->ringBuffer.writePos);

        mm->ringBuffer.buffer[writePos] = msg;
        writePos++;
        writePos = writePos % ARRSIZE(mm->ringBuffer.buffer);
        minimidi_atomic_store_i32(&mm->ringBuffer.writePos, writePos);
    }
    /* handle sysex*/
    /* https://www.midi.org/specifications-old/item/table-4-universal-system-exclusive-messages */
    /* else if (wMsg == MIM_LONGDATA) {} */
}

DWORD CALLBACK minimidi_CM_NOTIFY_CALLBACK(
    _In_ HCMNOTIFICATION                                  hNotify,
    _In_opt_ PVOID                                        Context,
    _In_ CM_NOTIFY_ACTION                                 Action,
    _In_reads_bytes_(EventDataSize) PCM_NOTIFY_EVENT_DATA EventData,
    _In_ DWORD                                            EventDataSize)
{
    if (Action == CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED &&
        EventData->FilterType == CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE)
    {
        MiniMIDI* mm = Context;

        if (Action == CM_NOTIFY_ACTION_DEVICEINSTANCEREMOVED)
            minimidi_atomic_store_i32((volatile int*)&mm->shouldReconnect, 1);
    }
    return 0;
}

int minimidi_connect_port(MiniMIDI* mm, unsigned int portNumber, const char* portName)
{
    MMRESULT         result;
    int              i;
    CM_NOTIFY_FILTER notifyFilter;

    result =
        midiInOpen(&mm->midiInHandle, portNumber, (DWORD_PTR)&minimidi_MidiInProc, (DWORD_PTR)mm, CALLBACK_FUNCTION);

    if (result != MMSYSERR_NOERROR)
        goto failed;

    memset(&notifyFilter, 0, sizeof(notifyFilter));
    notifyFilter.cbSize     = sizeof(notifyFilter);
    notifyFilter.Flags      = CM_NOTIFY_FILTER_FLAG_ALL_DEVICE_INSTANCES;
    notifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINSTANCE;

    result = CM_Register_Notification(&notifyFilter, mm, minimidi_CM_NOTIFY_CALLBACK, &mm->notifyContext);
    if (result != CR_SUCCESS)
        goto failed;

    for (i = 0; i < ARRSIZE(mm->buffers); i++)
    {
        result = midiInPrepareHeader(mm->midiInHandle, &mm->buffers[i].header, sizeof(mm->buffers[i].header));
        if (result != MMSYSERR_NOERROR)
            goto failed;
        result = midiInAddBuffer(mm->midiInHandle, &mm->buffers[i].header, sizeof(MIDIHDR));
        if (result != MMSYSERR_NOERROR)
            goto failed;
    }

    result = midiInStart(mm->midiInHandle);
    if (result != MMSYSERR_NOERROR)
        goto failed;

    mm->connected            = 1;
    mm->lastConnectedPortNum = portNumber;

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
    CM_Unregister_Notification(mm->notifyContext);
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

int minimidi_should_reconnect(MiniMIDI* mm) { return _InterlockedCompareExchange(&mm->shouldReconnect, 0, 1); }

int minimidi_try_reconnect(MiniMIDI* mm, const char* portName)
{
    int i;
    int numPorts = minimidi_get_num_ports(mm);
    for (i = 0; i < numPorts; i++)
    {
        if (i == mm->lastConnectedPortNum)
        {
            minimidi_disconnect_port(mm);
            return minimidi_connect_port(mm, i, portName) == 0;
        }
    }

    return 0;
}

#endif /* _WIN32 */

MiniMIDIMessage minimidi_read_message(MiniMIDI* mm)
{
    MiniMIDIMessage msg;
    int             writePos, readPos;

    msg.bytesAsInt  = 0;
    msg.timestampMs = 0;
    writePos        = minimidi_atomic_load_i32(&mm->ringBuffer.writePos);
    readPos         = minimidi_atomic_load_i32(&mm->ringBuffer.readPos);

    if (readPos != writePos)
    {
        msg = mm->ringBuffer.buffer[readPos];
        readPos++;
        readPos = readPos % ARRSIZE(mm->ringBuffer.buffer);
        minimidi_atomic_store_i32(&mm->ringBuffer.readPos, readPos);
    };
    return msg;
}

#ifdef MINIMIDI_USE_GLOBAL
static MiniMIDI g_minimidi;
MiniMIDI*       minimidi_get_global(void) { return &g_minimidi; }
#endif /* MINIMIDI_USE_GLOBAL */

#endif /* MINIMIDI_IMPL */

#ifdef __cplusplus
}
#endif