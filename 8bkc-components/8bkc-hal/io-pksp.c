#include <string.h>
#include <stdio.h>
#include <sdkconfig.h>
#include "rom/ets_sys.h"
#include "soc/gpio_reg.h"
#include "soc/dport_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io-pksp.h"
#include "ssd1331.h"
#include "esp_deep_sleep.h"
#include "driver/rtc_io.h"
#include "8bkc-hal.h" //for button codes


//Buttons. Pressing a button pulls down the associated GPIO
#define GPIO_BTN_RIGHT (1<<21)
#define GPIO_BTN_LEFT ((uint64_t)1<<39)
#define GPIO_BTN_UP ((uint64_t)1<<34)
#define GPIO_BTN_DOWN ((uint64_t)1<<35)
#define GPIO_BTN_B (1<<4)
#define GPIO_BTN_A (1<<16)
#define GPIO_BTN_SELECT (1<<25)
#define GPIO_BTN_START (1<<27)
#define GPIO_BTN_PWR_PIN 32
#define GPIO_BTN_PWR ((uint64_t)1<<GPIO_BTN_PWR_PIN)

//OLED connections
#define GPIO_OLED_CS_PIN 33UL
//#define GPIO_OLED_CS_PIN 5
#define GPIO_OLED_CLK_PIN 18
#define GPIO_OLED_DAT_PIN 23

//WARNING: Reset and /cs switched.
//#define GPIO_OLED_RST ((uint64_t)1UL<<33UL)
#define GPIO_OLED_RST ((uint64_t)1UL<<5UL)
#define GPIO_OLED_CS ((uint64_t)1UL<<GPIO_OLED_CS_PIN)
#define GPIO_OLED_CLK (1<<GPIO_OLED_CLK_PIN)
#define GPIO_OLED_DC (1<<22)
#define GPIO_OLED_DAT (1<<GPIO_OLED_DAT_PIN)
#define GPIO_DAC (1<<26)
#define GPIO_CHGDET (1<<19) //battery is charging, low-active
#define GPIO_CHGSTDBY ((uint64_t)1<<36)  //micro-usb plugged
#define GPIO_OLED_PWR (1<<2)
#define GPIO_14VEN (1<<17) //High enables 14V generation for OLED (and audio amp)

#define VBAT_ADC_CHAN ADC1_CHANNEL_5


#define OLED_SPI_NUM HSPI_HOST

spi_device_handle_t oled_spi_handle;


//For D/C handling to OLED
static void oled_spi_pre_transfer_callback(spi_transaction_t *t) {
	int dc=(int)t->user;
	WRITE_PERI_REG(dc?GPIO_OUT_W1TS_REG:GPIO_OUT_W1TC_REG, GPIO_OLED_DC);
}


#define VBATMEAS_HISTCT 16

int vbatHist[VBATMEAS_HISTCT]={0};
int vbatHistPos=0;

int ioGetVbatAdcVal() {
	int sum=0;
	for (int i=0; i<VBATMEAS_HISTCT; i++) {
		if (vbatHist[i]==0) {
			//Measured less than VBATMEAS_HISTCT values
			return (i==0)?0:sum/i;
		}
		sum+=vbatHist[i];
	}
	return sum/VBATMEAS_HISTCT;
}

void ioOledSend(const char *data, int count, int dc) {
	esp_err_t ret;
	spi_transaction_t t;
	if (count==0) return;             //no need to send anything
	memset(&t, 0, sizeof(t));       //Zero out the transaction
	t.length=count*8;                 //Len is in bytes, transaction length is in bits.
	t.tx_buffer=data;               //Data
	t.user=(void*)dc;               //D/C info for callback
	ret=spi_device_transmit(oled_spi_handle, &t);  //Transmit!
	assert(ret==ESP_OK);            //Should have had no issues.
	if (dc) { //data
		GPIO.func_out_sel_cfg[GPIO_OLED_CS_PIN].oen_inv_sel=1;
		SET_PERI_REG_MASK(rtc_gpio_desc[GPIO_OLED_CS_PIN].reg, (rtc_gpio_desc[GPIO_OLED_CS_PIN].mux));
		ets_delay_us(15);
		vbatHist[vbatHistPos++]=adc1_get_voltage(VBAT_ADC_CHAN);
		if (vbatHistPos>=VBATMEAS_HISTCT) vbatHistPos=0;
		GPIO.func_out_sel_cfg[GPIO_OLED_CS_PIN].oen_inv_sel=0;
		CLEAR_PERI_REG_MASK(rtc_gpio_desc[GPIO_OLED_CS_PIN].reg, (rtc_gpio_desc[GPIO_OLED_CS_PIN].mux));
	}
}

