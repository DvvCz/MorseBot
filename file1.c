// File file1.c
// file3.c but using Pixy as a backend, instead of color sensor
#include "file4.c"

uint8_t R, G, B;

#define LOADCOL_OK PIXY_RESULT_OK
#define LOADCOL_ERROR PIXY_RESULT_ERROR
#define LOADCOL_BUSY PIXY_RESULT_BUSY

inline int loadColors() {
	vGetRGB( ROBOT_WIDTH / 2, ROBOT_HEIGHT / 2, &R, &G, &B, true );
}

inline uint8_t getRed() {
	return R;
}

inline uint8_t getGreen() {
	return G;
}

inline uint8_t getBlue() {
	return B;
}
