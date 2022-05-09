/*
	Information retrieved from:
	https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
	https://github.com/charmedlabs/pixy2/blob/55ca01bc8d205f44277f02e706c8413face9e44a/src/host/arduino/libraries/Pixy2/TPixy2.h
*/

#include "fdserial.h"

// --- Configs --- //

// These are flipped for some reason, don't ask me why.
// So rx is really PIN_TX, tx is PIN_RX.
// This randomly fixed the robot for me after it stopped for a while. Weird.
#define PIN_RX 6
#define PIN_TX 10

// Debug prints
#define DEBUG 0

// --- Configs --- //
#define PIXY_CHECKSUM_SYNC									 0xc1af
#define PIXY_NO_CHECKSUM_SYNC								0xc1ae

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

// Pixy2 Request Types
// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide#sendpacketrequests-sent-to-pixy2
#define REQ_CHANGE_PROG 0x02
#define REQ_RESOLUTION 12
#define REQ_VERSION 14
#define REQ_BRIGHTNESS 16
#define REQ_LED 20
#define REQ_LAMP 22

// Addons
#define REQ_GET_FEATURES 0x30
#define REQ_VIDEO_RGB 0x70	

fdserial *channel;
bool m_cs; // Want to get rid of this

// ------ Function Declarations ------
#define println(fmt) print(fmt "\n")
#define printlnf(fmt, ...) print(fmt "\n", __VA_ARGS__)

// Incoming bytes count
#define ibytes() fdserial_rxCount(channel)

// I keep accidentally using this over pause().
#undef sleep

#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF

// Custom byte writing functions.
inline void pWriteU8( uint8_t byte ) {
	fdserial_txChar( channel, byte );
}

inline void pWriteU16( uint16_t longint ) {
	pWriteU8( longint & U8_MAX );
	pWriteU8( longint >> 8 );
}

// Not sure if this is correct
inline void pWriteU32( uint32_t theint	) {
	pWriteU16( theint & U16_MAX	);
	pWriteU16( theint >> 16 );
}

inline void pWriteI8( int8_t byte ) {
	if (byte < 0) byte += U8_MAX;
	pWriteU8(byte & U8_MAX);
}

inline void pWriteI16( int16_t n ) {
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
}

inline uint8_t pReadU8() {
	return fdserial_rxChar(channel);
}

inline uint16_t pReadU16() {
	uint8_t first = pReadU8();
	uint8_t second = pReadU8();
	return second << 8 | first; // Use proper endianness
	// return pReadU8() << 8 | pReadU8();
}

// Not sure if this is correct
inline uint16_t pReadU32() {
	return pReadU16() << 16 | pReadU16();
}

PacketRx* pGetPacket() {
	PacketRx* packet;

	uint16_t sync = pReadU16();
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
	while (true) {
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
