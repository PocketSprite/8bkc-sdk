#include "io-pksp.h"
#include <stdint.h>


void ssd1331SetBrightness(int ctr) {
#if 0
	char sendCmd[]={0xAE, 0x81, 0, 0x82, 0, 0x83, 0,0xAF};
	sendCmd[1]=(0x91*ctr)>>8;
	sendCmd[3]=(0x50*ctr)>>8;
	sendCmd[5]=(0x7D*ctr)>>8;
#else
	char sendCmd[]={0x87, ctr/16};
#endif
	ioOledSend(sendCmd, sizeof(sendCmd), 0);
}

void ssd1331SendFB(const uint16_t *fb, int xs, int ys, int w, int h) {
	//cmd, start col, end col, cmd, start row, end row, send data
	char sendCmd[]={0x15, xs, xs+w-1, 0x75, ys, ys+h-1, 0x2C};
	ioOledSend(sendCmd, sizeof(sendCmd), 0);
	ioOledSend((char*)fb, w*h*2, 1);
}

void ssd1331Init() {
	char initSeq[]={ //note: not const char becauuse this is transfered using DMA
		0xFD, 0x12,
		0xAE, 0x81, 0x91, 0x82, 0x50, 0x83, 0x7D,
		0x87, 0x06, 0x8A, 0x64, 0x8B, 0x78, 0x8C, 0x64,
		0xA0, 0x72, 0xA1, 0x00, 0xA2, 0x00, 0xA4,
		0xA8, 0x3F, 0xAD, 0x8E, 0xB0, 0x00, 0xB1, 0x31,
		0xB3, 0xF0, 0xBB, 0x3A, 0x2E, 0xAF
	};
	ioOledSend(initSeq, sizeof(initSeq), 0);
}

void ssd1331PowerDown() {
	char powerOffSeq[]={
		0xAE, 0xAE,
		0xFD, 0x16,
	};
	ioOledSend(powerOffSeq, sizeof(powerOffSeq), 0);
}

