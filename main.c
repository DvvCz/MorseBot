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
#define DOWNTIME_LEN 100

#define SCAN_TIME 10000 // max time to scan for morse code for in milliseconds (40000 = 40 seconds)
#define END_LEN WORD_LEN * 2

// Might need to be different?
#define PAUSE_TIME DOWNTIME_LEN

// R, G, B (255 - 0)
#define COLOR_OFF() pSetLED(0, 0, 255); // blue (detected light off)
#define COLOR_ON() pSetLED(255, 0, 255); // purple (detected light)
#define COLOR_START() pSetLED(0, 255, 0); // green (started scanning for morse code)
#define COLOR_FAIL() pSetLED(255, 255, 0); // yellow (failed to get color result from pixy camera)
#define COLOR_WAIT() pSetLED(0, 0, 0); // no color (waiting for signal to start scanning)

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

talk *TTS;

void initTTS() {
	// L, R pins.
	// Left as -1 for mono audio to pin 27.
	TTS = talk_run(PIN_AUDIO_L, PIN_AUDIO_R);
	talk_set_speaker(TTS, 1, 100);
}

inline void setVolume(int vol) {
	talk_setVolume(TTS, vol);
}

inline void spell(char* txt) {
	talk_spell(TTS, txt);
}

inline void say(char* txt) {
	talk_say(TTS, txt);
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
*/

int main() {
	channel = fdserial_open(PIN_RX, PIN_TX, 0, PIXY_BAUD);
	assert(channel != NULL && "Failed to create channel");

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
	// Pixy2 Demo Code
	println("Pixy2 Camera Demo");

	pGetResolution(&ROBOT_WIDTH, &ROBOT_HEIGHT);
	printlnf("Resolution: %dx%d", ROBOT_WIDTH, ROBOT_HEIGHT);


	uint8_t r, g, b;
	COLOR_WAIT();

	// Wait until we see green. That is the signal to start scanning for morse code. (and to stop)
	do {
		if ( vGetRGB( ROBOT_WIDTH / 2, ROBOT_HEIGHT / 2, &r, &g, &b, true ) != PIXY_RESULT_OK ) break;
		pause(PAUSE_TIME);
	} while( r > 60 || g < 230 || b > 60 );

	COLOR_START();

	pause(1000); // Wait for initial green to change]

	COLOR_FAIL();

	int before = CNT;
	int light_on = false;
	int last = CNT; // Time current status was achieved.
	int status_for = 0;

	char buf[1000] = "";

	int pos = 0;

	while ( elapsed(before) < SCAN_TIME ) {
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

				print("-off %d %d %b", pos, elapsed(last), elapsed(last) >= LONG_LEN);

				last = CNT;
				light_on = true;
			}
		} else if ( r < 90 && g > 200 && b < 90 ) {
			// Green Light. END of Morse code stream.
			buf[pos++] = '!';
			break;
		} else { // if ( r <= 90 && g <= 90 && b <= 90 ) {
			if (light_on) {
				print("-on %d %d", pos, elapsed(last));
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
	buf[pos] = '\0';

	COLOR_START();

	printlnf("\nBuf: [%s]", buf);

	char out[] = "";
	if ( translateMorse(buf, out) == MORSE_OK ) {
		print("Success: [%s]\n", out);
		pSetLED(0, 255, 0);

		// Morse code was translated successfully.
		// Call "spell" tts on the output message.
		// Can also try "say"
		spell( out );
	} else {
		print("Failed!\n");
		pSetLED(255, 0, 0);
	}
}
