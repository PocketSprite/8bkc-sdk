/*
TileGFX: A small tile-based renderer engine for the PocketSprite.
*/
#pragma once
#include <stdint.h>

typedef struct {
	int h;
	int w;
	const uint16_t *gfx;
	uint16_t tiles[];
} tilegfx_map_t;

typedef struct {
	int x, y;
	int w, h;
} tilegfx_rect_t;

void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *dest);
void tilegfx_init(int double_res);
void tilegfx_flush();
