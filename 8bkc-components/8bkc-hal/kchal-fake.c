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
#include "spi_lcd.h"
#include "appfs.h"
#include "8bkc-ugui.h"
#include "ugui.h"
#include "psxcontroller.h"

SemaphoreHandle_t oledMux;
SemaphoreHandle_t configMux;

//The hardware size of the display.
#define OLED_REAL_H 320
#define OLED_REAL_W 240

//Bit used as pocketsprite screen
#define OLED_FAKE_XOFF ((OLED_REAL_W-KC_SCREEN_W)/2)
#define OLED_FAKE_W KC_SCREEN_W
#define OLED_FAKE_YOFF ((OLED_REAL_H-KC_SCREEN_H)/2)
#define OLED_FAKE_H KC_SCREEN_H

typedef struct {
	uint8_t volume;
	uint8_t contrast;
} ConfVars;
#define VOLUME_KEY "vol"
#define CONTRAST_KEY "con"
#define BATFULLADC_KEY "batadc"

static ConfVars config, savedConfig;
static QueueHandle_t soundQueue;
static int soundRunning=0;
static nvs_handle nvsHandle=NULL, nvsAppHandle=NULL;

int kchal_get_hw_ver() {
	return -1; //fake
}


static void flushConfigToNvs() {
	//Check if anything changed
	if (memcmp(&config, &savedConfig, sizeof(config))==0) return;
	if (config.volume!=savedConfig.volume) nvs_set_u8(nvsHandle, VOLUME_KEY, config.volume);
	if (config.contrast!=savedConfig.contrast) nvs_set_u8(nvsHandle, CONTRAST_KEY, config.contrast);
	//Okay, we're up to date again
	memcpy(&savedConfig, &config, sizeof(config));
	nvs_commit(nvsHandle);
}


void kchal_cal_adc() {
	//stub
}

int kchal_get_bat_mv() {
	return 3600; //stub
}

int kchal_get_bat_pct() {
	return 100;
}

static uint32_t orig_store0_reg=0xFFFFFFFF;

uint32_t kchal_rtc_reg_bootup_val() {
	if (orig_store0_reg==0xFFFFFFFF) {
		return REG_READ(RTC_CNTL_STORE0_REG);
	} else {
		return orig_store0_reg;
	}
}

static volatile uint16_t buttons=0xff;

	//On the real pocketsprite, this handles battery etc. On the 'fake' one, this handles
	//the input.
#if CONFIG_HW_INPUT_PSX
static void kchal_mgmt_task(void *args) {
	//Order of (PocketSprite) keys as array indices: r, l, u, d, start, sel, a, b, power
	//psx mapping: a=circle, b=X, power=triangle
	const uint16_t btns[8]={
		0x20, 0x80, 0x10, 0x40, 0x8, 0x1, 0x2000, 0x4000, 0x1000
	};
	psxcontrollerInit();
	while(1) {
		uint16_t b=psxReadInput(); //warning: this returns 1s for *non*pressed buttons.
		uint16_t tb=0;
		for (int i=0; i<8; i++) {
			if ((b&btns[i])==0) tb|=(1<<i);
		}
		buttons=tb;
		//if (b!=0xffff) printf("btn %x\n", ~b);
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}
#else

//User serial port input as input
static void kchal_mgmt_task(void *args) {
	printf("Using serial port for input.\n");
	printf("Use arrow keys or JIKL for D-pad.\n");
	printf("Use A, S for A, B buttons.\n");
	printf("Use Z for start, X for select, P for power.\n");
	int ansi_escaped=0;
	int b;
	while(1) {
		b=0;
		while(1) {
			int c=getchar();
			if (c==-1) break;
			if (ansi_escaped==0 && c=='\033') {
				ansi_escaped++;
			} else if (ansi_escaped==1 && c=='[') {
				ansi_escaped++;
			} else if (ansi_escaped==2) {
				if (c=='A') b=KC_BTN_UP;
				if (c=='B') b=KC_BTN_DOWN;
				if (c=='D') b=KC_BTN_LEFT;
				if (c=='C') b=KC_BTN_RIGHT;
				ansi_escaped=0;
			} else {
				ansi_escaped=0;
				if (c=='a') b=KC_BTN_A;
				if (c=='s') b=KC_BTN_B;
				if (c=='z') b=KC_BTN_START;
				if (c=='x') b=KC_BTN_SELECT;
				if (c=='j') b=KC_BTN_LEFT;
				if (c=='i') b=KC_BTN_UP;
				if (c=='k') b=KC_BTN_DOWN;
				if (c=='l') b=KC_BTN_RIGHT;
				if (c=='p') b=KC_BTN_POWER;
			}
		}
		buttons=b;
		vTaskDelay(100/portTICK_PERIOD_MS);
	}
}

#endif

#define INIT_HW_DONE 1
#define INIT_SDK_DONE 2
#define INIT_COMMON_DONE 4
static int initstate=0;

static void kchal_init_common() {
	initstate|=INIT_COMMON_DONE;
}

void kchal_init_hw() {
	if (initstate&INIT_HW_DONE) return; //already did this
	oledMux=xSemaphoreCreateMutex();
	configMux=xSemaphoreCreateMutex();
	//Route DAC
	i2s_set_pin(0, NULL);
	i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
	//I2S enables *both* DAC channels; we only need DAC2. Do some Deeper Magic to make this into
	//an essentially uninitialized GPIO pin again.
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC_XPD_FORCE_M);
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC_M);
	gpio_config_t io_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_INPUT,
		.pull_up_en=1,
		.pin_bit_mask=(1<<25)
	};
	gpio_config(&io_conf);

	//Initialize display
	spi_lcd_init();
	//Clear entire OLED screen
	uint16_t *fb=malloc(OLED_REAL_W*2);
	assert(fb);
	memset(fb, 0, OLED_REAL_W*2);
	for (int y=0; y<320; y++) {
		spi_lcd_send(0, y, 240, 1, fb);
	}
	free(fb);
	initstate|=INIT_HW_DONE;
	if (initstate==(INIT_HW_DONE|INIT_SDK_DONE)) kchal_init_common();
}

