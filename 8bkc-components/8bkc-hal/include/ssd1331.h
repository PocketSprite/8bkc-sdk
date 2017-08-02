#ifndef SSD1331_H
#define SSD1331_H

void ssd1331SendFB(uint16_t *fb);
void ssd1331Init();
void ssd1331SetContrast(int ctr);
void ssd1331PowerDown();

#endif