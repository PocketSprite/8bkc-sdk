#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
Connector functions between the PocketSprite SDK and uGUI.
*/

/**
 * @brief Clear the uGUI framebuffer to black
 */
void kcugui_cls();

/**
 * @brief Flush the uGUI framebuffer to the OLED screen
 */
void kcugui_flush();

/**
 * @brief Initialize uGUI and this connector
 */
void kcugui_init();

/**
 * @brief De-initialize uGUI, release associated memory
 */
void kcugui_deinit();

/**
 * @brief Get pointer to framebuffer used by uGUI
 *
 * For this, uGUI needs to be initialized using kcugui_init. The framebuffer returned
 * will be KC_SCREEN_W*KC_SCREEN_H (normally 80*64) 16-bit words.
 *
 * @return Pointer to framebuffer
 */
uint16_t *kcugui_get_fb();

/**
 * @brief Convert an RGB value into a color usable by uGUI.
 *
 * @param r Red value (0-255)
 * @param g Green value (0-255)
 * @param b Blue value (0-255)
 * @return uGUI color value
 */
static inline uint16_t kchal_ugui_rgb(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t v=((r>>3)<<11)|((g>>2)<<5)|((b>>3)<<0);
	return (v&0xffff);
}

#ifdef __cplusplus
}
#endif
