#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <dos.h>
#include <conio.h>

#undef outp		// Turbo C 3.0 needs this

//
// parallel.c - DOS parallel port monitoring program
//
// (C) Copyright Greg Ercolano 1988, 2007.
// (C) Copyright Seriss Corporation 2008, 2020.
// Available under GPL2. http://github.com/erco77/parallel-dos\n"
//
// 80 //////////////////////////////////////////////////////////////////////////
//

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned long  ulong;

// Tone frequency when "input bit set" (in HZ)
#define TONE_FREQ	3000

// Pin->dir
#define OUT 1
#define IN  2
#define GND 3

// ANSI colors
#define NORMAL    "\033[0m"
#define BOLD      "\033[1m"
#define UNDERLINE "\033[4m"
#define INVERSE   "\033[7m"

typedef struct {
    int x,y;	 	// x/y position on screen
    int port;		// port offset. actual_port = (portbase+port)
    int mask;		// mask bit
    int inv;		// 1=actual hardware output is inverted
    int dir;		// IN, OUT or GND
    const char *label; 	// label for this pin
    uchar laststate;	// last state of port (optimizes redraws)
} Pin;

// GLOBALS
Pin *pins[26];

// CREATE AN INSTANCE OF A 'Pin' STRUCT
Pin* MakePin(int x, int y, int port, 
             int mask, int inv, int dir,
	     const char *label) {
    Pin *p = (Pin*)malloc(sizeof(Pin));
    p->x     = x;
    p->y     = y;
    p->port  = port;
    p->mask  = mask;
    p->inv   = inv;
    p->dir   = dir;
    p->label = label;
    p->laststate = -1;
    return p;
}

// PEEK THE BYTE AT ADDRESS <segment>:<offset>
uchar PeekByte(ushort segment, ushort offset)
{
    uchar far *addr = MK_FP(segment, offset);
    return *addr;
}

void Init(void)
{
    //                 X   Y     PORT MASK  INV DIR  LABEL
    //                 -   -     ---- ----  --- ---  ---------
    pins[ 1] = MakePin(5,  2+5,  2,   0x01, 1,  OUT, "-strobe");
    pins[ 2] = MakePin(5,  2+6,  0,   0x01, 0,  OUT, "+data0");
    pins[ 3] = MakePin(5,  2+7,  0,   0x02, 0,  OUT, "+data1");
    pins[ 4] = MakePin(5,  2+8,  0,   0x04, 0,  OUT, "+data2");
    pins[ 5] = MakePin(5,  2+9,  0,   0x08, 0,  OUT, "+data3");
    pins[ 6] = MakePin(5,  2+10, 0,   0x10, 0,  OUT, "+data4");
    pins[ 7] = MakePin(5,  2+11, 0,   0x20, 0,  OUT, "+data5");
    pins[ 8] = MakePin(5,  2+12, 0,   0x40, 0,  OUT, "+data6");
    pins[ 9] = MakePin(5,  2+13, 0,   0x80, 0,  OUT, "+data7");
    pins[10] = MakePin(5,  2+14, 1,   0x40, 1,  IN,  "-ack");
    pins[11] = MakePin(5,  2+15, 1,   0x80, 1,  IN,  "+busy");
    pins[12] = MakePin(5,  2+16, 1,   0x20, 0,  IN,  "+outpap");
    pins[13] = MakePin(5,  2+17, 1,   0x10, 0,  IN,  "+sel");
    pins[14] = MakePin(45, 2+5,  2,   0x02, 1,  OUT, "-autofeed");
    pins[15] = MakePin(45, 2+6,  1,   0x08, 0,  IN,  "-error");
    pins[16] = MakePin(45, 2+7,  2,   0x04, 0,  OUT, "-init");
    pins[17] = MakePin(45, 2+8,  2,   0x08, 1,  OUT, "-sel");
    pins[18] = MakePin(45, 2+9,  0,   0x00, 0,  GND, "gnd");
    pins[19] = MakePin(45, 2+10, 0,   0x00, 0,  GND, "gnd");
    pins[20] = MakePin(45, 2+11, 0,   0x00, 0,  GND, "gnd");
    pins[21] = MakePin(45, 2+12, 0,   0x00, 0,  GND, "gnd");
    pins[22] = MakePin(45, 2+13, 0,   0x00, 0,  GND, "gnd");
    pins[23] = MakePin(45, 2+14, 0,   0x00, 0,  GND, "gnd");
    pins[24] = MakePin(45, 2+15, 0,   0x00, 0,  GND, "gnd");
    pins[25] = MakePin(45, 2+16, 0,   0x00, 0,  GND, "gnd");
}

