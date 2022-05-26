/*
	Configs Start
*/
#define PIN_AUDIO_L 26
#define PIN_AUDIO_R 27

// These are flipped for some reason :/
// rx is PIN_TX, tx is PIN_RX. If the robot randomly stops working try swapping these.
#define PIN_RX 6
#define PIN_TX 10

// Try to sync these with the website
#define CHAR_LEN 100
#define LONG_LEN 800
#define WORD_LEN 2700 // ms
#define DOWNTIME_LEN 50

#define SCAN_TIME 10000 // max time to scan for morse code for in milliseconds (40000 = 40 seconds)
#define END_LEN WORD_LEN * 2

// Might need to be different?
#define PAUSE_TIME 50

// Baud rate between the robot and the pixy camera through UART. (Default 19200)
// Apparently the max is 230 kbaud (230,000 ??) https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
#define PIXY_BAUD 19200

/*
	Configs End
*/

// These are the only dependencies besides fdserial.h (Need to keep the binary fairly small for the robot.)
#include <assert.h>
#include "simpletools.h"
#include "text2speech.h"
#include "stdbool.h"

int ROBOT_WIDTH, ROBOT_HEIGHT;

#include "file4.c"
#include "file5.c"

inline void say(char* txt) {
	print("<tts>%s</tts>", txt);
}

// Returns time elapsed since certain timestamp in ms.
int elapsed(int cnt) {
	return (CNT - cnt) / st_pauseTicks;
}

/*
-off 0 10 0
-on 0 645
-off 1 80 0
-on 1 403
-off 2 80 0
-on 2 241
-off 3 80 0
-on 3 967
-off 4 80 0
-on 4 967
-off 5 80 0
-on 5 1128
-off 6 161 0
-on 6 403
-off 7 80 0
-on 7 403
-off 8 80 0
-on 8 403
-off 9 80 0
Buf: [...---...!]

vs
-off 0 10 0
-on 0 141
-off 1 211 0
-on 1 70
-off 2 70 0
-on 2 706
-off 3 141 0
-on 3 141
-off 4 70 0
-on 4 1977
-off 5 70 0
-on 5 1271
-off 6 70 0
-on 6 494
-off 7 70 0
-on 7 353
-off 8 70 0
-on 8 423
....--..Got word ....--...!

*/

int main() {
	channel = fdserial_open(PIN_RX, PIN_TX, 0, PIXY_BAUD);
	assert(channel != NULL && "Failed to create channel");

	uint8_t r, g, b;

	// Wait until we see green. That is the signal to start scanning for morse code. (and to stop)
	while (sanitycheck(2) && (r > 60 || g < 230 || b > 60)) {
		if ( vGetRGB( ROBOT_WIDTH / 2, ROBOT_HEIGHT / 2, &r, &g, &b, true ) != PIXY_RESULT_OK ) break;
		pause(PAUSE_TIME);
	}

	int before = CNT;
	int light_on = false;
	int last = CNT; // Time current status was achieved.
	int status_for = 0;

	char buf[1000];
	int pos;

	ml:
	buf[1000] = "";
	pos = 0;

	print("<tts>Started</tts>\n");

	while ( true ) {
		getStr(buf, 128); // Load buffer into memory
		if (!sanitycheck()) break;

		if ( vGetRGB( ROBOT_WIDTH / 2, ROBOT_HEIGHT / 2, &r, &g, &b, true ) != PIXY_RESULT_OK ) break;

		if ( r >= 180 && g >= 180 && b >= 180 ) {
			if (!light_on) {
				if ( elapsed(last) >= WORD_LEN ) {
					buf[pos] = '/';
					pos++;
				} else if ( elapsed(last) >= 300 ) {
					// Seems to take 121 ms for each downtime space.
					buf[pos] = ' ';
					pos++;
				}

				last = CNT;
				light_on = true;
			}
		} else if ( r < 90 && g > 200 && b < 90 ) {
			// Green Light. END of Morse code stream.
			buf[pos++] = '!';
			break;
		} else { // if ( r <= 90 && g <= 90 && b <= 90 ) {
			if (light_on) {
				if (elapsed(last) >= LONG_LEN) {
					buf[pos] = '-';
				} else {
					buf[pos] = '.';
				}
				pos++;

				last = CNT;
				light_on = false; 
			}
		}

		pause(PAUSE_TIME);
	}

	fwrite(buf, 1, pos, stdout);

	char out[] = "";
	if ( translateMorse(buf, out) == MORSE_OK ) {
		// Morse code was translated successfully.
		print("Success: [%s]<tts>%s</tts>\n", out, out);
		pSetLED(0, 255, 0);

		goto ml;
	} else {
		print("<tts>Failed!</tts>\n");
		pSetLED(255, 0, 0);
		
		goto ml;
	}
}