void kchal_init_sdk() {
	esp_err_t r;
	if (initstate&INIT_SDK_DONE) return; //already did this
	//Hack: This initializes a bunch of locks etc; that process uses a bunch of locks. If we do not
	//do it here, it happens in the mgmt task, which is somewhat stack-starved.
	esp_get_deep_sleep_wake_stub();

#if 0
 //no appfs for fake pocketsprite for now?
	//Init appfs
	esp_err_t r=appfsInit(0x43, 3);
	assert(r==ESP_OK);
	printf("Appfs inited.\n");
#endif
	//Grab relevant nvram variables
	r=nvs_flash_init();
	if (r!=ESP_OK) {
		printf("Warning: NVS init failed!\n");
	}
	printf("NVS inited\n");

	//Default values
	config.volume=128;
	config.contrast=192;
	r=nvs_open("8bkc", NVS_READWRITE, &nvsHandle);
	if (r==ESP_OK) {
		nvs_get_u8(nvsHandle, VOLUME_KEY, &config.volume);
		nvs_get_u8(nvsHandle, CONTRAST_KEY, &config.contrast);
		memcpy(&savedConfig, &config, sizeof(config));
	}

	//We don't have appfs, so we can't deduce the app name. Just assume a generic appname for nvs.
	char *name="MyApp";
	printf("Opening NVS storage for app %s\n", name);
	r=nvs_open(name, NVS_READWRITE, &nvsAppHandle);
	if (r!=ESP_OK) {
		printf("Opening app NVS storage failed!\n");
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
	return buttons;
}

void kchal_send_fb(const uint16_t *fb) {
	xSemaphoreTake(oledMux, portMAX_DELAY);
	spi_lcd_send(OLED_FAKE_XOFF, OLED_FAKE_YOFF, OLED_FAKE_W, OLED_FAKE_H, fb);
	xSemaphoreGive(oledMux);
}

void kchal_send_fb_partial(const uint16_t *fb, int x, int y, int w, int h) {
	if (w<=0 || h<=0) return;
	if (x<0 || x+w>OLED_FAKE_W) return;
	if (y<0 || y+h>OLED_FAKE_H) return;
	xSemaphoreTake(oledMux, portMAX_DELAY);
	spi_lcd_send(x+OLED_FAKE_XOFF, y+OLED_FAKE_YOFF, w, h, fb);
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

void kchal_set_contrast(int contrast) {
	xSemaphoreTake(configMux, portMAX_DELAY);
	config.contrast=contrast;
	xSemaphoreGive(configMux);
}

uint8_t kchal_get_contrast(int contrast) {
	return config.contrast;
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
	i2s_set_sample_rates(0, cfg.sample_rate);
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

void kchal_power_down() {
	printf("Powerdown not implemented on fake hardware. Aborting!\n");
	abort();
}

void kchal_exit_to_chooser() {
	printf("Exit to chooser not implemented on fake hardware. Aborting!\n");
	abort();
}

int kchal_get_chg_status() {
	return KC_CHG_NOCHARGER;
}

void kchal_set_new_app(int fd) {
	//Stub: no chooser
}

int kchal_get_new_app() {
	//Stub: no chooser
	return -1;
}

void kchal_boot_into_new_app() {
	printf("ERROR: No chooser in fake hardware!\n");
	abort();
}

nvs_handle kchal_get_app_nvsh() {
	return nvsAppHandle;
}

