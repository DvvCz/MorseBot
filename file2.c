// File file2.c
// Dummy version of file1 and file3.
// Simulates if morse code was playing.

int R, G, B;

#define SLEEP_SMALL 100
#define SLEEP_LONG SLEEP_SMALL * 3
#define SLEEP_WORD SLEEP_LONG * 3

char* morse = ".-.- / .-.-";
int length = 11;

void simulateMorse(char* morse, int length) {
	for (int i = 0; i < length; i++) {
		char c = morse[i];
		switch (c) {
			case '.':
				R = 200;
				pause(SLEEP_SMALL);
				R = 0;
			break;

			case '-':
				R = 200;
				pause(SLEEP_LONG);
				R = 0;
			break;

			case ' ':
			break;

			case '/':
				R = 0;
				pause(SLEEP_WORD);
			break;

			default:
			break;
		}
		print("%c", c);
	}
}

inline int getRed() {
	return R;
}

inline int getGreen() {
	return G;
}

inline int getBlue() {
	return B;
}
