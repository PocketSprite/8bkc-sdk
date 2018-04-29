#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/i2s.h"
#include "esp_deep_sleep.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "driver/dac.h"
#include "soc/rtc_cntl_reg.h"
#include "8bkc-hal.h"
#include "io-pksp.h"
#include "ssd1331.h"
#include "appfs.h"
#include "8bkc-ugui.h"
#include "ugui.h"

SemaphoreHandle_t oledMux;
SemaphoreHandle_t configMux;

//The hardware size of the oled.
#define OLED_REAL_H 64
#define OLED_REAL_W 96

//The oled is mounted in a bezel that blocks the left and right side of it; the remaining bit has the aspect
//ratio of 5:4 which is a more narrow screen and what people expect from a machine that can run 8-bit games.
#define OLED_FAKE_XOFF 8
#define OLED_FAKE_W KC_SCREEN_W
#define OLED_FAKE_YOFF 0
#define OLED_FAKE_H KC_SCREEN_H

//On the left side of the bezel, some pixels show through a hole in the bezel. It is used to fake a power LED.
#define OLED_LED_X 0
#define OLED_LED_Y 16
#define OLED_LED_H 3
#define OLED_LED_W 3

typedef struct {
	uint8_t volume;
	uint8_t brightness;
} ConfVars;
#define VOLUME_KEY "vol"
#define BRIGHTNESS_KEY "con" //backwards compatible; used to be mis-named 'contrast'
#define BATFULLADC_KEY "batadc"
#define KEYLOCK_KEY "kl"

static ConfVars config, savedConfig;
static QueueHandle_t soundQueue;
static int soundRunning=0;
static nvs_handle nvsHandle=NULL; //8bkc namespace
static nvs_handle nvsAppHandle=NULL; //app namespace
static uint32_t battFullAdcVal;

int kchal_get_hw_ver() {
	return 1;
}

//Accepts 16-bit colors in *non-swapped* order (for easy decodability)
static void setLed(int color) {
	uint16_t fb[OLED_LED_H*OLED_LED_W];
	for (int i=0; i<OLED_LED_H*OLED_LED_W; i++) fb[i]=(color>>8)|((color&0xff)<<8);
	xSemaphoreTake(oledMux, portMAX_DELAY);
	ssd1331SendFB(fb, OLED_LED_X, OLED_LED_Y, OLED_LED_H, OLED_LED_W);
	xSemaphoreGive(oledMux);
}


static void flushConfigToNvs() {
	//Check if anything changed
	if (memcmp(&config, &savedConfig, sizeof(config))==0) return;
	if (config.volume!=savedConfig.volume) nvs_set_u8(nvsHandle, VOLUME_KEY, config.volume);
	if (config.brightness!=savedConfig.brightness) nvs_set_u8(nvsHandle, BRIGHTNESS_KEY, config.brightness);
	//Okay, we're up to date again
	memcpy(&savedConfig, &config, sizeof(config));
	nvs_commit(nvsHandle);
}

/*
Management thread. Handles charging indicator, extra-long presses.
*/
static void kchal_mgmt_task(void *arg) {
	static int ledBlink=0;
	uint32_t keys;
	int longCt=0;
	int oldChgStatus=-1;
	int checkConfCtr=0;
	while(1) {
		keys=kchal_get_keys();
		//Check if powerdown is pressed > 5 sec. If so, hard powerdown.
		if (keys & KC_BTN_POWER_LONG) {
			longCt++;
			if (longCt>6) kchal_power_down();
		} else {
			longCt=0;
		}
		
		//See if we're charging; if so, modify 'LED'.
		int chgStatus=ioGetChgStatus();
		if (chgStatus!=oldChgStatus) {
			if (chgStatus==IO_CHG_NOCHARGER) {
				if (kchal_get_bat_pct()<10) {
					ledBlink^=1;
					setLed(0xF800);
					if (ledBlink&1) {
						setLed(0xF800);
					} else {
						setLed(0x0000);
					}
				} else {
					setLed(0xF800);
				}
			} else if (chgStatus==IO_CHG_CHARGING) {
				ledBlink^=1;
				if (ledBlink&1) {
					setLed(0x07E0);
				} else {
					setLed(0x0400);
				}
			} else if (chgStatus==IO_CHG_FULL) {
				setLed(0x07E0);
			}
		}

		checkConfCtr++;
		if (checkConfCtr==6) {
			checkConfCtr=0;
			flushConfigToNvs();
		}
		vTaskDelay(500/portTICK_PERIOD_MS);
	}
}

