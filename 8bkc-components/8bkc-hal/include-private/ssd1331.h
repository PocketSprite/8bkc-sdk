#ifndef SSD1331_H
#define SSD1331_H

void ssd1331SendFB(const uint16_t *fb, int xs, int ys, int w, int h);
void ssd1331Init();
void ssd1331SetBrightness(int ctr);
void ssd1331PowerDown();

#endif