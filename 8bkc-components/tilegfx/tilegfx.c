/*
 Simple graphics library for the PocketSprite based on 8x8 tiles.
*/

#include "tilegfx.h"
#include "8bkc-hal.h"

static uint16_t *fb;

void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *dest) {
	uint16_t *p=fb+KC_SCREEN_W*dest->y+dest->x;
	for (int y=dest->y; y<dest->y+dest->h; y++) {
		uint16_t *pp=p;
		for (int x=dest->x; x<dest->x+dest->w; x++) {
			int tilex=(offx+x);
			int tiley=(offy+y);
			tilex%=tiles->w*8;
			tiley%=tiles->h*8;
			if (tilex<0) tilex+=tiles->w*8;
			if (tiley<0) tiley+=tiles->h*8;
			int tile=(tilex/8)+((tiley/8)*tiles->w);
			*pp++=tiles->gfx[tiles->tiles[tile]*64+(tiley&7)*8+(tilex&7)];
		}
		p+=KC_SCREEN_W;
	}
}

void tilegfx_init() {
	fb=malloc(KC_SCREEN_H*KC_SCREEN_W*2);
}

void tilegfx_flush() {
	kchal_send_fb(fb);
}

