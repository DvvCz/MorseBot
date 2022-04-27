#include "simpletools.h"
#include "text2speech.h"
#include "stdbool.h"

#define PIN_S0 0
#define PIN_S1 1
#define PIN_S2 2
#define PIN_S3 3
#define PIN_OUT 4

#define PIN_AUDIO 27

#include "file3.c"

talk *TTS;

void initTTS() {
	// L, R pins.
	// Left as -1 for mono audio to pin 27.
	TTS = talk_run(-1, PIN_AUDIO);
	talk_set_speaker(TTS, 1, 100);
}

inline void setVolume(int vol) {
	talk_setVolume(TTS, 0);
}

inline void spell(const char* txt) {
	talk_spell(TTS, txt);
}

inline void say(const char* txt) {
	talk_say(TTS, txt);
}

int main() {
	initTTS();
	setVolume(100);

	/*
		4/26/2022
		minr = 1, ming = 2, minb = 0 maxr = 221, maxg = 49, maxb = 40
	*/


	int pos = 0;
	char* buf = malloc(100);

	int start = CNT;
	while (true) {
		print("%d\n", CNT);
		if (getRed() > 200) {
			buf[pos] = '.';
		} else if (CNT - start > 20) {
			buf[pos] = '\0';
			print(" end [%s]", buf);
			break;
		} else {
			buf[pos] = ' ';
		}
		pos++;
	}
	//calibrate();


	/*
	spell("Hello");
	say("Hello");
	*/
}
