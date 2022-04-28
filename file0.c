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

#define PIN_LEFT_WHEEL 13
#define PIN_RIGHT_WHEEL 12

// Debug prints
#define DEBUG 0

// Whether to hard error on anything the library is stumped on.
// Has a lot of false alarms right now so don't turn this on.
#define ASSERTIONS 0

// --- Configs --- //

#define LINE_MAX_INTERSECTION_LINES 6

#define LINE_GET_MAIN_FEATURES									 0x00
#define LINE_GET_ALL_FEATURES										0x01

#define LINE_REQUEST_GET_FEATURES								0x30
#define LINE_RESPONSE_GET_FEATURES							 0x31
#define LINE_REQUEST_SET_MODE										0x36
#define LINE_REQUEST_SET_VECTOR									0x38
#define LINE_REQUEST_SET_NEXT_TURN_ANGLE				 0x3a
#define LINE_REQUEST_SET_DEFAULT_TURN_ANGLE			0x3c
#define LINE_REQUEST_REVERSE_VECTOR							0x3e

#define LINE_MODE_TURN_DELAYED									 0x01
#define LINE_MODE_MANUAL_SELECT_VECTOR					 0x02
#define LINE_MODE_WHITE_LINE										 0x80

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

// compare packet checksum to calcutated checksum
int32_t validateChecksum(PacketRx* packet) {
	uint16_t cs0 = packet->checksum;
	uint16_t cs1 = 0;

	uint8_t i = packet->length;
	uint8_t j = 0;
	while (i-- > 0) {
		cs1 += packet->buf[j++];
	}

	return (cs1 == cs0) ? PIXY_RESULT_OK : PIXY_RESULT_ERROR;
}

// Receive a stream of bytes
// cs is NULL by default
int16_t pRecv(uint8_t* buf, uint8_t len, uint16_t* cs) {
	uint8_t i, j;
	int16_t c;

	if (cs != NULL) {
		*cs = 0;
	}

	for (i=0; i<len; i++) {
		// wait for byte, timeout after 2ms
		// note for a baudrate of 19.2K, each byte takes about 500us
		for (j=0; true; j++) {
			if (j==200)
				return -1;
			c = fdserial_rxCheck(channel); // Try reading U8, It returns -1 if couldn't read.
			if (c>=0)
				break;
			// Delay by 10 microseconds (us)
			usleep(10);
		}
		buf[i] = c; 

		if (cs) {
			*cs += buf[i];
		}
	}

	return len;
}

int16_t pGetSync() {
	uint8_t i, j, c, cprev;
	int16_t res;
	uint16_t start;
	
	// parse bytes until we find sync
	for(i=j=0, cprev=0; true; i++) {
		res = pRecv(&c, 1, NULL);
		if (res >= PIXY_RESULT_OK) {
			// since we're using little endian, previous byte is least significant byte
			start = cprev;
			// current byte is most significant byte
			start |= c << 8;
			cprev = c;
			if (start == PIXY_CHECKSUM_SYNC) {
				m_cs = true;
				return PIXY_RESULT_OK;
			}

			if (start == PIXY_NO_CHECKSUM_SYNC) {
				m_cs = false;
				return PIXY_RESULT_OK;
			}
		}

		// If we've read some bytes and no sync, then wait and try again.
		// And do that several more times before we give up.	
		// Pixy guarantees to respond within 100us.
		if (i >= 4) {
			if (j >= 4) {
				#if DEBUG
					println("error: no response")
				#endif
				return PIXY_RESULT_ERROR;
			}

			usleep(25);
			j++;
			i = 0;
		}
	}
}

// Better function I guess
uint8_t pGetPacket2(PacketRx* packet) {
	uint16_t csCalc, csSerial;
	uint8_t res;

	res = pGetSync();
	if (res < 0) {
		return res;
	}

	if (m_cs) {
		res = pRecv(packet->buf, 4, NULL);

		if(res < 0) {
			return res;
		}

		packet->type = packet->buf[0];
		packet->length = packet->buf[1];

		csSerial = *(uint16_t *)&packet->buf[2];

		// Overwrite buffer to contain data after the header.
		res = pRecv(packet->buf, packet->length, &csCalc);
		if (res < 0) {
			return res;
		}

		if (csSerial != csCalc) {
			return PIXY_RESULT_CHECKSUM_ERROR;
		}
	} else {
		res = pRecv(packet->buf, 2, NULL);
		if (res < 0) {
			return res;
		}

		packet->type = packet->buf[0];
		packet->length = packet->buf[1];

		// Overwrite buffer to contain data after the header.
		res = pRecv(packet->buf, packet->length, NULL);
		if (res < 0) {
			return res;
		}
	}
	return PIXY_RESULT_OK;
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
	uint16_t signature;
	uint16_t x;
	uint16_t y;
	uint16_t width;
	uint16_t height;
	int16_t angle;
	uint8_t index;
	uint8_t age;
} Block;

