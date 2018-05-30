#ifndef KC_HAL_H
#define KC_HAL_H

#include <stdint.h>
#include "nvs.h"


#define KC_BTN_RIGHT (1<<0)			/*!< 'Right' on D-pad is pressed */
#define KC_BTN_LEFT (1<<1)			/*!< 'Left' on D-pad is pressed */
#define KC_BTN_UP (1<<2)			/*!< 'Up' on D-pad is pressed */
#define KC_BTN_DOWN (1<<3)			/*!< 'Down' on D-pad is pressed */
#define KC_BTN_START (1<<4)			/*!< 'Start' button is pressed */
#define KC_BTN_SELECT (1<<5)		/*!< 'Select' button is pressed */
#define KC_BTN_A (1<<6)				/*!< 'A' button is pressed */
#define KC_BTN_B (1<<7)				/*!< 'B' button is pressed */
#define KC_BTN_POWER (1<<8)			/*!< 'Power' button is pressed */
#define KC_BTN_POWER_LONG (1<<9)	/*!< 'Power' button is pressed and held longer than 1.5 seconds*/

//Warning: must be the same as the IO_CHG_* defines in io.h
#define KC_CHG_NOCHARGER 0			/*!< No charger attached, running from battery */
#define KC_CHG_CHARGING 1			/*!< Charger attached, internal battery is charging */
#define KC_CHG_FULL 2				/*!< Charger attached, battery is fully charged. */

#define KC_SCREEN_W 80				/*!< Screen width, excluding bezel area */
#define KC_SCREEN_H 64				/*!< Screen height */

/**
 * @brief Get PocketSprite hardware version
 *
 *
 * @return
 *     - -1 if hardware is not a PocketSprite
 *     - 1 for initial revision of hardware
 */
int kchal_get_hw_ver();



/**
 * @brief Initialize SDK
 *
 * This is used to initialize the SDK software, allowing SDK functions to be called. It does
 * not initialize any of the hardware.
 *
 * @warning For normal use, it is not advised to call this function. Use kchal_init() instead.
 */
void kchal_init_sdk();

/**
 * @brief Initialize PocketSprite hardware
 *
 * This is used to initialize the PocketSprite hardware, allowing hardware-related SDK functions
 * to work.
 *
 * @warning For normal use, it is not advised to call this function. Use kchal_init() instead.
 */
void kchal_init_hw();


/**
 * @brief Initialize PocketSprite SDK and hardware
 *
 * 
 * Before calling any of the other SDK functions, this function should normally be called. It 
 * essentially combines kchal_init_sdk() and kchal_init(). Please call this early in your apps
 * main function.
 */
void kchal_init();


/**
 * @brief Get keys pressed.
 *
 * @return A bitmap of the keys pressed; essentially an OR of all KC_BTN_* values of the keys pressed.
 */
uint32_t kchal_get_keys();

/**
 * @brief Wait until all buttons currently pressed are released
 *
 * @note Any keys pressed while this routine is running will *not* be monitored.
 **/
void kchal_wait_keys_released();

/**
 * @brief Send framebuffer to display
 *
 * @param fb Pointer to KC_SCREEN_W*KC_SCREEN_H (80x64) 16-bit values. The 16-bit values
 *           are defined in 5-6-5 RGB format (RRRRRGGG:GGGBBBBB), BUT with the two bytes swapped,
 *           so GGGBBBBB:RRRRRGGG.
 *
 * @note The kchal_fbval_rgb inline function provides an easy way to calculate pixel values
 *       starting from 8-bit RGB values.
 */
void kchal_send_fb(const uint16_t *fb);

/**
 * @brief Send partial framebuffer to display
 *
 * @param fb Pointer to h*w 16-bit values. Values are formatted as kchal_send_fb() describes.
 * @param x Starting horizontal position
 */
void kchal_send_fb_partial(const uint16_t *fb, int x, int y, int h, int w);


/**
 * @brief Start sound subsystem. 
 *
 * This starts up the I2S peripheral and configures the DAC for sound output. It allows you to push a
 * single sound stream to the speaker using kchal_sound_push.
 *
 * @param rate Sample rate, in Hz.
 * @param buffsize Buffer size, in bytes. Internally, I2S uses 32-bit samples, so the actual
 *                 buffer is ((buffsize/4)/rate) seconds.
 */
void kchal_sound_start(int rate, int buffsize);

