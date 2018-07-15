/*
 Wrapper for stdout to also be able to write to other things (e.g. rtc mem for debugging)

 The idea is that not everyone has access to the serial console of the PocketSprite, even when they're
 writing programs. To somewhat compensate for this, we also redirect the standard output to a ringbuffer
 located in fast RTC memory. The Chooser can then be used to read out this memory and show it to the user,
 e.g. over the WiFi interface.

 Note that this routine normally gets called from the kchal_init functions. These can be passed a specific
 flag to skip this installation, in case the contents of the RTC memory should be preserved.
*/

#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/xtensa_api.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_attr.h"
#include "soc/uart_struct.h"
#include "driver/uart.h"
#include "sdkconfig.h"
#include "8bkc-vfs-stdout.h"

static int uart_fd=0;
kchal_stdout_rtc_buf_t *kchal_stdout_rtc_buf=(kchal_stdout_rtc_buf_t*)0x50000000;

static portMUX_TYPE bufmux=portMUX_INITIALIZER_UNLOCKED;

static void add_to_rtc_rb(const char *data, size_t size) {
	kchal_stdout_rtc_buf_t *rb=kchal_stdout_rtc_buf; //for quicker typing
	portENTER_CRITICAL(&bufmux);
	//Note we use memcpy here intentionally, RTC ram is somewhat slow (as it runs on the RTC clock)
	//so we want to do accesses in 32-bit increments as much as we can. Memcpy is optimized to provide
	//this.
	int rem_sz=rb->bufsz - rb->writeptr;
	if (rem_sz < size) {
		//Data doesn't entirely fit in remaining bit in ringbuffer. Write the bit before the
		//wraparound first.
		memcpy(&rb->buffer[rb->writeptr], data, rem_sz);
		rb->writeptr=0;
		size-=rem_sz;
		data+=rem_sz;
		rb->has_wrapped=1;
	}
	//If data still doesn't fit in ringbuffer, only copy the last bit of it.
	if (size > rb->bufsz) {
		int dif=rb->bufsz-size;
		data+=dif+1;
		size=rb->bufsz-1;
	}
	//Copy final bit.
	memcpy(&rb->buffer[rb->writeptr], data, size);
	rb->writeptr+=size;
	portEXIT_CRITICAL(&bufmux);
}

static ssize_t stdout_write(int fd, const void * data, size_t size) {
	add_to_rtc_rb((const char*)data, size);
	return write(uart_fd, data, size);
}

static ssize_t stdout_read(int fd, void* data, size_t size) {
	return read(fd, data, size);
}

static int stdout_open(const char * path, int flags, int mode) {
	return 0;
}

static int stdout_close(int fd) {
	return 0;
}

static int stdout_fstat(int fd, struct stat * st) {
	st->st_mode = S_IFCHR;
	return 0;
}


void kchal_stdout_register() {
	const esp_vfs_t vfs = {
		.flags = ESP_VFS_FLAG_DEFAULT,
		.write = &stdout_write,
		.open = &stdout_open,
		.fstat = &stdout_fstat,
		.close = &stdout_close,
		.read = &stdout_read,
	};
	kchal_stdout_rtc_buf_t *rb=kchal_stdout_rtc_buf; //for quicker typing
	rb->magic=KCHAL_STDOUT_MAGIC;
	rb->bufsz=(8*1024)-sizeof(kchal_stdout_rtc_buf_t);
	rb->writeptr=0;
	rb->has_wrapped=0;

	uart_fd=open("/dev/uart/0", O_RDWR);
	ESP_ERROR_CHECK(esp_vfs_register("/dev/pkspstdout", &vfs, NULL));
	freopen("/dev/pkspstdout", "w", stdout);
	freopen("/dev/pkspstdout", "w", stderr);
	printf("8bkc_hal_stdout_register: Custom stdout/stderr handler installed.\n");
}

