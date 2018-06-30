#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define POWERBTN_MENU_NONE 0
#define POWERBTN_MENU_EXIT 1
#define POWERBTN_MENU_POWERDOWN 2

/**
 * @brief Show powerbutton menu
 *
 * Shows a menu allowing users to adjust volume and contrast, exit the application or power down the
 * PocketSprite. Ususally invoked after the power button is pressed.
 *
 * @arg fb An 80x64 array of uint16 pixels used as the framebuffer by whatever graphics subsystem
 *         is used. For instance, the result of tilegfx_get_fb() after a tilegfx_flush() call.
 * @return One of
 *         - POWERBTN_MENU_NONE - User cancelled the menu
 *         - POWERBTN_MENU_EXIT - User chose to exit the application
 *         - POWERBTN_MENU_POWERDOWN - User chose to powerdown the PocketSprite
 */

int powerbtn_menu_show(uint16_t *fb);

#ifdef __cplusplus
}
#endif
