# PicoK7

Tape player emulation for ZX81.

In this first step, the objective of this project is to convert a .P file, containing a ZX81 tape image, into a series of pulses to be fed to a ZX81 computer through the EAR input.

Future ideas include generating .P files from the MIC output of the ZX81 and from the output of a tape player playing a real ZX81 K7 tape.

## Directories

* PExplorer: a simple C# program to analyze (explore) .P files. It can also export the data in a .P file into a C header file.
* TstK7: first test, sends a fixed program using busy-loop delays for the timing.
* TstK7Pio: uses the RP2040's PIO to generate the pulses. Can send continuous 0 and continuous 1, for timing checks with an oscilloscope. Can also send a fixed program, to check with an actual ZX81.

## Hardware

The final hardware will include a RP2040 board, a micro SD card adapter, a monochrome graphic LCD display and a rotary encoder.

For testing I am using a Raspberry Pi Pico board (because it has a debug header).

To interface the RP2040 pulse output to the ZX81 a simple NPN transistor is used. Its base is connected to the RP2040 pin through a 1k resistor. The collector is connected to +5V through a 1k resistor and to the output connector. The emitter is connected to ground. This will simply convert the 3.3V pulses into (inverted) 5V pulses supplying/sinking adequate current. The ZX81 input has a capacitor to remove the DC component in this signal. 