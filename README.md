# MINIMIDI

Mini STB style header library.

Listens to desired MIDI input port on Windows & MacOS, skipping SYSEX messages.

The motivation for this library was that I needed a library in C that doesn't allocate memory every time it recieves a new MIDI message, or when another reading thread tries to read from the MIDI ring buffer.

It was also intended to be used by instruments in standalone applications, hence lacking support for SYSEX.

### What's next?
In future, support for linux will likely be added. Also support for SYSEX messgaes may be added in future.

MIDI output is unlikely...