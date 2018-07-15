#pragma once

void kchal_stdout_register();

#define KCHAL_STDOUT_MAGIC 0x0dead109

typedef struct {
	int magic;
	int bufsz;
	int writeptr;
	int has_wrapped;
	char buffer[];
} kchal_stdout_rtc_buf_t;

extern kchal_stdout_rtc_buf_t *kchal_stdout_rtc_buf;