// SEND A TONE TO THE SPEAKER
void Tone(int onoff)
{
    static int last = 0;
    if ( onoff== 0 )  { nosound();        last = 0; return; }
    if ( last == 0 )  { sound(TONE_FREQ); last = 1; return; }
}

// SCROLL SINGLE LINE RIGHT STARTING AT x,y
void ScrollRight(int x, int y)
{
    uchar far *mono = MK_FP(0xb000, (y-1)*160+160-1-2); // (x*2));
    uchar far *cga  = MK_FP(0xb800, (y-1)*160+160-1-2); // (x*2));
    for ( ; x < 80; x++ ) {
        // ATTRIB                      // CHAR
	*(mono+2) = *mono; mono--;     *(mono+2) = *mono; mono--;
	*(cga+2)  = *cga;  cga--;      *(cga+2)  = *cga;  cga--;
    }
}

// PLOT CHAR 'c' AT POSITION x,y
void Plot(int x, int y, uchar c)
{
    uchar far *mono = MK_FP(0xb000, ((y-1)*160)+(x*2));
    uchar far *cga  = MK_FP(0xb800, ((y-1)*160)+(x*2));
    *mono = c;
    *cga  = c;
}

// BREAK (^C) HANDLER
void BreakHandler(void)
{
    signal(SIGINT, BreakHandler);
}

// SHOW HELP AND EXIT PROGRAM
void HelpAndExit(void)
{
    printf(
        "parallel - monitor/control an IBM PC parallel port\n"
	"    (C) Copyright Greg Ercolano 1988, 2007.\n"
	"    (C) Copyright Seriss Corporation 2008, 2020.\n"
	"    Available under GPL2. http://github.com/erco77/parallel\n"
        "\n"
        "USAGE\n"
	"    parallel [-h] [port|lpt#]\n"
	"\n"
        "EXAMPLES\n"
	"    parallel     - monitor LPT1 (default)\n"
	"    parallel 1   - monitor LPT1\n"
	"    parallel 2   - monitor LPT2\n"
	"    parallel 3   - monitor LPT3\n"
	"    parallel 378 - monitor parallel port 0378h\n"
	"    parallel -h[elp] - this help screen\n"
	"\n"
	"KEYS\n"
	"    ESC        - quit program\n"
	"    UP/DOWN    - move edit cursor up/down\n"
	"    ENTER      - toggles state of output (when cursor on an output)\n"
	"\n"
        "    While edit cursor is on an input, speaker makes a %d HZ tone.\n",
	TONE_FREQ);
    exit(0);
}

// REPORT PORT# FOR GIVEN LPT#
int LPT2Port(int lpt)
{
    // IBM PC: Stores port for LPT1/2/3 at 0040:0008 - 0004:000d
    return (PeekByte(0x0040, 0x0008 + (((lpt-1)*2)+0)) << 0) |
	   (PeekByte(0x0040, 0x0008 + (((lpt-1)*2)+1)) << 8);
}

