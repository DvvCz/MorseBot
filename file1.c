// File file1.c
// file3.c but using Pixy as a backend, instead of color sensor
#include "file0.c"

int R, G, B;

inline void loadColors() {
	vGetRGB( ROBOT_WIDTH / 2, ROBOT_HEIGHT / 2, &R, &G, &B, true );
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
