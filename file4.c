/*
	Pixy2Robot Software
	Autonomous driving car software by David Cruz

	Information retrieved from:
	https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
	https://github.com/charmedlabs/pixy2/blob/55ca01bc8d205f44277f02e706c8413face9e44a/src/host/arduino/libraries/Pixy2/TPixy2.h
*/


#include "simpletools.h"
#include "fdserial.h"
#include "stdint.h"
#include "stdbool.h"
#include <stdlib.h>
#include <assert.h>
#include "servo.h"

// --- Configs --- //

// These are flipped for some reason, don't ask me why.
// So rx is really PIN_TX, tx is PIN_RX.
// This randomly fixed the robot for me after it stopped for a while. Weird.
#define PIN_RX 6
#define PIN_TX 10

// Debug prints
#define DEBUG 0

// Whether to hard error on anything the library is stumped on.
// Has a lot of false alarms right now so don't turn this on.
#define ASSERTIONS 0

// --- Configs --- //
#define PIXY_CHECKSUM_SYNC									 0xc1af
#define PIXY_NO_CHECKSUM_SYNC								0xc1ae

#define TLINE_INVALID 	0x0
#define TLINE_VECTOR 	0x1
#define TLINE_INTERSECTION 	0x2
#define TLINE_BARCODE 	0x4

#define PIXY_RESULT_OK											 0
#define PIXY_RESULT_ERROR										-1
#define PIXY_RESULT_BUSY										 -2
#define PIXY_RESULT_CHECKSUM_ERROR					 -3
#define PIXY_RESULT_TIMEOUT									-4
#define PIXY_RESULT_BUTTON_OVERRIDE					-5
#define PIXY_RESULT_PROG_CHANGING						-6

// Pixy2 Response Types
#define RES_RESOLUTION 0x0d
#define RES_VERSION 0x0f
#define RES_RESULT 0x01
#define RES_ERROR 0x03
#define RES_BLOCKS 0x21

// Pixy2 Request Types
// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide#sendpacketrequests-sent-to-pixy2
#define REQ_CHANGE_PROG 0x02
#define REQ_RESOLUTION 12
#define REQ_VERSION 14
#define REQ_BRIGHTNESS 16
#define REQ_SERVO 18
#define REQ_LED 20
#define REQ_LAMP 22
#define REQ_FPS 24
// Addons
#define REQ_GET_FEATURES 0x30
#define REQ_VIDEO_RGB 0x70	
#define REQ_BLOCKS 0x20

fdserial *channel;
bool m_cs; // Want to get rid of this

// ------ Function Declarations ------
#define println(fmt) print(fmt "\n")
#define printlnf(fmt, ...) print(fmt "\n", __VA_ARGS__)
#define transmit(...) dprint(channel, __VA_ARGS__)
#define assertp(cond, ...) if (!cond) printlnf(__VA_ARGS__)

// Incoming bytes count
#define ibytes() fdserial_rxCount(channel)

// I keep accidentally using this over pause().
#undef sleep

#if !ASSERTIONS
	// Assertions disabled, just make it do nothing.
	#undef assert
	#define assert(x)
#endif

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFF

// Custom byte writing functions.
void pWriteU8( uint8_t byte ) {
	#if DEBUG
		printlnf("Writing u8: [%u]", byte);
	#endif
	fdserial_txChar( channel, byte );
}

void pWriteU16( uint16_t longint ) {
	#if DEBUG
		printlnf("Writing U16: %u => [%u, %u]", longint, longint & U8_MAX, longint >> 8);
	#endif
	pWriteU8( longint & U8_MAX );
	pWriteU8( longint >> 8 );
}

// Not sure if this is correct
void pWriteU32( uint32_t theint	) {
	pWriteU16( theint & U16_MAX	);
	pWriteU16( theint >> 16 );
}

void pWriteI8( int8_t byte ) {
	if (byte < 0) byte += U8_MAX;
	pWriteU8(byte & U8_MAX);
}

void pWriteI16( int16_t n ) {
	if (n < 0) n += U16_MAX;

	pWriteU8( n & 8 );
	pWriteU8( (n >> 8) & 8 );
}

// Packet to receive
typedef struct {
	/* 0-1 */ uint16_t sync; // PIXY_CHECKSUM_SYNC
	/* 2 */ uint8_t type;
	/* 3 */ uint8_t length;

	/* 4-5 */ uint16_t checksum;
	/* 6+ */ uint8_t* buf; // Buffer of the packet. Length varies per type of message.
} PacketRx;

// All functions prefixed with p for namespacing
void pWriteHeader(int8_t type, int8_t len) {
	// First two bytes are signature
	pWriteU16(PIXY_NO_CHECKSUM_SYNC);
	pWriteU8(type);
	pWriteU8(len);

	#if DEBUG
		println("Wrote header.");
	#endif
}

uint8_t pReadU8() {
	assert(ibytes() >= 1 && "Tried to read u8 but there was not enough bytes!");

	return fdserial_rxChar(channel);
}

uint16_t pReadU16() {
	assert(ibytes() >= 2 && "Tried to read u16 but there was not enough bytes!");

	uint8_t first = pReadU8();
	uint8_t second = pReadU8();
	return second << 8 | first; // Use proper endianness
	// return pReadU8() << 8 | pReadU8();
}