int main(int argc, char **argv)
{
    int portbase = 0;
    int edit   = 1;	// pin# currently being edited
    int done   = 0;	// set to 1 when user hits ESC
    int redraw = 1;	// set to 1 to do a full redraw
    int lpt    = 1;
    ulong lasttime = time(NULL);

    // USE LPT1 BY DEFAULT
    //    If there is no LPT, portbase will be 0,
    //    which we'll catch and fail below..
    //
    portbase = LPT2Port(lpt);

    // PARSE COMMAND LINE
    if (argc >= 2) {

	// -help?
	if ( argv[1][0] == '-' && argv[1][1] == 'h' ) 
	    HelpAndExit();

        // USER SPECIFIED PORT/LPT#?
        if ( sscanf(argv[1], "%x", &lpt) != 1 ) {
	    printf("parallel: '%s' bad LPT number "
	           "(expected 1,2,3,4 or 0378, 03eb, etc)\n",
	        argv[1]);
	    exit(1);
	}

	if ( lpt > 0 && lpt <= 3 ) {	// LPT#?
	    portbase = LPT2Port(lpt);
	} else if ( lpt > 0x100 ) {	// PORT#?
	    portbase = lpt;		// user specified a port#
	    lpt      = -1;		// lpt# unknown
	} else {
	    printf("parallel: '%s' bad LPT or port#\n", argv[1]);
	    exit(1);
	}
    }

    // INVALID PARALLEL PORT?
    if ( portbase == 0 ) {
        if ( lpt >= 1 && lpt <= 3 ) {
	    printf("parallel: no printer port for LPT%d\n", lpt);
	} else {
	    printf("parallel: bad port# %04x\n", portbase);
	}
	exit(1);
    }

    // DISABLE ^C
    //     Prevent interrupt while sound() left on..
    //
    BreakHandler();

    // INITIALIZE pins[] ARRAY
    Init();

    // TOP OF SCREEN
    printf("\33[2J\33[1;1H"); // clear screen, cursor to top
    printf("usage: parallel <port|lpt#> ; monitors specific parallel port\n");
    printf("\n");
    printf("%.4x = port base\n", portbase);
    printf("\n");

    // PIN HEADING
    printf("    %-39s %s\n",
           "PIN STATE  PORT MASK SIGNAL",
           "PIN STATE  PORT MASK SIGNAL");
    printf("    %-39s %s\n",
           "--- -----  ---- ---- ------",
           "--- -----  ---- ---- ------");

    while (!done) {
        Pin *p;
        int pin;
	int port;
	uchar state;
	const char *statestr;
	static int first = 1;

	// KEEP CURSOR POSITIONED AT EDIT CURSOR
	printf("\033[%d;%dH", pins[edit]->y, pins[edit]->x-1);

	for (pin=1; pin<=25; pin++) {
	    p = pins[pin];
	    port = portbase + p->port;
	    state = inp(port) & p->mask;

	    // REDRAW LINE ONLY IF STATE CHANGED
	    if ( redraw || state != p->laststate ) {
	        statestr = (p->dir==GND) ? "gnd" :
		           (state)       ? "SET" :
			   "clr";
		// HANDLE PIN COLOR
		if ( pin == edit ) printf(INVERSE);
		if ( p->dir == IN) printf(BOLD);
		// SHOW PIN
		printf("\033[%d;%dH%2d   %s   %04x %c%02x  %s",
		    p->y, p->x, 
		    pin, statestr,
		    port, (p->inv?'!':' '),
		    p->mask, p->label);
		printf(NORMAL);
		p->laststate = state;	// save state change
	    }

	    // UPDATE INPUT "OSCILLOSCOPE" AT BOTTOM OF SCREEN
	    if ( p->dir == IN ) {
	        int y = (pin == 10) ? 21 :
		        (pin == 11) ? 22 :
			(pin == 12) ? 23 :
			(pin == 13) ? 24 :
			(pin == 15) ? 25 : 0;
		if ( y ) {
		    ScrollRight(8, y);	// rotate line right

		    // DISPLAY LEGEND ONLY ONCE
		    if ( first || redraw)
		        printf("\033[%d;4H %2d [", y, pin);

		    // DISPLAY STATE IN LEFT COLUMN
		    Plot(8, y, (state ? 0xdf : '_'));	// less cursor noise
		}
	    }

	    // TONE SOUND IF INPUT PIN UNDER EDIT CURSOR 'SET'
	    if ( (pin == edit) ) {
	        if ( p->dir == IN ) Tone(state ? 1 : 0);
		else                Tone(0);
	    }
	}

	// "OSCILLOSCOPE" SECONDS MARKERS
	{
	    ulong lt = time(NULL);
	    ScrollRight(8, 20);
	    Plot(8, 20, (lt != lasttime) ? '.' : ' ');	// c2, b3
	    lasttime = lt;
	}

	delay(25);	// not too fast

	// NO LONGER FIRST EXECUTION
	first = 0;
	redraw = 0;

	/* KEYBOARD HANDLER */
	if (kbhit()) {
	    uchar c = getch();
	    // printf("\033[2HKEY=%02x\n", c);
	    switch (c) {
	        case ' ':
		case 0x1b:
		    done = 1;
		    break;

	        case '\r':
	        case '\n': {
		    // CHANGE VALUE OF PORT
		    p = pins[edit];			// pin being edited
		    if ( p->dir == IN ) break;		// early exit if input
		    port = portbase + p->port;		// port
		    state = inp(port);			// get current value
		    state ^= p->mask;			// toggle bit
		    outp(port, state);			// write modified value
		    redraw = 1;
		    break;
		}

	        case 0:
		    c = getch();
		    // printf("\033[2HKEY=%02x\n", c);
		    switch(c) {
			case 0x48:	/* UP ARROW */
			    if (--edit < 1) edit = 17;
			    redraw = 1;
			    break;
		        case 0x50: 	/* DOWN ARROW */
			    if (++edit > 17) edit = 1;	// pins past 17 are gnd
			    redraw = 1;
			    break;
		    }
		    break;
	    }
	}
    }
    nosound();	// TurboC: stop sound
    printf("\033[25H");
    return 0;
}

