/* This example is based on the qmidiin.c exmaple from the RtMidi library */

#include <signal.h>
#include <stdio.h>

#define MINIMIDI_IMPL
#define MINIMIDI_USE_GLOBAL
#include "minimidi.h"

#ifdef _WIN32
#define SLEEP(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP(ms) usleep(ms * 1000)
#endif

int         done = 0;
static void finish(int ignore) { done = 1; }

int main()
{
    struct MiniMIDI* mm;
    unsigned int     numPorts;
    int              portNameLen;
    char             portName[128];
    int              err;

    mm = minimidi_get_global();
    minimidi_init(mm);

    numPorts = minimidi_get_num_ports(mm);
    if (numPorts == 0)
    {
        printf("No ports available!\n");
        return 1;
    }
    err = minimidi_get_port_name(mm, 0, portName, sizeof(portName));
    if (err != 0)
    {
        printf("Failed getting name!\n");
        return 1;
    }
    err = minimidi_connect_port(mm, 0, "MiniMIDI example");
    if (err != 0)
    {
        printf("Failed connecting to port 0!\n");
        return 1;
    }

    /* Set up callback for user hitting Ctrl-C
       A neat trick found in qmidiin.c */
    (void)signal(SIGINT, finish);

    printf("Reading MIDI from port %s. Quit with Ctrl-C.\n", portName);
    while (done == 0)
    {
        MiniMIDIMessage msg;
        do
        {
            msg = minimidi_read_message(mm);

            if (msg.timestampMs != 0)
            {
                if ((msg.status & 0xf0) == 0x80)
                {
                    unsigned channel  = msg.status & 0x0f;
                    unsigned note     = msg.data1;
                    unsigned velocity = msg.data2;
                    printf("note off... channel: %u, note: %u, velocity: %u\n", channel, note, velocity);
                }
                else if ((msg.status & 0xf0) == 0x90)
                {
                    unsigned channel  = msg.status & 0x0f;
                    unsigned note     = msg.data1;
                    unsigned velocity = msg.data2;
                    printf("note on! channel: %u, note: %u, velocity: %u\n", channel, note, velocity);
                }
            }
        }
        while (msg.timestampMs != 0 && done != 0);

        SLEEP(10);
    }
    minimidi_disconnect_port(mm);

    /* OS cleans up automatically when process exits... */
    /* minimidi_free(mm); */
    return err;
}