//Gets called when battery is full for a while. Used to calibrate the ADC: we know the battery is at
//precisely 4.2V so whatever value we measure on the ADC is that.
void kchal_cal_adc() {
	if (kchal_get_chg_status()!=KC_CHG_FULL) return;
	uint32_t v=ioGetVbatAdcVal();
	if (v==0) return;
	printf("Full battery ADC cal: 4.2V equals ADC val %d\n", v);
	nvs_set_u32(nvsHandle, BATFULLADC_KEY, v);
	battFullAdcVal=v;
}

#define BAT_FULL_MV 4200
#define BAT_EMPTY_MV 3200

int kchal_get_bat_mv() {
	int v=ioGetVbatAdcVal();
	if (battFullAdcVal==0) return 0;
	return (v*BAT_FULL_MV)/battFullAdcVal;
}

int kchal_get_bat_pct() {
	int pct=((kchal_get_bat_mv()-BAT_EMPTY_MV)*100)/(BAT_FULL_MV-BAT_EMPTY_MV);
	if (pct>100) pct=100;
	if (pct<0) pct=0;
	return pct;
}

const static uint8_t batEmptyIcon[]={
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
	1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
	1,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,
	1,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
	1,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
	1,0,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
	1,0,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
	1,0,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,
	1,0,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0
};

//ToDo: Do not use ugui (needs an entire frame buffer) but just use a partial blit
//to dump this to screen.
static void show_bat_empty_icon() {
	const int cols[2][3]={
		{0x0000, 0xffff, 0xf800},
		{0x0000, 0xffff, 0x0000},
	};
	kcugui_init();
	for (int i=0; i<6; i++) {
		kcugui_cls();
		UG_FontSelect(&FONT_6X8);
		UG_SetForecolor(C_WHITE);
		const uint8_t *p=batEmptyIcon;
		for (int y=0; y<12; y++) {
			for (int x=0; x<32; x++) {
				UG_DrawPixel(x+(80-32)/2, y+(64-12)/2, cols[i&1][*p++]);
			}
		}
		kcugui_flush();
		vTaskDelay(500/portTICK_RATE_MS);
	}
}


static uint32_t orig_store0_reg=0xFFFFFFFF;

uint32_t kchal_rtc_reg_bootup_val() {
	if (orig_store0_reg==0xFFFFFFFF) {
		return REG_READ(RTC_CNTL_STORE0_REG);
	} else {
		return orig_store0_reg;
	}
}

#define INIT_HW_DONE 1
#define INIT_SDK_DONE 2
#define INIT_COMMON_DONE 4
static int initstate=0;

static void kchal_init_common() {
	ssd1331SetBrightness(config.brightness);
	initstate|=INIT_COMMON_DONE;
}

void kchal_init_hw() {
	if (initstate&INIT_HW_DONE) return; //already did this
	oledMux=xSemaphoreCreateMutex();
	configMux=xSemaphoreCreateMutex();
	//Initialize IO
	ioInit();
	//Clear entire OLED screen
	uint16_t *fb=malloc(OLED_REAL_H*OLED_REAL_W*2);
	assert(fb);
	memset(fb, 0, OLED_REAL_H*OLED_REAL_W*2);
	ssd1331SendFB(fb, 0, 0, OLED_REAL_W, OLED_REAL_H);
	free(fb);
	initstate|=INIT_HW_DONE;
	if (initstate==(INIT_HW_DONE|INIT_SDK_DONE)) kchal_init_common();
}

