#ifdef __cplusplus
extern "C" {
#endif
#ifndef MINIMIDI_H
#define MINIMIDI_H

#include <stddef.h>

typedef struct MiniMIDI MiniMIDI;

MiniMIDI* minimidi_init();
void      minimidi_free(MiniMIDI* device);

unsigned long minimidi_get_num_ports(MiniMIDI* device);

/* Fills 'nameBuffer' with null terminated string
   Returns 0 on success */
int minimidi_get_port_name(MiniMIDI* device, unsigned int portNumber, char* nameBuffer, size_t bufferSize);

/* Creates a port with given name.
   Returns 0 on success */
int  minimidi_connect_port(MiniMIDI* device, unsigned int portNumber, const char* portName);
void minimidi_disconnect_port(MiniMIDI* device);

/*
    TODO:
    - get midi message from tueue
    - push midi message to queue
*/

#endif /* MINIMIDI_H */

#define MINIMIDI_IMPL
#ifdef MINIMIDI_IMPL
#undef MINIMIDI_IMPL

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

MiniMIDI* minimidi_init()
{
    MiniMIDI* ptr = (MiniMIDI*)calloc(1, sizeof(MiniMIDI));

    /* TODO: try and create string here without allocating */
    ptr->clientName = CFStringCreateWithCString(NULL, "MiniMIDI Input Client", kCFStringEncodingASCII);
    OSStatus error  = MIDIClientCreate(ptr->clientName, NULL, NULL, &ptr->clientRef);

    if (error != noErr)
    {
        minimidi_free(ptr);
        return NULL;
    }

    return ptr;
}

void minimidi_free(MiniMIDI* ptr)
{
    assert(ptr != NULL);
    minimidi_disconnect_port(ptr);
    if (ptr->clientName != NULL)
        CFRelease(ptr->clientName);
    free(ptr);
}

unsigned long minimidi_get_num_ports(MiniMIDI* device)
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
int minimidi_get_port_name(MiniMIDI* device, unsigned int portNum, char* nameBuf, size_t bufSize)
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
    MiniMIDI* device = (MiniMIDI*)readProcRefCon;

    const MIDIPacket* packet = &pktlist->packet[0];
    for (unsigned int i = 0; i < pktlist->numPackets; ++i)
    {
        printf("Packet - ts: %llu, num bytes: %hu\n", packet->timeStamp, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

int minimidi_connect_port(MiniMIDI* device, unsigned int portNumber, const char* portName)
{
    OSStatus        err = 0;
    MIDIEndpointRef sourceRef;
    assert(device->connectedPortName == NULL);
    assert(device->portRef == 0);

    /* TODO: try and create string here without allocating */
    device->connectedPortName = CFStringCreateWithCString(NULL, portName, kCFStringEncodingASCII);
    err =
        MIDIInputPortCreate(device->clientRef, device->connectedPortName, minimidi_readProc, device, &device->portRef);

    if (err != noErr)
        goto failed;

    sourceRef = MIDIGetSource(portNumber);

    if (sourceRef == 0)
        goto failed;

    err = MIDIPortConnectSource(device->portRef, sourceRef, NULL);
    if (err != noErr)
        goto failed;

    return err;

failed:
    minimidi_disconnect_port(device);
    if (err == 0)
        err = 1;
    return err;
}

void minimidi_disconnect_port(MiniMIDI* device)
{
    if (device->portRef != 0)
    {
        MIDIPortDispose(device->portRef);
        device->portRef = 0;
    }
    if (device->connectedPortName != NULL)
    {
        CFRelease(device->connectedPortName);
        device->connectedPortName = NULL;
    }
}

#endif /* __APPLE__ */

#endif /* MINIMIDI_IMPL */

#ifdef __cplusplus
}
#endif