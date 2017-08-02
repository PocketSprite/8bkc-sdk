#ifndef CLASSDAMP_RMT_H
#define CLASSDAMP_RMT_H

#include "esp_err.h"

void classd_rmt_rate(int rate, int depth);
esp_err_t classd_rmt_init();

//Warning: This takes _signed_ samples.
void classd_rmt_push(int sample);

//Takes 128 bytes of _unsigned) samples
void classd_rmt_push_buf(uint8_t *buf);


#endif