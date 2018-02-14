#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "sndmixer.h"

#define CHUNK_SIZE 32


typedef struct {
	const int8_t *data;
	int pos;
	int data_len;
	int rate;
	uint16_t channels, bits;
} wav_ctx_t;

typedef struct __attribute__((packed)) {
	int8_t riffmagic[4];
	uint32_t size;
	int8_t wavemagic[4];
} riff_hdr_t;

typedef struct __attribute__((packed)) {
	uint16_t fmtcode;
	uint16_t channels;
	uint32_t samplespersec;
	uint32_t avgbytespersec;
	uint16_t blockalign;
	uint16_t bitspersample;
	uint16_t bsize;
	uint16_t validbitspersample;
	uint32_t channelmask;
	int8_t subformat[16];
} fmt_data_t;

typedef struct __attribute__((packed)) {
	int8_t magic[4];
	int32_t size;
	union {
		fmt_data_t fmt;
		int8_t data[0];
	};
} chunk_hdr_t;



int wav_init_source(const void *data_start, const void *data_end, int req_sample_rate, void **ctx) {
	//Check sanity first
	char *p=(char*)data_start;
	wav_ctx_t *wav=calloc(sizeof(wav_ctx_t), 1);
	if (!wav) goto err;
	riff_hdr_t *riff=(riff_hdr_t*)p;
	if (memcmp(riff->riffmagic, "RIFF", 4)!=0) return -1;
	if (memcmp(riff->wavemagic, "WAVE", 4)!=0) return -1;
	p+=sizeof(riff_hdr_t);
	while (p<(char*)data_end) {
		chunk_hdr_t *ch=(chunk_hdr_t*)p;
		if (memcmp(ch->magic, "fmt ", 4)==0) {
			if (ch->fmt.fmtcode != 0x0001) {
				printf("Unsupported wav format: %d\n", ch->fmt.fmtcode);
				goto err;
			}
			wav->rate=ch->fmt.samplespersec;
			wav->bits=ch->fmt.bitspersample;
			wav->channels=ch->fmt.channels;
			if (wav->channels==0) wav->channels=1;
		} else if (memcmp(ch->magic, "data", 4)==0) {
			wav->data_len=ch->size;
			wav->data=ch->data;
		}
		p+=8+ch->size;
		if (ch->size&1) p++; //pad to even address
	}
	
	if (wav->bits!=8 && wav->bits!=16) {
		printf("No fmt chunk or unsupported bits/sample: %d\n", wav->bits);
		goto err;
	}
	printf("Wav: %d bit/sample, %d Hz, %d bytes long\n", wav->bits, wav->rate, wav->data_len);
	wav->pos=0;
	*ctx=(void*)wav;
	return CHUNK_SIZE;
err:
	free(wav);
	return -1;
}

int wav_get_sample_rate(void *ctx) {
	wav_ctx_t *wav=(wav_ctx_t*)ctx;
	return wav->rate;
}

int wav_fill_buffer(void *ctx, int8_t *buffer) {
	wav_ctx_t *wav=(wav_ctx_t*)ctx;
	for (int i=0; i<CHUNK_SIZE; i++) {
		if (wav->pos >= wav->data_len) return i;
		if (wav->bits==8) {
			buffer[i]=((uint8_t)wav->data[wav->pos])-128;
			wav->pos+=1;
		} else {
			buffer[i]=wav->data[wav->pos];
			wav->pos+=2;
		}
		if (wav->channels!=1) wav->pos+=(wav->channels-1)*(wav->bits/8);
	}
	return CHUNK_SIZE;
}

void wav_deinit_source(void *ctx) {
	wav_ctx_t *wav=(wav_ctx_t*)ctx;
	free(wav);
}

const sndmixer_source_t sndmixer_source_wav={
	.init_source=wav_init_source,
	.get_sample_rate=wav_get_sample_rate,
	.fill_buffer=wav_fill_buffer,
	.deinit_source=wav_deinit_source
};