void kchal_init_sdk() {
	if (initstate&INIT_SDK_DONE) return; //already did this
	//Hack: This initializes a bunch of locks etc; that process uses a bunch of stack. If we do not
	//do it here, it happens in the mgmt task, which is somewhat stack-starved.
	esp_get_deep_sleep_wake_stub();

	//If we were force-started during an usb loading cycle, remove that bit so when we shut down we'll go
	//back to the recharge screen
	uint32_t rg=REG_READ(RTC_CNTL_STORE0_REG);
	orig_store0_reg=rg; //save for later
	rg&=(~0x100);
	REG_WRITE(RTC_CNTL_STORE0_REG, rg);

	//Init appfs
	esp_err_t r=appfsInit(0x43, 3);
	assert(r==ESP_OK);
	printf("Appfs inited.\n");
	//Grab relevant nvram variables
	r=nvs_flash_init();
	if (r!=ESP_OK) {
		printf("Warning: NVS init failed!\n");
	}
	printf("NVS inited\n");

	//Default values
	config.volume=128;
	config.brightness=192;
	battFullAdcVal=2750;
	r=nvs_open("8bkc", NVS_READWRITE, &nvsHandle);
	if (r==ESP_OK) {
		nvs_get_u8(nvsHandle, VOLUME_KEY, &config.volume);
		nvs_get_u8(nvsHandle, BRIGHTNESS_KEY, &config.brightness);
		memcpy(&savedConfig, &config, sizeof(config));
		nvs_get_u32(nvsHandle, BATFULLADC_KEY, &battFullAdcVal);
	}
	
	//If available, grab app nvs handle
	appfs_handle_t thisApp;
	r=appfsGetCurrentApp(&thisApp);
	if (r==ESP_OK) {
		const char *name;
		appfsEntryInfo(thisApp, &name, NULL);
		printf("Opening NVS storage for app %s\n", name);
		r=nvs_open(name, NVS_READWRITE, &nvsAppHandle);
		if (r!=ESP_OK) {
			printf("Opening app NVS storage failed!\n");
		}
	} else {
		printf("No app running; factory app?\n");
		r=nvs_open("factoryapp", NVS_READWRITE, &nvsAppHandle);
	}


	//Use this info to measure battery voltage. If too low, refuse to start.
	ioVbatForceMeasure();
	printf("Battery voltage: %d mv\n", kchal_get_bat_mv());
	if (kchal_get_bat_pct() == 0) {
		show_bat_empty_icon();
		kchal_power_down();
	}
	xTaskCreatePinnedToCore(&kchal_mgmt_task, "kchal", 1024*4, NULL, 5, NULL, 0);
	initstate|=INIT_SDK_DONE;
	if (initstate==(INIT_HW_DONE|INIT_SDK_DONE)) kchal_init_common();
}

void kchal_init() {
	kchal_init_hw();
	kchal_init_sdk();
}

uint32_t kchal_get_keys() {
	return ioJoyReadInput();
}

void kchal_send_fb(const uint16_t *fb) {
	xSemaphoreTake(oledMux, portMAX_DELAY);
	ssd1331SendFB(fb, OLED_FAKE_XOFF, OLED_FAKE_YOFF, OLED_FAKE_W, OLED_FAKE_H);
	xSemaphoreGive(oledMux);
}

void kchal_send_fb_partial(const uint16_t *fb, int x, int y, int h, int w) {
	if (w<=0 || h<=0) return;
	if (x<0 || x+w>OLED_FAKE_W) return;
	if (y<0 || y+h>OLED_FAKE_H) return;
	xSemaphoreTake(oledMux, portMAX_DELAY);
	ssd1331SendFB(fb, x+OLED_FAKE_XOFF, y+OLED_FAKE_YOFF, w, h);
	xSemaphoreGive(oledMux);
}


void kchal_set_volume(uint8_t new_volume) {
	xSemaphoreTake(configMux, portMAX_DELAY);
	config.volume=new_volume;
	xSemaphoreGive(configMux);
}

uint8_t kchal_get_volume() {
	return config.volume;
}

void kchal_set_brightness(int brightness) {
	xSemaphoreTake(oledMux, portMAX_DELAY);
	ssd1331SetBrightness(brightness);
	xSemaphoreGive(oledMux);
	xSemaphoreTake(configMux, portMAX_DELAY);
	config.brightness=brightness;
	xSemaphoreGive(configMux);
}

uint8_t kchal_get_brightness(int brightness) {
	return config.brightness;
}

void kchal_sound_start(int rate, int buffsize) {
	i2s_config_t cfg={
		.mode=I2S_MODE_DAC_BUILT_IN|I2S_MODE_TX|I2S_MODE_MASTER,
		.sample_rate=rate,
		.bits_per_sample=16,
		.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format=I2S_COMM_FORMAT_I2S_MSB,
		.intr_alloc_flags=0,
		.dma_buf_count=4,
		.dma_buf_len=buffsize/4
	};
	i2s_driver_install(0, &cfg, 4, &soundQueue);
	i2s_set_pin(0, NULL);
	i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
	i2s_set_sample_rates(0, cfg.sample_rate);
	//I2S enables *both* DAC channels; we only need DAC2. DAC1 is connected to the select button.
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC_XPD_FORCE_M);
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC_M);
	gpio_config_t io_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_INPUT,
		.pull_up_en=1,
		.pin_bit_mask=(1<<25)
	};
	gpio_config(&io_conf);
	soundRunning=1;
}

void kchal_sound_mute(int doMute) {
	if (doMute) {
		dac_i2s_disable();
	} else {
		dac_i2s_enable();
	}
}

void kchal_sound_stop() {
	i2s_driver_uninstall(0);
}