// ------ Functions ------
#define  CCC_SIG1         0b00000001
#define  CCC_SIG2         0b00000010
#define  CCC_SIG3         0b00000100
#define  CCC_SIG4         0b00001000
#define  CCC_SIG5         0b00010000
#define  CCC_SIG6         0b00100000
#define  CCC_SIG7         0b01000000
#define  CCC_ALL          0b01111111
#define  CCC_COLOR_CODES  0b10000000
#define  CCC_MAX_SIGNATURE 7

#define  SIG_MAP  CCC_SIG1 | CCC_SIG2 | CCC_SIG3                // monitor Signatures 1, 2, and 3
#define  CC_MAP   CCC_COLOR_CODES | SIG_MAP                     // monitor color codes for CC sigs 1, 2, and 3

void printBlock(Block* block) {
	int i, j;
	char buf[128], sig[6], d;
	bool flag;	
	if (block->signature > CCC_MAX_SIGNATURE) {
		// convert signature number to an octal string
		for (i=12, j=0, flag=false; i>=0; i-=3) {
			d = (block->signature >> i) & 0x07;

			if (d>0 && !flag) flag = true;
			if (flag) sig[j++] = d + '0';
		}
		sig[j] = '\0';	
		sprintf(buf, "CC block sig: %s (%d decimal) x: %d y: %d width: %d height: %d angle: %d index: %d age: %d", sig, block->signature, block->x, block->y, block->width, block->height, block->angle, block->index, block->age);
	} else { // regular block.	Note, angle is always zero, so no need to print
		sprintf(buf, "sig: %d x: %d y: %d width: %d height: %d index: %d age: %d", block->signature, block->x, block->y, block->width, block->height, block->index, block->age);
	}

	printlnf("%s", buf); 
}

