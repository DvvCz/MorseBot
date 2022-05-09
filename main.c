#include "simpletools.h"
#include "text2speech.h"
#include "stdbool.h"

/* Configs */
#define PIN_AUDIO 27
#define BACKEND_PIXY

int ROBOT_WIDTH, ROBOT_HEIGHT;

#include "file1.c"

// Colorsensor backend
// #// include "file3.c"

// Pixy backend
// #//include "file1.c"
//#//include "file2.c"

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

char* script = " \
According to all known laws\
of aviation,\
there is no way a bee\
should be able to fly.\
Its wings are too small to get\
its fat little body off the ground.\
The bee, of course, flies anyway\
because bees don't care\
what humans think is impossible.\
Yellow, black. Yellow, black.\
Yellow, black. Yellow, black.\
";

int main() {
	channel = fdserial_open(PIN_RX, PIN_TX, 0, 19200);
	assert(channel != NULL && "Failed to create channel");

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide

	// Pixy2 Demo Code
	println("Pixy2 Camera Demo");

	// Track white lines on dark background, not the opposite. 
	//int8_t moderes = lSetMode( LINE_MODE_WHITE_LINE );

	PixyVersion* ver = pGetVersion();
	assert(ver && "Failed to get pixy version");

	printlnf("Version: %d.%d.%d", ver->major, ver->minor, ver->build);
	printlnf("Hardware: %d", ver->hardware);

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

	#define WORD_LEN 500 // ms
	#define LONG_LEN 300
	#define CHAR_LEN 200

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

				pSetLED(255, 0, 0);
			}
		} else if (is_light) {
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

		pause(200);
	}

	buf[pos] = '\0';
	print("Buf: %s", buf);
}

/*int main() {
	initTTS();
	setVolume(100);

	PixyVersion* ver = pGetVersion();
	assert(ver && "Failed to get pixy version");

	print("Version: %d.%d.%d\n", ver->major, ver->minor, ver->build);
	print("Hardware: %d\n", ver->hardware);

	int width = 0, height = 0;
	pGetResolution(&width, &height);
	print("Resolution: %dx%d\n", width, height);

	int pos = 0;
	char buf[1000] = "";

	int start = CNT;

	int ticks_off = 0;
	int ticks_on = 0;

	while (true) {
		loadColors();

		pSetLED( getRed(), getGreen(), getBlue() );
		pause(500);
	}

	while (true) {
		loadColors();
		// print("%d\n", getRed());
		// continue;

		if (getRed() > 200) {
			// Light on char
			ticks_off = 0;
			ticks_on++;

			if (ticks_on > 3) {
				buf[pos] = '-';
			} else {
				buf[pos] = '.';
			}
		} else {
			ticks_on = 0;
			ticks_off++;

			print("Til off: %d\n", 10 - ticks_off);

			if (ticks_off > 10) {
				// Light not responding
				buf[pos] = '\0';
				print(" end [%.*s]", (int)sizeof buf, buf);
				break;
			} else if (ticks_off > 3) {
				// Light off in between word
				buf[pos] = '/';
				ticks_off = 0;
			} else {
				// Light off single time
				buf[pos] = ' ';
			}
		}
		// Every 50ms check again.
		pause(50);
		pos++;
	}
}*/
