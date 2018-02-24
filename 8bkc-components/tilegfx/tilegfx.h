/*
TileGFX: A small tile-based renderer engine for the PocketSprite.
*/
#pragma once
#include <stdint.h>

typedef struct {
	uint16_t delay_ms; //Delay for one frame. On the 0th frame of a seq, this indicates total duration.
	uint16_t tile; //Tile index for frame. 0xffff on 0th frame of a seq.
} tilegfx_anim_frame_t;

typedef struct {
	int trans_col; //transparent color, or -1 if none
	const uint16_t *anim_offsets; //Offsets into the frames array. Indexed by tile index.
	const tilegfx_anim_frame_t *anim_frames;
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

/**
 * @brief Initialize tilegfx system
 *
 * Allocates the OLED framebuffer and the infrastructure for the virtual VBlank.
 *
 * @param double_res Set to 0 to get a framebuffer in the native (80x64) resolution; set to
 *                   1 to get a 160x128 framebuffer that is then subpixel-aware scaled to the OLED.
 * @param hz Set the 'refresh rate', as in the tilegfx_flush call is throttled to a maximum of this amount
 *           of calls per second.
 * @return True if succesful, false if out of memory.
 */
int tilegfx_init(int double_res, int hz);

/**
 * @brief Render the OLED framebuffer to the actual OLED display
 *
 * This shows the result of all tilegfx calls on the PocketSprite display. This call may block for a while
 * in order for it to never be called at more than 'refresh rate' passed to tilegfx_init().
 */
void tilegfx_flush();

/**
 * @brief De-initialize tilegfx
 *
 * This frees the OLED-buffer and all other resources allocated when initializing tilegfx.
 */
void tilegfx_deinit();

/**
 * @brief Render a tilemap to screen
 *
 * This renders the content of a tilemap to the OLED framebuffer. It takes care of transparency and clips the
 * regions that fall outside of the destination rectangle. The tilemap is rendered in a repeating way, so it
 * 'wraps around' when displaying bits outside its actual height and/or width.
 * 
 * @param tiles The tilemap to render
 * @param offx X-offset in the tilemap. The tilemap is started with (offx, offy) in the top right corner if
 *             the destionation rectangle.
 * @param offy Y-offset in the tilemap.
 * @param dest Rectangle, in the coordinates of the OLED framebuffer, to limit rendering to. If this parameter
 *             is NULL, the tilemap is rendered to the entire framebuffer.
 */
void tilegfx_tile_map_render(const tilegfx_map_t *tiles, int offx, int offy, const tilegfx_rect_t *dest);

/**
 * @brief 'Fade' the framebuffer to a certain color.
 *
 * This effectively sets every pixel in the OLED framebuffer to a color that is (pct/255)'th the original color
 * that was there, 1-(pct/255)'th the color indicated by the RGB values. A pct value of 256 does not affect the
 * OLED framebuffer, a pct value of 0 clears it entirely to the color given in r,g,b.
 *
 * @param r Red component of fade color
 * @param g Green component of fade color
 * @param b Blue component of fade color
 * @param pct Fade amount (0-256)
 */
void tilegfx_fade(uint8_t r, uint8_t g, uint8_t b, uint8_t pct);