void ioVbatForceMeasure() {
	GPIO.func_out_sel_cfg[GPIO_OLED_CS_PIN].oen_inv_sel=1;
	SET_PERI_REG_MASK(rtc_gpio_desc[GPIO_OLED_CS_PIN].reg, (rtc_gpio_desc[GPIO_OLED_CS_PIN].mux));
	for (int i=0; i<VBATMEAS_HISTCT; i++) {
		vTaskDelay(1);
		vbatHist[i]=adc1_get_voltage(VBAT_ADC_CHAN);
	}
	GPIO.func_out_sel_cfg[GPIO_OLED_CS_PIN].oen_inv_sel=0;
	CLEAR_PERI_REG_MASK(rtc_gpio_desc[GPIO_OLED_CS_PIN].reg, (rtc_gpio_desc[GPIO_OLED_CS_PIN].mux));
}

int ioGetChgStatus() {
	uint64_t io=((uint64_t)GPIO.in1.data<<32)|GPIO.in;
	if ((io&GPIO_CHGSTDBY)==0) {
		return IO_CHG_NOCHARGER;
	} else {
		if ((io&GPIO_CHGDET)==0) {
			return IO_CHG_CHARGING;
		} else {
			return IO_CHG_FULL;
		}
	}
}


int ioJoyReadInput() {
	int i=0;
	static int initial=1;
	static int powerWasPressed=0;
	static uint32_t powerPressedTime;
	uint64_t io=((uint64_t)GPIO.in1.data<<32)|GPIO.in;
	static uint64_t last=0xffffffff;

	//There's some weirdness with the select button... I see dips of about 10uS in the signal. May be caused by 
	//some other hardware EMC'ing power into that line... need to research a bit more.
	//For now, here's a quick and dirty hack to fix it: if the select button was not pressed but is now, wait 12uS
	//(long enough for the glitch to pass) and re-sample.
	if (last&GPIO_BTN_SELECT) {
		if (!(io&GPIO_BTN_SELECT)) {
			ets_delay_us(12);
			io=((uint64_t)GPIO.in1.data<<32)|GPIO.in;
		}
	}
	last=io;

	//Ignore remnants from 1st power press
	if ((io&GPIO_BTN_PWR)) {
		if (!initial) {
			i|=KC_BTN_POWER;
			if (!powerWasPressed) powerPressedTime=xTaskGetTickCount();
			if ((xTaskGetTickCount()-powerPressedTime)>(1500/portTICK_PERIOD_MS)) {
				i|=KC_BTN_POWER_LONG;
			}
			powerWasPressed=1;
		}
	} else {
		initial=0;
		powerWasPressed=0;
	}
	if (!(io&GPIO_BTN_RIGHT)) i|=KC_BTN_RIGHT;
	if (!(io&GPIO_BTN_LEFT)) i|=KC_BTN_LEFT;
	if (!(io&GPIO_BTN_UP)) i|=KC_BTN_UP;
	if (!(io&GPIO_BTN_DOWN)) i|=KC_BTN_DOWN;
	if (!(io&GPIO_BTN_SELECT)) i|=KC_BTN_SELECT;
	if (!(io&GPIO_BTN_START)) i|=KC_BTN_START;
	if (!(io&GPIO_BTN_A)) i|=KC_BTN_A;
	if (!(io&GPIO_BTN_B)) i|=KC_BTN_B;
//	printf("%x\n", i);
	return i;
}

void ioOledPowerDown() {
	WRITE_PERI_REG(GPIO_OUT_W1TS_REG, GPIO_14VEN);
	vTaskDelay(300/portTICK_PERIOD_MS);
	ssd1331PowerDown();
}

void ioPowerDown() {
	printf("PowerDown: wait till power btn is released...\n");
	while(1) {
		uint64_t io=((uint64_t)GPIO.in1.data<<32)|GPIO.in;
		vTaskDelay(50/portTICK_PERIOD_MS);
		if (!(io&GPIO_BTN_PWR)) break;
	}
	//debounce
	vTaskDelay(200/portTICK_PERIOD_MS);

	esp_deep_sleep_enable_ext1_wakeup(GPIO_BTN_PWR|GPIO_CHGSTDBY, ESP_EXT1_WAKEUP_ANY_HIGH);
//	esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
	esp_deep_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);

	printf("PowerDown: esp_deep_sleep_start.\n");
	esp_deep_sleep_start();
	printf("PowerDown: after deep_sleep_start, huh?\n");
	while(1);
}

