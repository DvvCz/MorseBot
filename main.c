#include <assert.h>
#include "simpletools.h"
#include "text2speech.h"
#include "stdbool.h"

/* Configs */
#define PIN_AUDIO 27

int ROBOT_WIDTH, ROBOT_HEIGHT;

#include "file1.c"
#include "file5.c"

talk *TTS;

void initTTS() {
	// L, R pins.
	// Left as -1 for mono audio to pin 27.
	TTS = talk_run(-1, PIN_AUDIO);
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
	channel = fdserial_open(PIN_RX, PIN_TX, 0, 19200);
	assert(channel != NULL && "Failed to create channel");

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide

	// Pixy2 Demo Code
	println("Pixy2 Camera Demo");

	// Track white lines on dark background, not the opposite. 
	//int8_t moderes = lSetMode( LINE_MODE_WHITE_LINE );

	pGetResolution(&ROBOT_WIDTH, &ROBOT_HEIGHT);
	printlnf("Resolution: %dx%d", ROBOT_WIDTH, ROBOT_HEIGHT);
	
	pSetLED(0, 255, 0);

	//initTTS();
	//setVolume(100);

	// How long the light has been either off or on.
	bool is_light = false;
	int status_for = 0;

	char buf[1000] = "";
	uint16_t pos = 0;

	#define CHAR_LEN 100
	#define LONG_LEN CHAR_LEN * 2
	#define WORD_LEN LONG_LEN * 2 // ms

	int before = CNT;
	int last_status = CNT;
	while ( elapsed(before) < 15000 ) {
		if ( loadColors() != LOADCOL_OK ) {
			pSetLED(0, 255, 0);

			pause(200);
			continue;
		};

		uint8_t r = getRed();
		uint8_t g = getGreen();
		uint8_t b = getBlue();

		status_for = elapsed( last_status );

		if ( (r + g + b) / 3 > 220 ) {
			// Light ON
			if (!is_light) {
				if (status_for > WORD_LEN) {
					buf[pos] = '/';
					pos++;
				} else if (status_for > CHAR_LEN) {
					buf[pos] = ' ';
					pos++;
				} else {
					// Skip.
					// buf[pos] = '';
					// pos++;
				}

				last_status = CNT;
				is_light = true;
				status_for = 0;

				pSetLED(255, 0, 255);
			}
		} else if (is_light) {
			// Light OFF (Was ON last time)
			if (status_for > LONG_LEN) {
				buf[pos] = '-';
				pos++;
			} else {
				buf[pos] = '.';
				pos++;
			}

			last_status = CNT;
			pSetLED(0, 0, 255);
			is_light = false;
		}

		pause(100);
	}
	buf[pos] = '\0';

	print("Buf: %s", buf);

	char out[] = "";
	if ( translateMorse(buf, out) == MORSE_OK ) {
		print("Success: [%s]\n", out);
		pSetLED(0, 255, 0);
	} else {
		print("Failed!\n");
		pSetLED(255, 0, 0);
	}
}
