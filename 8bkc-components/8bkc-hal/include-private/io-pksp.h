#ifndef IO_H
#define IO_H


#define IO_CHG_NOCHARGER 0
#define IO_CHG_CHARGING 1
#define IO_CHG_FULL 2



void ioOledSend(const char *data, int count, int dc);
int ioJoyReadInput();
void ioInit();
void ioPowerDown();
int ioGetChgStatus();
void ioOledPowerDown();
int ioGetVbatAdcVal();
void ioVbatForceMeasure();

#endif