void ioInit() {
	int x;
	esp_err_t ret;
	spi_bus_config_t buscfg={
		.miso_io_num=-1,
		.mosi_io_num=GPIO_OLED_DAT_PIN,
		.sclk_io_num=GPIO_OLED_CLK_PIN,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
        .max_transfer_sz=4096*3
	};
	spi_device_interface_config_t devcfg={
		.clock_speed_hz=10000000,               //Clock out at 10 MHz
		.mode=0,                                //SPI mode 0
		.spics_io_num=GPIO_OLED_CS_PIN,         //CS pin
		.queue_size=7,                          //We want to be able to queue 7 transactions at a time
		.pre_cb=oled_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
	};

	gpio_config_t io_conf[]={
		{
			.intr_type=GPIO_INTR_DISABLE,
			.mode=GPIO_MODE_OUTPUT,
			.pin_bit_mask=GPIO_OLED_RST|GPIO_OLED_CS|GPIO_OLED_CLK|GPIO_OLED_DC|GPIO_OLED_DAT|GPIO_14VEN|GPIO_OLED_PWR
		},
		{
			.intr_type=GPIO_INTR_DISABLE,
			.mode=GPIO_MODE_INPUT,
			.pull_up_en=1,
			.pin_bit_mask=GPIO_BTN_RIGHT|GPIO_BTN_LEFT|GPIO_BTN_UP|GPIO_BTN_DOWN|GPIO_BTN_B|GPIO_BTN_A|GPIO_BTN_SELECT|GPIO_BTN_START|GPIO_CHGDET
		},
		{
			.intr_type=GPIO_INTR_DISABLE,
			.mode=GPIO_MODE_INPUT,
			.pull_down_en=1,
			.pin_bit_mask=GPIO_BTN_PWR|GPIO_CHGSTDBY
		}
	};
	//Connect all pins to GPIO matrix
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO21_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO39_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO34_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO35_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO16_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO25_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO27_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO36_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO33_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO22_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO23_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO18_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO19_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO26_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO17_U,2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U,2);

	//In some esp-idf versions, gpio 32/33 are hooked up as 32KHz xtal pins. Reset that.
	//Note: RTC_IO_X32N_MUX_SEL|RTC_IO_X32P_MUX_SEL need to be 0 instead of 1, as the trm notes.
	WRITE_PERI_REG(RTC_IO_XTAL_32K_PAD_REG, 0);

	for (x=0; x<sizeof(io_conf)/sizeof(io_conf[0]); x++) {
		gpio_config(&io_conf[x]);
	}

	//Start initializing OLED
	uint64_t pinsToClear=GPIO_OLED_PWR|GPIO_OLED_RST|GPIO_OLED_CS|GPIO_OLED_CLK|GPIO_OLED_DC|GPIO_OLED_DAT|GPIO_14VEN;
	WRITE_PERI_REG(GPIO_OUT_W1TC_REG, pinsToClear);
	WRITE_PERI_REG(GPIO_OUT1_W1TC_REG, (pinsToClear>>32UL));

	//Initialize battery voltage ADC
	//We double-use /CS as the voltage measurement pin.
	adc1_config_width(ADC_WIDTH_12Bit);
	adc1_config_channel_atten(VBAT_ADC_CHAN, ADC_ATTEN_0db);

	//Initialize the SPI bus
	ret=spi_bus_initialize(OLED_SPI_NUM, &buscfg, 1);
	assert(ret==ESP_OK);
	//Attach the LCD to the SPI bus
	ret=spi_bus_add_device(OLED_SPI_NUM, &devcfg, &oled_spi_handle);
	assert(ret==ESP_OK);

	//Enable power to OLED
	WRITE_PERI_REG(GPIO_OUT_W1TS_REG, GPIO_OLED_PWR);
	vTaskDelay(100 / portTICK_PERIOD_MS);

	//Enable 14V, un-reset OLED and initialize controller
	WRITE_PERI_REG(GPIO_OUT_W1TC_REG, GPIO_14VEN);
	vTaskDelay(20 / portTICK_PERIOD_MS);
//	WRITE_PERI_REG(GPIO_OUT1_W1TS_REG, (GPIO_OLED_RST)>>32);
	WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (GPIO_OLED_RST));
	vTaskDelay(20 / portTICK_PERIOD_MS);
	ssd1331Init();
}