#define SND_CHUNKSZ 32
void kchal_sound_push(uint8_t *buf, int len) {
	uint32_t tmpb[SND_CHUNKSZ];
	int i=0;
	while (i<len) {
		int plen=len-i;
		if (plen>SND_CHUNKSZ) plen=SND_CHUNKSZ;
		for (int j=0; j<plen; j++) {
			int s=((((int)buf[i+j])-128)*config.volume); //Make [-128,127], multiply with volume
			s=(s>>8)+128; //divide off volume max, get back to [0-255]
			if (s>255) s=255;
			if (s<0) s=0;
			tmpb[j]=((s)<<8)+((s)<<24);
		}
		i2s_write_bytes(0, (char*)tmpb, plen*4, portMAX_DELAY);
		i+=plen;
	}
}

/*
Powerdown does a small CRT-like animation: the screen collapses into a bright line which then fades out.
The nice thing is that this actually serves a purpose and the fade out of the line is not in code: we need
some pixels on to allow the 14V to collapse quickly, which is the purpose of the white line anyway. The white
line does the fade out because the 14V line is cut while the display still displays the same thing: you 
actually see the 14V decoupling caps emptying themselves over the LEDs.
*/

static void setPdownSquare(uint16_t *fb, int s) {
	if (s<=0 || s>32) return;
	uint16_t col=kchal_fbval_rgb(s*4+8, s*4+8, s*4+8);
	for (int x=s; x<=KC_SCREEN_W-s; x++) {
		for (int y=s; y<=KC_SCREEN_H-s; y++) {
			fb[x+y*KC_SCREEN_W]=col;
		}
	}
}

void kchal_power_down() {
	//Reuse ugui framebuffer if we can, otherwise try to allocate a new one.
	uint16_t *tmpfb=kcugui_get_fb();
	if (tmpfb==NULL) tmpfb=malloc(KC_SCREEN_H*KC_SCREEN_W*2);
	xSemaphoreTake(oledMux, 100/portTICK_PERIOD_MS);
	if (tmpfb!=NULL) {
		//Animate powerdown thing
		for (int s=1; s<32; s++) {
			memset(tmpfb, 0, KC_SCREEN_H*KC_SCREEN_W*2);
			setPdownSquare(tmpfb, s);
			ssd1331SendFB(tmpfb, OLED_FAKE_XOFF, OLED_FAKE_YOFF, OLED_FAKE_W, OLED_FAKE_H);
			vTaskDelay(10/portTICK_PERIOD_MS);
		}
	}
	uint8_t doLock=0;
	nvs_get_u8(nvsHandle, KEYLOCK_KEY, &doLock);
	if (doLock) {
		uint32_t rg=REG_READ(RTC_CNTL_STORE0_REG);
		rg=(rg&0xffffff)|0xa6000000;
		REG_WRITE(RTC_CNTL_STORE0_REG, rg);
	}
	ioOledPowerDown();
	ioPowerDown();
}

void kchal_exit_to_chooser() {
	kchal_set_new_app(-1);
	kchal_boot_into_new_app();
	abort();
}

int kchal_get_chg_status() {
	return ioGetChgStatus();
}

void kchal_set_new_app(int fd) {
	if (fd<0 || fd>255) {
		REG_WRITE(RTC_CNTL_STORE0_REG, 0);
	} else {
		REG_WRITE(RTC_CNTL_STORE0_REG, 0xA5000000|fd);
	}
}

/*
We use RTC store0 as a location to communicate with the bootloader.
Bit 31-24: 0xA5 if register is valid and bootloader needs to load app in fd on startup. WARNING: Other values
       may be valid as well; particularily the SDK will use 0xA6 if a power-up needs to boot to the chooser
       first to do the keylock unlock sequence.
       If the bootloader does not see A5, it will always load up the chooser.
Bit 8: 1 if charge detection needs to be overridden (charger is plugged in, but user pressed power to start
       app anyway)
Bit 7-0: FD of app to start

Note that the bootloader will not change this RTC register. It will only boot the app if the A5 value is valid.
It wil additionally also boot the app if the charger is present and bit 8 is set; if the charger is detected but
bit 8 is clear, it will always boot to the chooser.
*/
int kchal_get_new_app() {
	uint32_t r=REG_READ(RTC_CNTL_STORE0_REG);
	if ((r&0xFF000000)!=0xA5000000) return -1;
	return r&0xff;
}

//Internal to the Chooser
void kchal_set_rtc_reg(uint32_t val) {
	REG_WRITE(RTC_CNTL_STORE0_REG, val);
}

void kchal_boot_into_new_app() {
	ioOledPowerDown();
	esp_deep_sleep_enable_timer_wakeup(10);
	esp_deep_sleep_start();
}

nvs_handle kchal_get_app_nvsh() {
	return nvsAppHandle;
}

