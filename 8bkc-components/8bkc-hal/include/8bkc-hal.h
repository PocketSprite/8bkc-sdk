#ifndef KC_HAL_H
#define KC_HAL_H

#include <stdint.h>
#include "nvs.h"


#define KC_BTN_RIGHT (1<<0)
#define KC_BTN_LEFT (1<<1)
#define KC_BTN_UP (1<<2)
#define KC_BTN_DOWN (1<<3)
#define KC_BTN_START (1<<4)
#define KC_BTN_SELECT (1<<5)
#define KC_BTN_A (1<<6)
#define KC_BTN_B (1<<7)
#define KC_BTN_POWER (1<<8)
#define KC_BTN_POWER_LONG (1<<9)

//Warning: must be the same as the IO_CHG_* defines in io.h
#define KC_CHG_NOCHARGER 0
#define KC_CHG_CHARGING 1
#define KC_CHG_FULL 2

#define KC_SCREEN_W 80
#define KC_SCREEN_H 64


int kchal_get_hw_ver();

void kchal_init();
void kchal_init_hw();
void kchal_init_sdk();

uint32_t kchal_get_keys();

void kchal_send_fb(void *fb);
void kchal_send_fb_partial(void *fb, int x, int y, int h, int w);

void kchal_sound_start(int rate, int buffsize);

void kchal_sound_push(uint8_t *buf, int len);

void kchal_sound_stop();

void kchal_sound_mute(int doMute);


void kchal_power_down();

int kchal_get_chg_status();

void kchal_set_volume(uint8_t new_volume);

uint8_t kchal_get_volume();

void kchal_set_contrast(int contrast);

uint8_t kchal_get_contrast();

void kchal_exit_to_chooser();

void kchal_set_new_app(int fd);
int kchal_get_new_app();
void kchal_boot_into_new_app();

int kchal_get_bat_mv();
int kchal_get_bat_pct();
void kchal_cal_adc();

nvs_handle kchal_get_app_nvsh();

static inline uint16_t kchal_fbval_rgb(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t v=((r>>3)<<11)|((g>>2)<<5)|((b>>3)<<0);
	return (v>>8)|((v&0xff)<<8);
}

static inline uint16_t kchal_ugui_rgb(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t v=((r>>3)<<11)|((g>>2)<<5)|((b>>3)<<0);
	return (v&0xffff);
}


#endif