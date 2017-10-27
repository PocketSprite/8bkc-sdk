#include <string.h>
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include "ugui.h"
#include "8bkc-hal.h"

static uint16_t *fb;
static UG_GUI *ugui;

static void oled_pset(UG_S16 x, UG_S16 y, UG_COLOR c) {
	if (fb==NULL) return;
	if (x<0 || x>=KC_SCREEN_W) return;
	if (y<0 || y>=KC_SCREEN_H) return;
	fb[(x)+(y*KC_SCREEN_W)]=(c>>8)|(c<<8);
}

void kcugui_cls() {
	memset(fb, 0, KC_SCREEN_W*KC_SCREEN_H*2);
}

void kcugui_flush() {
	kchal_send_fb(fb);
}

void kcugui_init() {
	fb=malloc(KC_SCREEN_H*KC_SCREEN_W*2);
	ugui=malloc(sizeof(UG_GUI));
	UG_Init(ugui, oled_pset, KC_SCREEN_W, KC_SCREEN_H);
	kcugui_cls();
	kcugui_flush();
}

void kcugui_deinit() {
	free(fb);
	free(ugui);
	fb=NULL;
	ugui=NULL;
}

uint16_t *kcugui_get_fb() {
	return fb;
}