/**
 * @brief Send samples to the sound subsystem
 *
 * This function is used to send a number of new samples to the sound subsystem. If the sound
 * buffer is full, this function will block until it is empty enough to store the provided samples
 * and only return then.
 *
 * Before the data passed here is output to the speaker, it is attenuated by the volume setting first.
 *
 * @param buf Samples, as unsigned bytes. (DC = 128)
 * @param len Lenght, in bytes, of the sample buffer buf
 */
void kchal_sound_push(uint8_t *buf, int len);

/**
 * @brief Stop/deinitialize the audio subsystem.
 */
void kchal_sound_stop();

/**
 * @brief Mute audio
 *
 * Silences the speakers. Use this when e.g. a game is paused and no data is fed into the audio
 * subsystem anymore using kchal_sound_push.
 *
 * @param doMute 1 to mute, 0 to unmute again.
 */
void kchal_sound_mute(int doMute);


/**
 * @brief Power down the PocketSprite
 *
 * Does the little crt-powering-off animation and puts the PocketSprite in deep sleep, to wake again
 * when the power button is pressed.
 *
 * When the power button is pressed again, your application will re-start from scratch, with all but the
 * RTC memory wiped clean. If you want to do any savestate saving, do it before you call this.
 */
void kchal_power_down();

/**
 * @brief Get the charging status of the PocketSprite
 *
 * @return One of the KC_CHG_* values
 */
int kchal_get_chg_status();

/**
 * @brief Set the volume.
 *
 * Note: The volume set here is stored in nvram as well and will be kept over a restart or app change.
 *
 * @param Volume, 0 is muted, 255 is full volume.
 */
void kchal_set_volume(uint8_t new_volume);

/**
 * @brief Get the current volume
 *
 * @return Volume, 0 is muted, 255 is full volume.
 */
uint8_t kchal_get_volume();

/**
 * @brief Set the brightness of the screen
 *
 * @param Brightness. 0 is lowest, 255 is full highest
 */
void kchal_set_brightness(int brightness);

/**
 * @brief Set the brightness of the screen
 *
 * @return Brightness. 0 is lowest, 255 is full highest
 */
uint8_t kchal_get_brightness();

/**
 * @brief Exit to chooser
 *
 * Stops the current app and reboots into the chooser menu, where the user can e.g. select
 * a different app to run. This function does not return.
 */
void kchal_exit_to_chooser();


/**
 * @brief Set new app to start after deep sleep
 *
 * Set a new app to start after deep sleep is over.
 *
 * This function is mostly used for chooser use (to start a new, chosen app) but can also be used by individual
 * apps. Call kchal_boot_into_new_app to actually reboot into the new app.
 * 
 * @param fd The AppFS fd of the app to start
 */
void kchal_set_new_app(int fd);

/**
 * @brief Get app set by set_new_app
 *
 * @return new app as set by kchal_set_new_app, possibly from the chooser or another app
 */
int kchal_get_new_app();

/**
 * @brief Boot into the app chosen using kchal_boot_into_new_app.
 *
 * This function does not return.
 */
void kchal_boot_into_new_app();

/**
 * @brief Get the current battery voltage in millivolt
 *
 * @return Current battery voltage
 */
int kchal_get_bat_mv();

/**
 * @brief Get the estimated battery full-ness, in percent
 *
 * @return Battery full-ness, 0-100. 0 is empty, 100 is entirely full.
 */
int kchal_get_bat_pct();

/**
 * @brief Re-calibrate the ADC when the battery is fully charged
 *
 * Used during ATE. Please do not call from apps.
 */
void kchal_cal_adc();

/**
 * @brief Get the NVS handle for the current app
 *
 * To facilitate storing things like high-scores, preferences etc, it is possible to use the NVS subsystem
 * of esp-idf. To stop namespace clashes, the PocketSprite SDK allocates a namespace for each installed app.
 * Use this call to get a handle to that namespace; feel free to save whatever fragment you like into it. Please
 * do not use more than a few K here: the NVS partition is shared between all apps. For larger storage, plese
 * write to an AppFs file.
 */
nvs_handle kchal_get_app_nvsh();

/**
 * @brief Calculate a value for a pixel in the OLED framebuffer from a triplet of 8-bit RGB values
 *
 * @param r Red value (0-255)
 * @param g Green value (0-255)
 * @param b Blue value (0-255)
 */
static inline uint16_t kchal_fbval_rgb(uint8_t r, uint8_t g, uint8_t b) {
	uint16_t v=((r>>3)<<11)|((g>>2)<<5)|((b>>3)<<0);
	return (v>>8)|((v&0xff)<<8);
}

#endif
