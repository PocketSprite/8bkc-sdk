/*
 Simple graphics library for the PocketSprite based on 8x8 tiles.
*/

#include "tilegfx.h"
#include "8bkc-hal.h"
#include <stdlib.h>
#include <string.h>

static uint16_t *fb;

//Render a tile that is not clipped by the screen extremities
static void render_tile_full(uint16_t *dest, const uint16_t *tile) {
	for (int y=0; y<8; y++) {
		memcpy(dest, tile, 8*2);
		tile+=8;
		dest+=KC_SCREEN_W;
	}
}

//Render a tile that is / may be clipped by the screen extremities
static void render_tile_part(uint16_t *dest, const uint16_t *tile, int xstart, int ystart, const tilegfx_rect_t *clip) {
	for (int y=0; y<8; y++) {
		if (y+ystart>=clip->x && y+ystart<clip->y+clip->h) {
			for (int x=0; x<8; x++) {
				if (x+xstart>=clip->y && x+xstart<clip->x+clip->w) dest[x]=tile[x];
			}
		}
		tile+=8;
		dest+=KC_SCREEN_W;
	}
}


void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *dest) {
	//Make sure to wrap around if offx/offy aren't in the tile map
	offx%=(tiles->w*8);
	offy%=(tiles->h*8);
	if (offx<0) offx+=tiles->w*8;
	if (offy<0) offy+=tiles->h*8;

	//Embiggen rendering field to start at the edges of all corner tiles.
	//We'll cut off the bits outside of dest when we get there.
	int sx=dest->x-(offx&7);
	int sy=dest->y-(offy&7);
	int ex=sx+((dest->w+(offx&7)+7)&~7);
	int ey=sy+((dest->h+(offy&7)+7)&~7);

	//x and y are the real onscreen coords that may fall outside the framebuffer.
	int tileposy=((offy/8)*tiles->w);
	uint16_t *p=fb+(KC_SCREEN_W*sy)+sx;
	memset(fb, 0x08, KC_SCREEN_H*KC_SCREEN_W*2);
	for (int y=sy; y<ey; y+=8) {
		int tileposx=offx/8;
		uint16_t *pp=p;
		for (int x=sx; x<ex; x+=8) {
			if (x < dest->x || y < dest->y || x+7 >= dest->x+dest->w || y+7 >= dest->y+dest->h) {
				render_tile_part(pp, &tiles->gfx[tiles->tiles[tileposx+tileposy]*64], 
							x - dest->x, y - dest->y, dest);
			} else {
				render_tile_full(pp, &tiles->gfx[tiles->tiles[tileposx+tileposy]*64]);
			}
			tileposx++;
			if (tileposx >= tiles->w) tileposx=0; //wraparound
			pp+=8; //we filled these 8 columns
		}
		tileposy+=tiles->w; //skip to next row
		if (tileposy >= tiles->h*tiles->w) tileposy-=tiles->h*tiles->w; //wraparound
		p+=KC_SCREEN_W*8; //we filled these 8 lines
	}
}


void tilegfx_init() {
	fb=malloc(KC_SCREEN_H*KC_SCREEN_W*2);
}

void tilegfx_flush() {
	kchal_send_fb(fb);
}

