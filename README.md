# parallel-dos
![screenshot](https://github.com/erco77/parallel-dos/blob/master/parallel-screenshot.jpg)

Builds in Turbo C 3.0; see Makefile

This program monitors and controls the bits on any parallel port. This does NOT support modern EPP/ECP modes.

To run this program, just invoke 'parallel' from the DOS prompt. The default will be to monitor LPT1.
You can specify '1' or '2' as an optional argument on the command line to specify which LPT port to monitor.
Or you can specify the full base port# (e.g. 0378). 'parallel -help' to see a help screen.

Lines shown in BOLD are "input" signals. The rest are either "outputs" or "ground" pins.

The STATE column monitors the realtime input and output bit states on the port:

    'SET' if the bit is set
    'clr' if the bit is clear.

You can use the UP/DOWN arrow keys to move the inverse "cursor" (the white block) to focus on a particular pin#:

    When sitting on an input (bold), the speaker audibly 'beeps' like a multimeter whenever the input is SET.
    When sitting on an output (normal), hit the ENTER key to toggle the state of that pin's output.

All the input pins (10,11,12,13 and 15) each have scrolling 'oscilloscope' that runs along the bottom of the screen to show the set/clear state. The scrolling aspect allows one to more easily oscillations on the input.

Hit ESC to exit this program.

REQUIREMENTS
------------
"ANSI.SYS" needs to be installed for screen positioning and colors to work, or you'll see control characters.
In your C:\CONFIG.SYS file, make sure you have:

    DEVICE=C:\DOS\ANSI.SYS

..adjust the path to ANSI.SYS based on where your DOS directory is installed; ANSI.SYS comes with DOS.