// Not sure if this is correct
uint16_t pReadU32() {
	assert(ibytes() >= 4 && "Tried to read u32 but there was not enough bytes!");
	return pReadU16() << 16 | pReadU16();
}

PacketRx* pGetPacket() {
	PacketRx* packet;

	#if DEBUG
		println("Getting packet");
	#endif

	uint16_t sync = pReadU16();

	#if DEBUG
		printlnf("Got sync %u", sync);
	#endif

	if (sync != 0xc1af) {
		printlnf("Received incorrect sync! Expected %u, but got %u. Split to: %u %u", 0xc1af, sync, sync & U8_MAX, sync >> 8);
		return NULL;
	}

	packet->sync = sync;
	packet->type = pReadU8();
	packet->length = pReadU8();
	packet->checksum = pReadU16();

	// 64 buffer slots just in case?
	packet->buf = malloc( sizeof(uint8_t) * 64 );

	for (int n = 0; n < packet->length; n++) {
		packet->buf[n] = pReadU8();
	}

	return packet;
}

int pSetLamps(int upper, int lower) {
	pWriteHeader(REQ_LAMP, 2);
	pWriteU8(upper);
	pWriteU8(lower);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("pSetLamps got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("pSetLamps got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

	return PIXY_RESULT_OK;
}

uint8_t pSetLED(uint8_t r, uint8_t g, uint8_t b) {
	pWriteHeader(REQ_LED, 3);
	pWriteU8(r);
	pWriteU8(g);
	pWriteU8(b);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("pSetLED got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("pSetLED got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

	return PIXY_RESULT_OK;
}

typedef struct {
	uint16_t hardware;
	uint8_t major;
	uint8_t minor;
	uint16_t build;
} PixyVersion;

PixyVersion* pGetVersion() {
	// Request version
	pWriteHeader(REQ_VERSION, 0);

	PacketRx* packet = pGetPacket();
	if ( packet->type != RES_VERSION ) {
		printlnf("pGetVersion got incorrect packet type [%u], expected %u", packet->type, RES_VERSION);
		return NULL;
	}

	if ( packet->length != 16 ) {
		printlnf("pGetVersion got incorrect packet length [%u], expected 16", packet->length);
		return NULL;
	}

	return (PixyVersion*)packet->buf;
}

int pGetResolution(int* width, int* height) {
	// Request
	pWriteHeader(REQ_RESOLUTION, 1);
		pWriteU8(0x0);

	PacketRx* packet = pGetPacket();

	if ( packet->type != RES_RESOLUTION ) {
		printlnf("pGetResolution got incorrect packet type [%u], expected %u", packet->type, RES_RESOLUTION);
		return PIXY_RESULT_ERROR;
	}

	// 4 bytes
	if ( packet->length != 4 ) {
		printlnf("pGetResolution got incorrect packet length [%u], expected 4", packet->length);
		//return PIXY_RESULT_ERROR;
	}

	*width = packet->buf[0] + packet->buf[1] * 256;
	*height = packet->buf[2] + packet->buf[3] * 256;

	return PIXY_RESULT_OK;
}

/*
	Video API
*/
int vGetRGB(uint16_t x, uint16_t y, uint8_t* r, uint8_t* g, uint8_t* b, bool saturate) {
	while(true) {
		pWriteHeader(REQ_VIDEO_RGB, 5);
		pWriteU16(x);
		pWriteU16(y);
		pWriteU8(saturate);

		PacketRx* packet = pGetPacket();

		if (packet->type == RES_RESULT) {
			if (packet->length != 4) {
				printlnf("vGetRGB got incorrect packet length [%u], expected %u", packet->length, 4);
				return PIXY_RESULT_ERROR;
			}
			*b = packet->buf[0];
			*g = packet->buf[1];
			*r = packet->buf[2];
			return PIXY_RESULT_OK;
		} else if (packet->type == RES_ERROR) {
			// Use first item from custom buffer. 0-3 is always reserved for packet header.
			// println("Waiting for pixy..");
			// pause(5);
			return PIXY_RESULT_BUSY;
		}

		return PIXY_RESULT_ERROR;
	}
}

/*
	Main Loop
*/

/*
int main() {
	channel = fdserial_open(PIN_RX, PIN_TX, 0, 19200);
	assert(channel != NULL && "Failed to create channel");

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide

	// Pixy2 Demo Code
	println("Pixy2 Camera Demo");

	// Random color lights
	while (true) {
		int r = rand() % 255;
		int g = rand() % 255;
		int b = rand() % 255;
		pSetLED(r, g, b);

		pause(500);
	}

	// Track white lines on dark background, not the opposite. 
	//int8_t moderes = lSetMode( LINE_MODE_WHITE_LINE );

	PixyVersion* ver = pGetVersion();
	assert(ver && "Failed to get pixy version");

	printlnf("Version: %d.%d.%d", ver->major, ver->minor, ver->build);
	printlnf("Hardware: %d", ver->hardware);

	int width = 0, height = 0;
	pGetResolution(&width, &height);
	printlnf("Resolution: %dx%d", width, height);
	
	pSetLED(255, 255, 255);
}
*/