uint8_t pGetBlocks(uint8_t sigmap, uint8_t max_blocks, bool wait, Block* blocks, uint8_t* numBlocks) {
	blocks = NULL;
	numBlocks = 0;

	pWriteHeader(32, 2);
	pWriteU8(sigmap);
	pWriteU8(max_blocks);

	PacketRx* packet = pGetPacket();

	if (validateChecksum(packet) != PIXY_RESULT_OK) {
		return PIXY_RESULT_CHECKSUM_ERROR;
	} else {
		if (packet->type == RES_BLOCKS) {
			blocks = (Block*)packet->buf;
			*numBlocks = packet->length / sizeof(Block);
			return PIXY_RESULT_OK;
		} else {
			return (int32_t)(0xFFFFFF00 | packet->type);
		}
	}
	/*else if (packet->type == RES_ERROR) {
		if( (int8_t)packet->buf[0] == PIXY_RESULT_BUSY ) {
			if (!wait) {
				return PIXY_RESULT_BUSY;
			}
		} else if ( (int8_t)packet->buf[0] != PIXY_RESULT_PROG_CHANGING) {
			return packet->buf[0];
		}
	} else {
		printlnf("pRequestBlocks got incorrect packet type [%u], expected %u or %u", packet->type, RES_BLOCKS, RES_ERROR);
		return PIXY_RESULT_ERROR;
	}*/
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
	LINE TRACKING API
*/

typedef struct {
	uint8_t x0;
	uint8_t y0;
	uint8_t x1;
	uint8_t y1;
	uint8_t index;
	uint8_t flags;
} Vector;

typedef struct {
	uint8_t index;
	uint8_t reserved;
	int16_t angle;
} IntersectionLine;

typedef struct {
	uint8_t x;
	uint8_t y;
	
	uint8_t n;
	uint8_t reserved;
	IntersectionLine intLines[LINE_MAX_INTERSECTION_LINES];
} Intersection;

typedef struct {
	uint8_t x;
	uint8_t y;
	uint8_t flags;
	uint8_t code;
} Barcode;

Vector* vectors;
int numVectors;
Intersection* intersections;
int numIntersections;
Barcode* barcodes;
int numBarcodes;


// 0 - 1	16-bit sync	175, 193 (0xc1af)
// 2	Type of packet	49
// 3	Length of payload	(varies)
// 4 - 5	16-bit checksum	sum of payload bytes
// 6 	Feature-type	1=Vector, 2=Intersection, 4=Barcode
// 7	Feature-length	(varies)
// 8 - 8+feature-length	Feature-data	(varies)
int lGetFeatures(uint8_t feature_type, uint8_t features, bool wait) {
	int8_t res;
	uint8_t offset, fsize, ftype, *fdata;
	while(true) {
		pWriteHeader(REQ_GET_FEATURES, 2);
		pWriteU8(feature_type); // LINE_GET_MAIN_FEATURES for example
		pWriteU8(features);

		PacketRx* packet = pGetPacket();

		if (packet->type == LINE_RESPONSE_GET_FEATURES) {
			for (offset=0, res=0; packet->length > offset; offset += fsize + 2) {
				ftype = packet->buf[offset + 0];
				fsize = packet->buf[offset + 1];
				fdata = packet->buf[offset + 2];

				if (ftype == TLINE_VECTOR) {
					vectors = (Vector*)fdata;
					numVectors = fsize / sizeof(Vector);
					res |= TLINE_VECTOR;
				} else if (ftype == TLINE_INTERSECTION) {
					intersections = (Intersection*)fdata;
					numIntersections = fsize / sizeof(Intersection);
					res |= TLINE_INTERSECTION;
				} else if (ftype == TLINE_BARCODE) {
					barcodes = (Barcode*)fdata;
					numBarcodes = fsize / sizeof(Barcode);
					res |= TLINE_BARCODE;
				} else {
					printlnf("Invalid line type in lGetFeatures. Got [%u]", ftype);
					break;
				}
				return res;
			}
		} else if(packet->type == RES_ERROR) {
			// if it's not a busy response, return the error
			if ( (int8_t)packet->buf[0] != PIXY_RESULT_BUSY ) {
				return packet->buf[0];
			}
			else if (!wait) {
				// Busy, data not available.
				// User doesn't want to wait, so exit.
				return PIXY_RESULT_BUSY;
			}
		} else {
			return PIXY_RESULT_ERROR;
		}

		// Wait for pixy to respond
		pause(5);
	}
}

int8_t lSetMode(uint8_t mode) {
	pWriteHeader(LINE_REQUEST_SET_MODE, 1);
	pWriteU8(mode);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("lSetMode got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("lSetMode got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

	uint32_t res = *(uint32_t *)(packet->buf);
	return (int8_t)(res);
}

int lSetNextTurn(int16_t angle) {
	pWriteHeader(58, 2);
	pWriteI16(angle);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("lSetNextTurn got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("lSetNextTurn got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

	return PIXY_RESULT_OK;
}

int lSetDefaultTurn(int16_t angle) {
	pWriteHeader(60, 2);
	pWriteI16(angle);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("lSetDefaultTurn got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("lSetDefaultTurn got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

	return PIXY_RESULT_OK;
}

int lSetVector(uint8_t index) {
	pWriteHeader(56, 1);
	pWriteU8(index);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("lSetVector got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("lSetVector got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

	return PIXY_RESULT_OK;
}

int lReverseVector(uint8_t index) {
	pWriteHeader(62, 0);

	PacketRx* packet = pGetPacket();

	if (packet->type != RES_RESULT) {
		printlnf("lReverseVector got incorrect packet type [%u], expected %u", packet->type, RES_RESULT);
		return PIXY_RESULT_ERROR;
	}

	if (packet->length != 4) {
		printlnf("lReverseVector got incorrect packet length [%u], expected %u", packet->length, 4);
		return PIXY_RESULT_ERROR;
	}

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
			println("Waiting for pixy..");
			pause(5);
			continue;
		}

		return PIXY_RESULT_ERROR;
	}
}

/*
	Robot Movement
	https://github.com/parallaxinc/Simple-Libraries/blob/1cc167c103756e5772b533c833a0f34e6050f459/Learn/Simple%20Libraries/Motor/libservo/servo.h
*/

void rStop() {
	servo_disable(PIN_LEFT_WHEEL);
	servo_disable(PIN_RIGHT_WHEEL);
}

// Turns left 90* with a rotation turn (not pivoting)
void rTurnLeft() {
	servo_speed(PIN_LEFT_WHEEL, -100);
	servo_speed(PIN_RIGHT_WHEEL, -100);
	pause(1000);
	rStop();
}

// Turns right 90* with a rotation turn (not pivoting)
void rTurnRight() {
	servo_speed(PIN_LEFT_WHEEL, 100);
	servo_speed(PIN_RIGHT_WHEEL, 100);
	pause(1000);
	rStop();
}

void rForward() {
	servo_speed(PIN_LEFT_WHEEL, 100);
	servo_speed(PIN_RIGHT_WHEEL, -100);
}

void rBackward() {
	servo_speed(PIN_LEFT_WHEEL, -100);
	servo_speed(PIN_RIGHT_WHEEL, 100);
}


/*
	Main Loop
*/

int ROBOT_WIDTH = 0;
int ROBOT_HEIGHT = 0;

int pixyInit() {
	channel = fdserial_open(PIN_RX, PIN_TX, 0, 19200);
	assert(channel != NULL && "Failed to create channel");

	// https://docs.pixycam.com/wiki/doku.php?id=wiki:v2:porting_guide
	PixyVersion* ver = pGetVersion();
	assert(ver && "Failed to get pixy version");

	pGetResolution(&ROBOT_WIDTH, &ROBOT_HEIGHT);
}
