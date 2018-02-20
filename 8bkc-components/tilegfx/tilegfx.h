/*
TileGFX: A small tile-based renderer engine for the PocketSprite.
*/
#pragma once
#include <stdint.h>

typedef struct {
	int trans_col; //transparent color, or -1 if none
	const uint16_t tile[];
} tilegfx_tileset_t;

typedef struct {
	int h;
	int w;
	const tilegfx_tileset_t *gfx;
	const uint16_t tiles[];
} tilegfx_map_t;

typedef struct {
	int x, y;
	int w, h;
} tilegfx_rect_t;

void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *dest);
void tilegfx_fade(uint8_t r, uint8_t g, uint8_t b, uint8_t pct);
int tilegfx_init(int double_res, int hz);
void tilegfx_flush();
void tilegfx_deinit();
