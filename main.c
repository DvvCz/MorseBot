#include "simpletools.h"
#include "text2speech.h"
#include "stdbool.h"

/* Configs */
#define PIN_AUDIO 27
#define BACKEND_PIXY

// Colorsensor backend
// #// include "file3.c"

// Pixy backend
// #//include "file1.c"
#include "file2.c"

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

inline void spell(const char* txt) {
	talk_spell(TTS, txt);
}

inline void say(const char* txt) {
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
	initTTS();
	setVolume(100);

	// pixyInit();
	cog_run(simulateMorse, 128);


	/*
		4/26/2022
		minr = 1, ming = 2, minb = 0 maxr = 221, maxg = 49, maxb = 40
	*/

	int pos = 0;
	// char* buf = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	char buf[1000] = "";

	int start = CNT;

	int ticks_off = 0;
	int ticks_on = 0;

	/*while (true) {
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
	}*/
}
