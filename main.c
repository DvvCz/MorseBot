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
#define DOWNTIME_LEN 150

#define SCAN_TIME 15000 // How long to look for morse code for in milliseconds (15000 = 15 seconds.)

// Might need to be different?
#define PAUSE_TIME DOWNTIME_LEN

// R, G, B (255 - 0)
#define COLOR_OFF() pSetLED(0, 0, 255); // blue (detected light off)
#define COLOR_ON() pSetLED(255, 0, 255); // purple (detected light)
#define COLOR_START() pSetLED(0, 255, 0); // green (started scanning for morse code)
#define COLOR_FAIL() pSetLED(255, 255, 0); // yellow (failed to get color result from pixy camera)

// Baud rate between the robot and the pixy camera through UART. (Default 19200)
// Apparently the max is 230 kbaud (230,000 ??) https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
#define PIXY_BAUD 20000

/*
	Configs End
*/

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

	/*
		TTS TESTING AREA START
		You can see all of the different combinations of things you can do here
		https://github.com/parallaxinc/Simple-Libraries/blob/master/Learn/Simple%20Libraries/Audio/libtext2speech/text2speech.h#L170-L248
		For what you give to the "say" function
	*/

	initTTS();
	setVolume(500);
	say("Test Test Test Test Quiet now (Test Test) <<<<<<<<<<<<<<LOUD>>>>>>>>>>>>");
	spell("abcdefghijklmnopqrstuvwxyz");

	while (1) {} // Remove this line if you want it to not stop the robot after talking.

	/*
		TTS TESTING AREA END
	*/

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
	// Pixy2 Demo Code
	println("Pixy2 Camera Demo");

	// Track white lines on dark background, not the opposite. 
	//int8_t moderes = lSetMode( LINE_MODE_WHITE_LINE );

	pGetResolution(&ROBOT_WIDTH, &ROBOT_HEIGHT);
	printlnf("Resolution: %dx%d", ROBOT_WIDTH, ROBOT_HEIGHT);

	// How long the light has been either off or on.
	bool is_light = false;
	int status_for = 0;

	char buf[1000] = "";
	uint16_t pos = 0;

	int before = CNT;
	int last_status = CNT;

	COLOR_START();
	while ( elapsed(before) < SCAN_TIME ) {
		if ( loadColors() != LOADCOL_OK ) {
			COLOR_FAIL();

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
					buf[pos++] = ' ';
					buf[pos++] = '/';
					buf[pos++] = ' ';
				} else if (status_for > CHAR_LEN) {
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
		} else if (is_light) {
			// Light OFF (Was ON last time)
			if (status_for > LONG_LEN) {
				buf[pos++] = '-';
			} else {
				buf[pos++] = '.';
			}

			last_status = CNT;
			COLOR_OFF();
			is_light = false;
		}

		pause(PAUSE_TIME);
	}
	buf[pos] = '\0';

	print("Buf: %s", buf);

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
