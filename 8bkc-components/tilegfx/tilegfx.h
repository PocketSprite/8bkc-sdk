/*
TileGFX: A small tile-based renderer engine for the PocketSprite.
*/
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Structure describing one frame of animation
 */
typedef struct {
	uint16_t delay_ms;	/*!< Delay for one frame. On the 0th frame of a seq, this indicates total duration. */
	uint16_t tile;		/*!< Tile index for frame. 0xffff on 0th frame of a seq. */
} tilegfx_anim_frame_t;

/**
 * @brief Structure describing a set of tiles, usable in a tilemap.
 */
typedef struct {
	int trans_col;					/*!< transparent color, or -1 if none */
	const uint16_t *anim_offsets;	/*!< Array of offsets into the animation frames array. Indexed by tile index. */
									/*!< If a tile is animated, it will have an offset number in this array at its  */
									/*!< index. That offset is 0xffff if no animation. */
	const tilegfx_anim_frame_t *anim_frames; /*< Pointer to array describing the various animations in the tileset */
	const uint16_t tile[];			/*!< Raw tile data. Each tile is 64 16-bit words worth of graphics data. */
} tilegfx_tileset_t;

/**
 * @brief Structure describing a tilemap
 */
typedef struct {
	int h;							/*!< Height of the tilemap, in tiles */
	int w;							/*!< Width of the tilemap, in tiles */
	const tilegfx_tileset_t *gfx;	/*!< Pointer to the tileset used in the map */
	const uint16_t tiles[];			/*!< Array of the tiles in the map. Values can be 0xffff for no tile. */
} tilegfx_map_t;

/**
 * @brief Structure describing a rectangle
 */
typedef struct {
	int x; /*!< Position of left side of rectangle */
	int y; /*!< Position of top of rectangle */
	int w; /*!< Width of rectangle */
	int h; /*!< Height of rectangle */
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


/**
 * @brief Create an empty tilemap in RAM
 *
 * This can be used to generate a tilemap that is in RAM and this modifiable from the code
 *
 * @param w Width, in tiles, of the map
 * @param h Height, in tiles, of the map
 * @param tiles Tileset the map will use
 * @return The map (with all tiles set to 0xffff/transparent), or NULL if out of memory
 */
tilegfx_map_t *tilegfx_create_tilemap(int w, int h, const tilegfx_tileset_t *tiles);


/**
 * @brief Create an editable copy of a tilemap
 *
 * Use this to duplicate a tilemap located into flash into one located into RAM.
 *
 * @param orig Tilemap to duplicate
 * @return Duplicated tilemap, or NULL if out of memory
 */
tilegfx_map_t *tilegfx_dup_tilemap(const tilegfx_map_t *orig);


/**
 * @brief Free a tilemap created with tilegfx_create_tilemap or tilegfx_dup_tilemap
 *
 * @param map Map to free
 */
void tilegfx_destroy_tilemap(tilegfx_map_t *map);


/**
 * @brief Set tile position in RAM-allocated map to the specific tile in its associated tileset
 *
 * @param map Tilemap to modify
 * @param x X-position of tile to change
 * @param y Y-position of tile to change
 * @param tile Tile index to change to (or 0xffff for completely transparent)
 */
static inline void tilegfx_set_tile(tilegfx_map_t *map, int x, int y, uint16_t tile) {
	//Cast to non-const... kind-of yucky but this is the least invasive way to do this.
	uint16_t *t=(uint16_t*)&map->tiles[x+y*map->w];
	*t=tile;
}

/**
 * @brief Get tile in specified tile position of tilemap
 *
 * @param map Tilemap to read
 * @param x X-position of tile
 * @param y Y-position of tile
 * @return tile Tile index in tileset (or 0xffff for completely transparent)
 */
static inline uint16_t tilegfx_get_tile(const tilegfx_map_t *map, int x, int y) {
	return map->tiles[x+y*map->w];
}

/**
 * @brief Get internal framebuffer
 *
 * This returns a pointer to the internal framebuffer memory where tilegfx renders everything.
 * You can use this to manually poke pixels into memory, or to get access to the raw pixels
 * after some tiles have been rendered. Note that in double_res mode, after rendering tilesets
 * this framebuffer is 160x128 pixels in size, but after calling tilegfx_flush the contents
 * will be resized to 80x64!
 *
 * Pixels here are in big-endian RGB565 format (same as what's sent to the OLED).
 */
uint16_t *tilegfx_get_fb();

#ifdef __cplusplus
}
#endif
