#include <string.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/i2s.h"
#include "esp_deep_sleep.h"

#include "soc/rtc_cntl_reg.h"
#include "8bkc-hal.h"
#include "io.h"
#include "ssd1331.h"

SemaphoreHandle_t oledMux;

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

static int volume=128;
static QueueHandle_t soundQueue;
static int soundRunning=0;

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

/*
Management thread. Handles charging indicator, extra-long presses.
*/
static void kchal_mgmt_task(void *arg) {
	static int ledBlink=0;
	uint32_t keys;
	int longCt=0;
	int oldChgStatus=-1;
	while(1) {
		keys=kchal_get_keys();
		//Check if powerdown is pressed > 5 sec. If so, hard powerdown.
		if (keys & KC_BTN_POWER_LONG) {
			longCt++;
			if (longCt>6) ioPowerDown();
		} else {
			longCt=0;
		}
		
		//See if we're charging; if so, modify 'LED'.
		int chgStatus=ioGetChgStatus();
		if (chgStatus!=oldChgStatus) {
			if (chgStatus==IO_CHG_NOCHARGER) {
				setLed(0xF800);
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
		vTaskDelay(500/portTICK_PERIOD_MS);
	}
}

void kchal_init() {
	oledMux=xSemaphoreCreateMutex();
	//Initialize IO
	ioInit();
	//Clear entire OLED screen
	uint16_t *fb=malloc(OLED_REAL_H*OLED_REAL_W*2);
	memset(fb, 0, OLED_REAL_H*OLED_REAL_W*2);
	ssd1331SendFB(fb, 0, 0, OLED_REAL_W, OLED_REAL_H);
	free(fb);
	xTaskCreatePinnedToCore(&kchal_mgmt_task, "kchal", 1024, NULL, 5, NULL, 1);
}

uint32_t kchal_get_keys() {
	return ioJoyReadInput();
}

void kchal_send_fb(void *fb) {
	xSemaphoreTake(oledMux, portMAX_DELAY);
	ssd1331SendFB(fb, OLED_FAKE_XOFF, OLED_FAKE_YOFF, OLED_FAKE_W, OLED_FAKE_H);
	xSemaphoreGive(oledMux);
}

void kchal_set_volume(uint8_t new_volume) {
	volume=new_volume;
}

uint8_t kchal_get_volume() {
	return volume;
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
		.dma_buf_len=256
	};
	i2s_driver_install(0, &cfg, 4, &soundQueue);
	i2s_set_pin(0, NULL);
	i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
	i2s_set_sample_rates(0, cfg.sample_rate);

#if 0
	//I2S enables *both* DAC channels; we only need DAC2. DAC1 is connected to the select button.
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC_XPD_FORCE_M);
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC_M);
#endif
	soundRunning=1;
}

#define SND_CHUNKSZ 32
void kchal_sound_push(uint8_t *buf, int len) {
	uint32_t tmpb[SND_CHUNKSZ];
	int i=0;
	while (i<len) {
		int plen=len-i;
		if (plen>SND_CHUNKSZ) plen=SND_CHUNKSZ;
		for (int j=0; j<plen; j++) {
			int s=((((int)buf[i])-128)*volume); //Make [-128,127], multiply with volume
			s=(s<<8)+128; //divide off volume max, get back to [0-255]
			if (s>255) s=255;
			if (s<0) s=0;
			tmpb[i]=((s)<<8)+((s)<<24);
		}
		i2s_write_bytes(0, (char*)tmpb, SND_CHUNKSZ*4, portMAX_DELAY);
		i+=plen;
	}
}

void kchal_power_down() {
	void ioPowerDown();
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

int kchal_get_new_app() {
	uint32_t r=REG_READ(RTC_CNTL_STORE0_REG);
	if ((r&0xFF000000)!=0xA5000000) return -1;
	return r&0xff;
}

void kchal_boot_into_new_app() {
	esp_deep_sleep_enable_timer_wakeup(10);
	esp_deep_sleep_start();
}

