/* This example is based on the qmidiin.c exmaple from the RtMidi library */

#include <signal.h>
#include <stdio.h>

#define MINIMIDI_IMPL
#define MINIMIDI_USE_GLOBAL
#include "minimidi.h"

#ifdef _WIN32
#define SLEEP(ms) Sleep(ms)
#define print(str, ...) (printf(str, __VA_ARGS__), fflush(stdout))
#else
#include <unistd.h>
#define SLEEP(ms) usleep(ms * 1000)
#define print printf
#endif

int         shouldExit = 0;
static void quit(int ignore)
{
    print("Shutting down\n");
    shouldExit = 1;
}

int main()
{
    MiniMIDI*    mm;
    unsigned int numPorts;
    char         portName[128];
    int          err;

    mm = minimidi_get_global();
    minimidi_init(mm);

    numPorts = minimidi_get_num_ports(mm);
    if (numPorts == 0)
    {
        print("No ports available!\n");
        return 1;
    }
    err = minimidi_get_port_name(mm, 0, portName, sizeof(portName));
    if (err != 0)
    {
        print("Failed getting name!\n");
        return 1;
    }
    err = minimidi_connect_port(mm, 0, "MiniMIDI example");
    if (err != 0)
    {
        print("Failed connecting to port 0!\n");
        return 1;
    }

    /* Set up callback for user hitting Ctrl-C
       A neat trick found in qmidiin.c */
    (void)signal(SIGINT, quit);

    print("Reading MIDI from port %s. Quit with Ctrl-C.\n", portName);
    while (shouldExit == 0)
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
                    print("note off... channel: %u, note: %u, velocity: %u\n", channel, note, velocity);
                }
                else if ((msg.status & 0xf0) == 0x90)
                {
                    unsigned channel  = msg.status & 0x0f;
                    unsigned note     = msg.data1;
                    unsigned velocity = msg.data2;
                    print("note on! channel: %u, note: %u, velocity: %u\n", channel, note, velocity);
                }
            }
        }
        while (msg.timestampMs != 0 && shouldExit != 0);

#ifdef _WIN32
        /* Hotplugging for windows. On MacOS it's automatic... */
        if (minimidi_should_reconnect(mm))
        {
            static const int HOTPLUG_TIMEOUT        = (1000 * 60 * 2); /* 2min */
            static const int HOTPLUG_SLEEP_INTERVAL = 100;             /* 100ms */
            int              msCounter              = 0;

            print("WARNING: Unknown device disconnected!\n");
            print("If this was your MIDI device, please plug it back in. This program will automatically reconnect.\n");

            while (msCounter < HOTPLUG_TIMEOUT && shouldExit == 0)
            {
                if (minimidi_try_reconnect(mm, "MiniMIDI example"))
                {
                    print("Successfully reconnected!\n");
                    break;
                }

                SLEEP(HOTPLUG_SLEEP_INTERVAL);
                msCounter += HOTPLUG_SLEEP_INTERVAL;
            }
        }
#endif

        SLEEP(10);
    }
    minimidi_disconnect_port(mm);

    /* OS cleans up automatically when process exits... */
    /* minimidi_free(mm); */
    return err;
}