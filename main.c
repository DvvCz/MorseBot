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
#define CHAR_LEN 300
#define LONG_LEN 900
#define WORD_LEN 2700 // ms
#define DOWNTIME_LEN 50

#define SCAN_TIME 40000 // max time to scan for morse code for in milliseconds (40000 = 40 seconds)
#define END_LEN WORD_LEN * 3

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

#include "file1.c"
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

int main() {
	channel = fdserial_open(PIN_RX, PIN_TX, 0, PIXY_BAUD);
	assert(channel != NULL && "Failed to create channel");

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
	// Pixy2 Demo Code
	println("Pixy2 Camera Demo");

	pGetResolution(&ROBOT_WIDTH, &ROBOT_HEIGHT);
	printlnf("Resolution: %dx%d", ROBOT_WIDTH, ROBOT_HEIGHT);

	// How long the light has been either off or on.
	bool is_light = false;
	int status_for = 0;

	char buf[1000] = "";
	uint16_t pos = 0;

	uint8_t r;
	uint8_t g;
	uint8_t b;

	COLOR_WAIT();
	do {
		if ( loadColors() != LOADCOL_OK ) {
			pause(200);
			continue;
		}

		/*
			47 255 51
			50 255 50
			48 255 48
			48 255 48
		*/

		// Wait until we see green. That is the signal to start scanning for morse code.
		r = getRed();
		g = getGreen();
		b = getBlue();

		print("%d %d %d\n", r, g, b);
		pause(200);
	} while( r > 60 || g < 230 || b > 60 );

	COLOR_START();

	int before = CNT;
	int last_status = CNT;

	while ( elapsed(before) < SCAN_TIME ) {
		if ( loadColors() != LOADCOL_OK ) {
			COLOR_FAIL();

			pause(200);
			continue;
		};

		r = getRed();
		g = getGreen();
		b = getBlue();

		status_for = elapsed( last_status );

		if ( r >= 200 && g >= 200 && b >= 200 ) {
			// Light ON (Was OFF last time)
			if (!is_light) {
				if (status_for >= WORD_LEN) {
					buf[pos++] = ' ';
					buf[pos++] = '/';
					buf[pos++] = ' ';
				} else if (status_for >= CHAR_LEN) {
					buf[pos++] = ' ';
				} else {
					// Skip.
					// buf[pos] = '';
					// pos++;
				}

				last_status = CNT;
				is_light = true;
				status_for = 0;

				COLOR_ON();
			}
		} else { //if ( r <= 90 && g <= 90 && b <= 90 ) {
			// Light OFF
			if (is_light) {
				// Light was ON last time
				if (status_for >= LONG_LEN) {
					buf[pos++] = '-';
				} else {
					buf[pos++] = '.';
				}

				last_status = CNT;
				COLOR_OFF();
				is_light = false;
			} else {
				if ( status_for >= END_LEN ) break;

				printlnf("LOFF: %d %d", status_for, LONG_LEN);
			}
		} /*else {
			print("Neither %d %d %d", r, g, b);
		}*/

		pause(PAUSE_TIME);
	}
	buf[pos] = '\0';

	print("Buf: %s\n", buf);

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
