// File file3.c
// color sensor stuff

#include "simpletools.h"
#include "serial.h"
#include "stdbool.h"
#include "servo.h"
#include "ping.h"

#define PIN_S0 0
#define PIN_S1 1
#define PIN_S2 2
#define PIN_S3 3
#define PIN_OUT 4

/// Combinations:
/// LOW	LOW	Red
/// LOW	HIGH Blue
/// HIGH LOW Clear (No filter)
/// HIGH HIGH Green

#define PULSE_LOW 0
#define PULSE_HIGH 1

inline void loadColors() {}

inline int getRedPw() {
	// LOW, LOW (RED)
	low(PIN_S2);
	low(PIN_S3);

	return pulse_in(PIN_OUT, PULSE_LOW);
}

inline int getBluePw() {
	// LOW, HIGH (BLUE)
	low(PIN_S2);
	high(PIN_S3);

	return pulse_in(PIN_OUT, PULSE_LOW);
}

inline int getGreenPw() {
	// HIGH, HIGH (GREEN)
	high(PIN_S2);
	high(PIN_S3);

	return pulse_in(PIN_OUT, PULSE_LOW);
}

// Values calibrated 3/28/22
// mr 61 mg 95 mb 80 minr 6 minb 4 ming 1
int redMin = 1;
int redMax = 44;
int greenMin = 2;
int greenMax = 176;
int blueMin = 2;
int blueMax = 178;

// minr = 1, ming = 2, minb = 2, maxr = 44, maxg = 176, maxb = 178

int map(int value, int in_min, int in_max, int out_min, int out_max) {
	return out_min + (out_max - out_min) * (value - in_min) / (in_max - in_min); // (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#define COLSENSOR_STABILIZE 50

// low2 + (high2 - low2) * (value - low1) / (high1 - low1);

int getRed() {
	int r = map( getRedPw(), redMin, redMax, 255, 0);
	pause(COLSENSOR_STABILIZE); // Stabilize sensor
	return r;
}

int getGreen() {
	int g = map( getGreenPw(), greenMin, greenMax, 255, 0);
	pause(COLSENSOR_STABILIZE); // Stabilize sensor
	return g;
}

int getBlue() {
	int b = map( getBluePw(), blueMin, blueMax, 255, 0);
	pause(COLSENSOR_STABILIZE); // Stabilize sensor
	return b;
}

int r, g, b;

// Calibrate by repeatedly looping to get rgb values, recording min and max.
void calibrate() {
	int max_red = 0;
	int min_red = 10000;

	int max_green = 0;
	int min_green = 10000;

	int max_blue = 0;
	int min_blue = 10000;

	while (true) {
		int r = getRedPw();
		if (r > max_red && r < 1000) {
			max_red = r;
			print("Max [R] = %d\n", r);
		} else if (r < min_red) {
			min_red = r;
			print("Min [R] = %d\n", r);
        }

		pause(200);

		int g = getGreenPw();
		if (g > max_green && g < 1000) {
			max_green = g;
			print("Max [G] = %d\n", g);
        } else if (g < min_green) {
			min_green = g;
			print("Min [G] = %d\n", g);
		}

		pause(200);

		int b = getBluePw();
		if (b > max_blue && b < 1000) {
			max_blue = b;
			print("Max [B] = %d\n", b);
        } else if (b < min_blue) {
			min_blue = b;
			print("Min [B] = %d\n", b);
		}

		pause(200);
	}
}
