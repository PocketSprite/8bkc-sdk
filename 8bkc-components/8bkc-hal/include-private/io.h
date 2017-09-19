#ifndef IO_H
#define IO_H

#define PAD_LEFT (1<<0)
#define PAD_RIGHT (1<<1)
#define PAD_UP (1<<2)
#define PAD_DOWN (1<<3)
#define PAD_SELECT (1<<3)
#define PAD_START (1<<3)
#define PAD_A (1<<4)
#define PAD_B (1<<4)

#define IO_CHG_NOCHARGER 0
#define IO_CHG_CHARGING 1
#define IO_CHG_FULL 2



void ioOledSend(char *data, int count, int dc);
int ioJoyReadInput();
void ioInit();
void ioPowerDown();
int ioGetChgStatus();
void ioOledPowerDown();
int ioGetVbatAdcVal();
void ioVbatForceMeasure();

#endif